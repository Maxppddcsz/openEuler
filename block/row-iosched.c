#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/blktrace_api.h>
#include <linux/jiffies.h>
#include <linux/sbitmap.h>
#include <linux/blk-mq.h>

enum row_queue_prio {
	ROWQ_PRIO_HIGH_READ = 0,
	ROWQ_PRIO_REG_READ,
	ROWQ_PRIO_HIGH_SWRITE,
	ROWQ_PRIO_REG_SWRITE,
	ROWQ_PRIO_REG_WRITE,
	ROWQ_PRIO_LOW_READ,
	ROWQ_PRIO_LOW_SWRITE,
	ROWQ_MAX_PRIO,
};

/* whether idling is enabled on the queue */
static const bool queue_idling_enabled[] = {
	true,	/* ROWQ_PRIO_HIGH_READ */
	true,	/* ROWQ_PRIO_REG_READ */
	false,	/* ROWQ_PRIO_HIGH_SWRITE */
	false,	/* ROWQ_PRIO_REG_SWRITE */
	false,	/* ROWQ_PRIO_REG_WRITE */
	false,	/* ROWQ_PRIO_LOW_READ */
	false,	/* ROWQ_PRIO_LOW_SWRITE */
};

/* Default values for row queues quantums in each dispatch cycle */
static const int queue_quantum[] = {
	100,	/* ROWQ_PRIO_HIGH_READ */
	100,	/* ROWQ_PRIO_REG_READ */
	2,	/* ROWQ_PRIO_HIGH_SWRITE */
	1,	/* ROWQ_PRIO_REG_SWRITE */
	1,	/* ROWQ_PRIO_REG_WRITE */
	1,	/* ROWQ_PRIO_LOW_READ */
	1	/* ROWQ_PRIO_LOW_SWRITE */
};

/* Default values for idling on read queues */
#define ROW_IDLE_TIME 50	/* 5 msec */
#define ROW_READ_FREQ 70	/* 7 msec */


struct rowq_idling_data {
	unsigned long		idle_trigger_time;  
	bool			begin_idling;  
};

struct row_queue {
	struct row_data		*rdata;
	struct list_head	fifo;
	enum row_queue_prio	prio;

	unsigned int		nr_dispatched;
	unsigned int		slice;

	/* used only for READ queues */
	struct rowq_idling_data	idle_data;
};

struct idling_data {
	unsigned long		idle_time; 
	unsigned long		freq; 

	struct delayed_work	idle_work; 
};


struct row_data {
	struct list_head dispatch_queue;

	struct {
		struct row_queue	rqueue;
		int			disp_quantum;
	} row_queues[ROWQ_MAX_PRIO]; 

	enum row_queue_prio		curr_queue; 

	struct idling_data		read_idle; 
	unsigned int			nr_reqs[2]; //0-read,1-write

	unsigned int			cycle_flags;

	spinlock_t lock;
};

#define RQ_ROWQ(rq) ((struct row_queue *) ((rq)->elv.priv[0]))


static inline void row_mark_rowq_unserved(struct row_data *rd, enum row_queue_prio qnum) {
	rd->cycle_flags |= (1 << qnum);
}

static inline void row_clear_rowq_unserved(struct row_data *rd, enum row_queue_prio qnum) {
	rd->cycle_flags &= ~(1 << qnum);
}

static inline int row_rowq_unserved(struct row_data *rd, enum row_queue_prio qnum) {
	return rd->cycle_flags & (1 << qnum);
}


static void kick_queue(struct work_struct *work) {
	struct delayed_work *idle_work = to_delayed_work(work);
	struct idling_data *read_data =
		container_of(idle_work, struct idling_data, idle_work);
	struct row_data *rd =
		container_of(read_data, struct row_data, read_idle);

	/* Mark idling process as done */
	rd->row_queues[rd->curr_queue].rqueue.idle_data.begin_idling = false;

}

/*
 * row_restart_disp_cycle() - Restart the dispatch cycle
 * @rd:	pointer to struct row_data
 *
 * This function restarts the dispatch cycle by:
 * - Setting current queue to ROWQ_PRIO_HIGH_READ
 * - For each queue: reset the number of requests dispatched in
 *   the cycle
 */
static inline void row_restart_disp_cycle(struct row_data *rd) {
	int i;

	for (i = 0; i < ROWQ_MAX_PRIO; i++)
		rd->row_queues[i].rqueue.nr_dispatched = 0;

	rd->curr_queue = ROWQ_PRIO_HIGH_READ;
}

