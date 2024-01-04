#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include "internal.h"

static char *buf_dirty;	/* buffer to store number of dirty pages */
static unsigned long buf_size;		/* size of buffer in bytes */
static unsigned long buff_num;		/* size of buffer in number of pages */
static unsigned long buff_limit;	/* filter threshold of dirty pages*/

static struct proc_dir_entry *dirty_dir;

unsigned long dirty_page_open;

/* proc root directory */
#define DIRTY_ROOT "dirty"
/* proc file for buffer allocation and release */
#define DIRTY_SWITCH "buffer_size"
/* proc file to obtain diry pages of each inode */
#define DIRTY_PAGES "dirty_list"
/* proc file to filter result */
#define DIRTY_LIMIT "page_threshold"

#define MAX_BUFF_SIZE 102400

static unsigned long dump_dirtypages_inode(struct inode *inode)
{
	XA_STATE(xas, &inode->i_mapping->i_pages, 0);
	unsigned long nr_dirtys = 0;
	void *page;

	xas_lock_irq(&xas);
	xas_for_each_marked(&xas, page, (pgoff_t)-1, PAGECACHE_TAG_DIRTY) {
		if (++nr_dirtys % XA_CHECK_SCHED)
			continue;

		xas_pause(&xas);
		xas_unlock_irq(&xas);
		cond_resched();
		xas_lock_irq(&xas);
	}
	xas_unlock_irq(&xas);

	return nr_dirtys;
}

static char *inode_filename(struct inode *inode, char *tmpname)
{
	struct dentry *dentry;
	char *filename;

	dentry = d_find_alias(inode);
	if (!dentry)
		return ERR_PTR(-ENOENT);

	tmpname[PATH_MAX-1] = '\0';
	filename = dentry_path_raw(dentry, tmpname, PATH_MAX);

	dput(dentry);

	return filename;
}

static inline bool is_sb_writable(struct super_block *sb)
{
	if (sb_rdonly(sb))
		return false;

	if (sb->s_writers.frozen == SB_FREEZE_COMPLETE)
		return false;

	return true;
}

/*
 * dump_dirtypages_sb - dump the dirty pages of each inode in the sb
 * @sb the super block
 * @m the seq_file witch is initialized in proc_dpages_open
 *
 * For each inode in the sb, call dump_dirtypages_pages to get the number
 * of dirty pages. And use seq_printf to store the result in the buffer
 * if it's not less than the threshold. The inode in unusual state will
 * be skipped.
 */
static void dump_dirtypages_sb(struct super_block *sb, struct seq_file *m)
{
	struct inode *inode, *toput_inode = NULL;
	unsigned long nr_dirtys;
	const char *fstype;
	char *filename;
	char *tmpname;
	unsigned long limit = READ_ONCE(buff_limit);

	if (!is_sb_writable(sb) || (sb->s_iflags & SB_I_NODEV))
		return;

	tmpname = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!tmpname)
		return;

	spin_lock(&sb->s_inode_list_lock);
	list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
		spin_lock(&inode->i_lock);

		/*
		 * We must skip inodes in unusual state. We may also skip
		 * inodes without pages but we deliberately won't in case
		 * we need to reschedule to avoid softlockups.
		 */
		if ((inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)) ||
		    (inode->i_mapping->nrpages == 0 && !need_resched()) ||
			!mapping_tagged(inode->i_mapping, PAGECACHE_TAG_DIRTY)) {
			spin_unlock(&inode->i_lock);
			continue;
		}
		__iget(inode);
		spin_unlock(&inode->i_lock);
		spin_unlock(&sb->s_inode_list_lock);

		cond_resched();

		nr_dirtys = dump_dirtypages_inode(inode);
		if (!nr_dirtys || nr_dirtys < limit)
			goto skip;

		filename = inode_filename(inode, tmpname);
		if (IS_ERR_OR_NULL(filename))
			filename = "unknown";

		if (sb->s_type && sb->s_type->name)
			fstype = sb->s_type->name;
		else
			fstype = "unknown";

		seq_printf(m, "FSType: %s, Dev ID: %u(%u:%u) ino %lu, dirty pages %lu, path %s\n",
			fstype, sb->s_dev, MAJOR(sb->s_dev),
			MINOR(sb->s_dev), inode->i_ino,
			nr_dirtys, filename);

		if (seq_has_overflowed(m)) {
			m->size += 13;		/* keep size > count to avoid overflow in seq_read_iter() */
			strncpy(m->buf + m->count - 12, "terminated\n\0", 12);
			m->count += 12;
			iput(inode);
			goto done;
		}
skip:
		iput(toput_inode);
		toput_inode = inode;
		spin_lock(&sb->s_inode_list_lock);
	}
	spin_unlock(&sb->s_inode_list_lock);
done:
	iput(toput_inode);
	kfree(tmpname);
}

static int proc_dpages_show(struct seq_file *m, void *v)
{
	iterate_supers((void *)dump_dirtypages_sb, (void *)m);
	return 0;
}

