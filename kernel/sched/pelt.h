#ifndef _KERNEL_SCHED_PELT_H
#define _KERNEL_SCHED_PELT_H

#include <linux/types.h>
struct rq;
struct sched_entity;
struct cfs_rq;

int __update_load_avg_blocked_se(u64 now, struct sched_entity *se);
int __update_load_avg_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se);
int __update_load_avg_cfs_rq(u64 now, struct cfs_rq *cfs_rq);
int update_rt_rq_load_avg(u64 now, struct rq *rq, int running);
int update_dl_rq_load_avg(u64 now, struct rq *rq, int running);

void cfs_se_util_change(struct sched_avg *avg);

#endif /* _KERNEL_SCHED_PELT_H */