static inline void row_get_next_queue(struct row_data *rd) {
	rd->curr_queue++;
	if (rd->curr_queue == ROWQ_MAX_PRIO)
		row_restart_disp_cycle(rd);
}

/******************* Elevator callback functions *********************/

static void row_add_request(struct request_queue *q, struct request *rq) {
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;
	struct row_queue *rqueue = RQ_ROWQ(rq);

	list_add_tail(&rq->queuelist, &rqueue->fifo);
	rd->nr_reqs[rq_data_dir(rq)]++;
	

	if (queue_idling_enabled[rqueue->prio]) {
		if (delayed_work_pending(&rd->read_idle.idle_work))
			(void)cancel_delayed_work(
				&rd->read_idle.idle_work);
		if (time_before(jiffies, rqueue->idle_data.idle_trigger_time)) {
			rqueue->idle_data.begin_idling = true;
			
		} else
			rqueue->idle_data.begin_idling = false;

		rqueue->idle_data.idle_trigger_time =
			jiffies + msecs_to_jiffies(rd->read_idle.freq);
	}
	
}

static void row_add_requests(struct blk_mq_hw_ctx *hctx, struct list_head *list, bool at_head) {

    struct request_queue *q = hctx->queue;
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;

    spin_lock(&rd->lock);
    while (!list_empty(list)) {
        struct request *rq;

        rq = list_first_entry(list, struct request, queuelist); // 将request取出       
        list_del_init(&rq->queuelist);
        row_add_request(q, rq);
    }
    spin_unlock(&rd->lock);
}


static void row_remove_request(struct request_queue *q, struct request *rq) {
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;

	rq_fifo_clear(rq);
	list_del_init(&rq->queuelist);
	rd->nr_reqs[rq_data_dir(rq)]--;
	elv_rqhash_del(q, rq);
}


static struct request * row_dispatch_insert(struct row_data *rd) {
	
    struct request *rq;

	rq = rq_entry_fifo(rd->row_queues[rd->curr_queue].rqueue.fifo.next);
	row_remove_request(rq->q, rq);
	// elv_dispatch_add_tail(rd->dispatch_queue, rq);
	list_add_tail(&rq->queuelist, &rd->dispatch_queue);
	rd->row_queues[rd->curr_queue].rqueue.nr_dispatched++;
	row_clear_rowq_unserved(rd, rd->curr_queue);
	
    return rq;
}

static int row_choose_queue(struct row_data *rd) {
	int prev_curr_queue = rd->curr_queue;

	if (!(rd->nr_reqs[0] + rd->nr_reqs[1])) {
		
		return 0;
	}

	row_get_next_queue(rd);

	/*
	 * Loop over all queues to find the next queue that is not empty.
	 * Stop when you get back to curr_queue
	 */
	while (list_empty(&rd->row_queues[rd->curr_queue].rqueue.fifo)
	       && rd->curr_queue != prev_curr_queue) {
		/* Mark rqueue as unserved */
		row_mark_rowq_unserved(rd, rd->curr_queue);
		row_get_next_queue(rd);
	}

	return 1;
}

static struct request *row_dispatch_requests(struct blk_mq_hw_ctx *hctx) {

    struct request_queue *q = hctx->queue;
	struct row_data *rd = (struct row_data *)q->elevator->elevator_data;
	int currq, i;
    struct request *rq = NULL;

	currq = rd->curr_queue;

	/*
	 * Find the first unserved queue (with higher priority then currq)
	 * that is not empty
	 */
	for (i = 0; i < currq; i++) {
		if (row_rowq_unserved(rd, i) &&
		    !list_empty(&rd->row_queues[i].rqueue.fifo)) {
			
			rd->curr_queue = i;
			rq = row_dispatch_insert(rd);
			goto done;
		}
	}

	if (rd->row_queues[currq].rqueue.nr_dispatched >=
	    rd->row_queues[currq].disp_quantum) {
		rd->row_queues[currq].rqueue.nr_dispatched = 0;
	
		if (row_choose_queue(rd))
			rq = row_dispatch_insert(rd);
		goto done;
	}

	/* Dispatch from curr_queue */
	if (list_empty(&rd->row_queues[currq].rqueue.fifo)) {
		/* check idling */
		if (delayed_work_pending(&rd->read_idle.idle_work)) {
			
			goto done;
		}

		if (queue_idling_enabled[currq] &&
		    rd->row_queues[currq].rqueue.idle_data.begin_idling) {
			if (! schedule_delayed_work(&rd->read_idle.idle_work, jiffies +
			     msecs_to_jiffies(rd->read_idle.idle_time))) {
				
				pr_err("ROW_BUG: Work already on queue!");
			} else
				
			goto done;
		} else {
			
			if (!row_choose_queue(rd))
				goto done;
		}
	}