static void free_buf_dirty(void)
{
	if (buf_dirty != NULL) {
		vfree(buf_dirty);
		buf_dirty = NULL;
		buf_size = 0;
	}
}

static ssize_t write_proc(
	struct file *filp,
	const char *buf,
	size_t count,
	loff_t *offp)
{
	int ret = 0;
	unsigned long old_buff_num;

	if (count > PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	old_buff_num = buff_num;
	ret = kstrtoul_from_user(buf, count, 10, &buff_num);
	if (ret != 0 || buff_num > MAX_BUFF_SIZE) {
		buff_num = old_buff_num;
		ret = -EINVAL;
		goto out;
	}

	ret = count;
	if (buff_num == 0) {
		free_buf_dirty();
		goto out;
	}
	if (buff_num == old_buff_num)
		goto out;

	free_buf_dirty();
	buf_size = PAGE_SIZE * buff_num;
	buf_dirty = vzalloc(buf_size);
	if (!buf_dirty)
		ret = -ENOMEM;

out:
	return ret;
}

static int proc_dpages_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct seq_file *m;

	if (xchg(&dirty_page_open, 1) == 1)
		return -EBUSY;

	if (buf_dirty == NULL || buf_size == 0) {
		pr_warn("please allocate buffer before getting dirty pages\n");
		dirty_page_open = 0;
		return -ENOMEM;
	}

	ret = single_open(filp, proc_dpages_show, NULL);
	if (!ret) {
		dirty_page_open = 0;
		return ret;
	}

	m = (struct seq_file *)filp->private_data;
	memset(buf_dirty, 0, buf_size);
	/* if seq_has_overflowed() return true, it need to contain "terminated\n\0" */
	m->size = buf_size - 13;
	m->buf = buf_dirty;

	return ret;
}

static int seq_release_dirty(struct inode *inode, struct file *file)
{
	struct seq_file *m = file->private_data;

	/* we don't want to free the buf */
	m->buf = NULL;
	single_release(inode, file);
	dirty_page_open = 0;
	return 0;
}

static const struct proc_ops proc_dpages_operations = {
	.proc_open           = proc_dpages_open,
	.proc_read           = seq_read,
	.proc_release        = seq_release_dirty,
};

static int proc_switch_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lu\n", buff_num);
	return 0;
}

static int proc_limit_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lu\n", READ_ONCE(buff_limit));
	return 0;
}

static int proc_switch_open(struct inode *inode, struct file *filp)
{
	int ret;

	if (xchg(&dirty_page_open, 1) == 1)
		return -EBUSY;

	ret = single_open(filp, proc_switch_show, NULL);
	if (!ret)
		dirty_page_open = 0;

	return ret;
}

static int proc_limit_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, proc_limit_show, NULL);
}

static int proc_switch_release(struct inode *inode, struct file *filp) {
	dirty_page_open = 0;
	return single_release(inode, filp);
}

static ssize_t write_limit_proc(
	struct file *filp,
	const char *buf,
	size_t count,
	loff_t *offp)
{
	int ret = 0;
	unsigned long tmp;

	if (count > PAGE_SIZE)
		return -EINVAL;

	ret = kstrtoul_from_user(buf, count, 10, &tmp);
	if (ret != 0)
		return -EINVAL;

	WRITE_ONCE(buff_limit, tmp);

	return count;
}


static const struct proc_ops proc_switch_operations = {
	.proc_open           = proc_switch_open,
	.proc_read           = seq_read,
	.proc_write          = write_proc,
	.proc_lseek          = seq_lseek,
	.proc_release        = proc_switch_release,
};

static const struct proc_ops proc_limit_operations = {
	.proc_open           = proc_limit_open,
	.proc_read           = seq_read,
	.proc_write          = write_limit_proc,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};


static int __init dpages_proc_init(void)
{
	static struct proc_dir_entry *proc_file;

	dirty_dir = proc_mkdir(DIRTY_ROOT, NULL);
	if (!dirty_dir)
		goto fail_dir;

	proc_file = proc_create(DIRTY_PAGES, 0440,
					dirty_dir, &proc_dpages_operations);
	if (!proc_file)
		goto fail_pages;

	proc_file = proc_create(DIRTY_SWITCH, 0640,
					dirty_dir, &proc_switch_operations);
	if (!proc_file)
		goto fail_switch;

	proc_file = proc_create(DIRTY_LIMIT, 0640,
					dirty_dir, &proc_limit_operations);
	if (!proc_file)
		goto fail_limit;

	return 0;

fail_limit:
	remove_proc_entry(DIRTY_SWITCH, dirty_dir);
fail_switch:
	remove_proc_entry(DIRTY_PAGES, dirty_dir);
fail_pages:
	remove_proc_entry(DIRTY_ROOT, NULL);
fail_dir:
	return -ENOMEM;
}

subsys_initcall(dpages_proc_init);
