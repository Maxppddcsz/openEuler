#ifndef OSSL_USER_H
#define OSSL_USER_H

#ifndef __USER__
#define __USER__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h> // ftok
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>

#include "ossl_types.h"
#ifdef __LINUX__
#include "./../linux/user/ossl_user_linux.h"
#endif

#include <ossl_ctype_ex.h>
#include "ossl_user_linux.h"

/* hinicadm send msg to driver */
typedef struct {
    volatile int counter;
} atomic_t;

struct semaphore {
    int count;
};

union semun {
    int val;               // value for SETVAL
    struct semid_ds *buf;  // buffer for IPC_STAT & IPC_SET
    unsigned short *array; // buffer for GETALL & SELALL
    struct seminfo *__buf; // buffer for IPC_INFO
};

#ifndef CHIP_DEV_NAME
#define CHIP_DEV_NAME "device target, e.g. hinic0, eth0"
#define DEV_NAME "device target, e.g. eth0"
#endif

#define STD_INPUT_LEN 1

#define OP_LOG_USER_NAME_SIZE 32
#define OP_LOG_IP_STR_SIZE 256

#define HIADM3_FTOK_PATH "/dev/"
#define HIADM3_FTOK_ID_TOOLS 10
#define HIADM3_SHARE_MEM_PATH "/dev/"
#define HIADM3_SHARE_MEM_ID_TOOLS 100
#define SEM_SUCCESS 0
#define SEM_ERROR (-1)
#define SEM_OP_RESET_MODE 1
#define SEM_SOURCE_ACCESS_ID 0
#define SEM_INDIR_INTERFACE_ID 1
#define SEM_SHM_SYNC_ID 2
#define SEM_CMD_SYNC_ID 3
#define SEM_CREATE_NUM 4
#define SHM_SUCCESS 0
#define SHM_ERROR (-1)

#define LOCKF_LOCK_ENTIRE_FILE 0

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

extern uda_status io_ctl(int fd, void *inBuf, long inBufLen);
extern void uda_get_username_ip(char *user, char *ip, u32 len);
int create_sems(int module_id, int nums);
int get_sems(int module_id);
int init_sems(int sem_id, int which, int value);
int destroy_sems(int sem_id);
int sem_p(int sem_id, int which, int flg);
int sem_v(int sem_id, int which, int flg);
int create_shm(int module_id, int size);
int get_shm(int module_id, int size);
int get_shm_addr(int shm_id, const void *shm_addr, int shmflg, void **dste_addr);
int destroy_shm(int shm_id);

#endif