	rq = row_dispatch_insert(rd);

done:
	return rq;
}


static int row_init_queue(struct request_queue *q, struct elevator_type *e) {

	struct row_data *rdata;
	int i;
    struct elevator_queue *eq;

	rdata = kmalloc_node(sizeof(*rdata),
			     GFP_KERNEL | __GFP_ZERO, q->node);
	if (!rdata)
		return -ENOMEM;

    eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;
    eq->elevator_data = rdata; // 关联row_data和elevator_queue


	for (i = 0; i < ROWQ_MAX_PRIO; i++) {
		INIT_LIST_HEAD(&rdata->row_queues[i].rqueue.fifo);
		rdata->row_queues[i].disp_quantum = queue_quantum[i];
		rdata->row_queues[i].rqueue.rdata = rdata;
		rdata->row_queues[i].rqueue.prio = i;
		rdata->row_queues[i].rqueue.idle_data.begin_idling = false;
	}

	/*
	 * Currently idling is enabled only for READ queues. If we want to
	 * enable it for write queues also, note that idling frequency will
	 * be the same in both cases
	 */
	rdata->read_idle.idle_time = ROW_IDLE_TIME;
	rdata->read_idle.freq = ROW_READ_FREQ;
	INIT_DELAYED_WORK(&rdata->read_idle.idle_work, kick_queue);

	rdata->curr_queue = ROWQ_PRIO_HIGH_READ;
	//rdata->dispatch_queue = q;
	INIT_LIST_HEAD(&rdata->dispatch_queue);

    spin_lock_init(&rdata->lock);

	rdata->nr_reqs[READ] = rdata->nr_reqs[WRITE] = 0;

    q->elevator = eq; // 关联elevator_queue和request_queue

	return 0;
}

static void row_exit_queue(struct elevator_queue *e) {
	struct row_data *rd = (struct row_data *)e->elevator_data;
	int i;

	for (i = 0; i < ROWQ_MAX_PRIO; i++)
		BUG_ON(!list_empty(&rd->row_queues[i].rqueue.fifo));
	(void)cancel_delayed_work_sync(&rd->read_idle.idle_work);
	kfree(rd);
}

static void row_merged_requests(struct request_queue *q, struct request *rq, struct request *next) {
	
    struct row_queue   *rqueue = RQ_ROWQ(next);

	list_del_init(&next->queuelist);

	rqueue->rdata->nr_reqs[rq_data_dir(rq)]--;
}


static enum row_queue_prio get_queue_type(struct request *rq) {
	const int data_dir = rq_data_dir(rq);
	const bool is_sync = rq_is_sync(rq);

	if (data_dir == READ)
		return ROWQ_PRIO_REG_READ;
	else if (is_sync)
		return ROWQ_PRIO_REG_SWRITE;
	else
		return ROWQ_PRIO_REG_WRITE;
}

static void row_set_request(struct request *rq) {

    struct row_queue *q = RQ_ROWQ(rq);
	struct row_data *rd = (struct row_data *)q->rdata;
	
	rq->elv.priv[0] =
		(void *)(&rd->row_queues[get_queue_type(rq)]);

}

/********** Helping sysfs functions/defenitions for ROW attributes ******/
static ssize_t row_var_show(int var, char *page) {
	return snprintf(page, 100, "%d\n", var);
}

static ssize_t row_var_store(int *var, const char *page, size_t count) {
	int err;
	err = kstrtoul(page, 10, (unsigned long *)var);

	return count;
}

#define SHOW_FUNCTION(__FUNC, __VAR, __CONV)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct row_data *rowd = e->elevator_data;			\
	int __data = __VAR;						\
	if (__CONV)							\
		__data = jiffies_to_msecs(__data);			\
	return row_var_show(__data, (page));			\
}
SHOW_FUNCTION(row_hp_read_quantum_show,
	rowd->row_queues[ROWQ_PRIO_HIGH_READ].disp_quantum, 0);
SHOW_FUNCTION(row_rp_read_quantum_show,
	rowd->row_queues[ROWQ_PRIO_REG_READ].disp_quantum, 0);
SHOW_FUNCTION(row_hp_swrite_quantum_show,
	rowd->row_queues[ROWQ_PRIO_HIGH_SWRITE].disp_quantum, 0);
SHOW_FUNCTION(row_rp_swrite_quantum_show,
	rowd->row_queues[ROWQ_PRIO_REG_SWRITE].disp_quantum, 0);
SHOW_FUNCTION(row_rp_write_quantum_show,
	rowd->row_queues[ROWQ_PRIO_REG_WRITE].disp_quantum, 0);
SHOW_FUNCTION(row_lp_read_quantum_show,
	rowd->row_queues[ROWQ_PRIO_LOW_READ].disp_quantum, 0);
SHOW_FUNCTION(row_lp_swrite_quantum_show,
	rowd->row_queues[ROWQ_PRIO_LOW_SWRITE].disp_quantum, 0);
SHOW_FUNCTION(row_read_idle_show, rowd->read_idle.idle_time, 1);
SHOW_FUNCTION(row_read_idle_freq_show, rowd->read_idle.freq, 1);
#undef SHOW_FUNCTION

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t __FUNC(struct elevator_queue *e,				\
		const char *page, size_t count)				\
{									\
	struct row_data *rowd = e->elevator_data;			\
	int __data;						\
	int ret = row_var_store(&__data, (page), count);		\
	if (__CONV)							\
		__data = (int)msecs_to_jiffies(__data);			\
	if (__data < (MIN))						\
		__data = (MIN);						\
	else if (__data > (MAX))					\
		__data = (MAX);						\
	*(__PTR) = __data;						\
	return ret;							\
}
STORE_FUNCTION(row_hp_read_quantum_store,
		&rowd->row_queues[ROWQ_PRIO_HIGH_READ].disp_quantum, 0,
		INT_MAX, 0);
STORE_FUNCTION(row_rp_read_quantum_store,
		&rowd->row_queues[ROWQ_PRIO_REG_READ].disp_quantum, 0,
		INT_MAX, 0);
STORE_FUNCTION(row_hp_swrite_quantum_store,
		&rowd->row_queues[ROWQ_PRIO_HIGH_SWRITE].disp_quantum, 0,
		INT_MAX, 0);
STORE_FUNCTION(row_rp_swrite_quantum_store,
		&rowd->row_queues[ROWQ_PRIO_REG_SWRITE].disp_quantum, 0,
		INT_MAX, 0);
STORE_FUNCTION(row_rp_write_quantum_store,
		&rowd->row_queues[ROWQ_PRIO_REG_WRITE].disp_quantum, 0,
		INT_MAX, 0);
STORE_FUNCTION(row_lp_read_quantum_store,
		&rowd->row_queues[ROWQ_PRIO_LOW_READ].disp_quantum, 0,
		INT_MAX, 0);
STORE_FUNCTION(row_lp_swrite_quantum_store,
		&rowd->row_queues[ROWQ_PRIO_LOW_SWRITE].disp_quantum, 0,
		INT_MAX, 1);
STORE_FUNCTION(row_read_idle_store, &rowd->read_idle.idle_time, 1, INT_MAX, 1);
STORE_FUNCTION(row_read_idle_freq_store, &rowd->read_idle.freq,
				1, INT_MAX, 1);

#undef STORE_FUNCTION

#define ROW_ATTR(name) \
	__ATTR(name, S_IRUGO|S_IWUSR, row_##name##_show, \
				      row_##name##_store)

static struct elv_fs_entry row_attrs[] = {
	ROW_ATTR(hp_read_quantum),
	ROW_ATTR(rp_read_quantum),
	ROW_ATTR(hp_swrite_quantum),
	ROW_ATTR(rp_swrite_quantum),
	ROW_ATTR(rp_write_quantum),
	ROW_ATTR(lp_read_quantum),
	ROW_ATTR(lp_swrite_quantum),
	ROW_ATTR(read_idle),
	ROW_ATTR(read_idle_freq),
	__ATTR_NULL
};

static struct elevator_type iosched_row = {
	.ops = {
		.requests_merged	=   row_merged_requests,
		.dispatch_request	=   row_dispatch_requests,
		.insert_requests    =   row_add_requests,
		.former_request	    =   elv_rb_former_request,
		.next_request       =   elv_rb_latter_request,
		.prepare_request	=   row_set_request,
		.init_sched		    =   row_init_queue,
		.exit_sched		    =   row_exit_queue,
	},
	.elevator_attrs = row_attrs,
	.elevator_name = "row",
	.elevator_owner = THIS_MODULE,
};

static int __init row_init(void)
{
	elv_register(&iosched_row);
	return 0;
}

static void __exit row_exit(void)
{
	elv_unregister(&iosched_row);
}

module_init(row_init);
module_exit(row_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Read Over Write IO scheduler");
