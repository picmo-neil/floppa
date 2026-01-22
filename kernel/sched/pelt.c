#include <linux/sched.h>
#include "sched.h"
#include "pelt.h"
#include "sched-pelt.h"

void cfs_se_util_change(struct sched_avg *avg)
{
	unsigned int enqueued;

	if (!sched_feat(UTIL_EST))
		return;

	enqueued = avg->util_est.enqueued;
	if (!(enqueued & UTIL_AVG_UNCHANGED))
		return;

	enqueued &= ~UTIL_AVG_UNCHANGED;
	WRITE_ONCE(avg->util_est.enqueued, enqueued);
}

static u64 decay_load(u64 val, u64 n)
{
	unsigned int local_n;

	if (unlikely(n > LOAD_AVG_PERIOD * 63))
		return 0;

	local_n = n;

	if (unlikely(local_n >= LOAD_AVG_PERIOD)) {
		val >>= local_n / LOAD_AVG_PERIOD;
		local_n %= LOAD_AVG_PERIOD;
	}

	val = mul_u64_u32_shr(val, runnable_avg_yN_inv[local_n], 32);
	return val;
}

static u32 __accumulate_pelt_segments(u64 periods, u32 d1, u32 d3)
{
	u32 c1, c2, c3 = d3;
	c1 = decay_load((u64)d1, periods);
	c2 = LOAD_AVG_MAX - decay_load(LOAD_AVG_MAX, periods) - 1024;
	return c1 + c2 + c3;
}

static __always_inline u32
accumulate_sum(u64 delta, struct sched_avg *sa,
	       unsigned long load, unsigned long runnable, int running)
{
	u32 contrib = (u32)delta;
	u64 periods;

	delta += sa->period_contrib;
	periods = delta / 1024;

	if (periods) {
		sa->load_sum = decay_load(sa->load_sum, periods);
		sa->runnable_sum = decay_load(sa->runnable_sum, periods);
		sa->util_sum = decay_load((u64)(sa->util_sum), periods);

		delta %= 1024;
		if (load) {
			contrib = __accumulate_pelt_segments(periods,
					1024 - sa->period_contrib, delta);
		}
	}
	sa->period_contrib = delta;

	if (load)
		sa->load_sum += load * contrib;
	if (runnable)
		sa->runnable_sum += runnable * contrib << SCHED_CAPACITY_SHIFT;
	if (running)
		sa->util_sum += contrib << SCHED_CAPACITY_SHIFT;

	return periods;
}

static __always_inline int
___update_load_sum(u64 now, struct sched_avg *sa,
		  unsigned long load, unsigned long runnable, int running)
{
	u64 delta;

	delta = now - sa->last_update_time;
	if ((s64)delta < 0) {
		sa->last_update_time = now;
		return 0;
	}

	delta >>= 10;
	if (!delta)
		return 0;

	sa->last_update_time += delta << 10;

	if (!load)
		runnable = running = 0;

	if (!accumulate_sum(delta, sa, load, runnable, running))
		return 0;

	return 1;
}

static __always_inline void
___update_load_avg(struct sched_avg *sa, unsigned long load)
{
	u32 divider = get_pelt_divider(sa);

	sa->load_avg = div_u64(load * sa->load_sum, divider);
	sa->runnable_avg = div_u64(sa->runnable_sum, divider);
	WRITE_ONCE(sa->util_avg, sa->util_sum / divider);
}

int __update_load_avg_blocked_se(u64 now, struct sched_entity *se)
{
	if (___update_load_sum(now, &se->avg, 0, 0, 0)) {
		___update_load_avg(&se->avg, scale_load_down(se->load.weight));
		return 1;
	}
	return 0;
}

int __update_load_avg_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (___update_load_sum(now, &se->avg, !!se->on_rq, !!se->on_rq,
				cfs_rq->curr == se)) {
		___update_load_avg(&se->avg, scale_load_down(se->load.weight));
		cfs_se_util_change(&se->avg);
		return 1;
	}
	return 0;
}

int __update_load_avg_cfs_rq(u64 now, struct cfs_rq *cfs_rq)
{
	if (___update_load_sum(now, &cfs_rq->avg,
				scale_load_down(cfs_rq->load.weight),
				cfs_rq->h_nr_running,
				cfs_rq->curr != NULL)) {
		___update_load_avg(&cfs_rq->avg, 1);
		return 1;
	}
	return 0;
}

int update_rt_rq_load_avg(u64 now, struct rq *rq, int running)
{
	if (___update_load_sum(now, &rq->avg_rt, running, running, running)) {
		___update_load_avg(&rq->avg_rt, 1);
		return 1;
	}
	return 0;
}

int update_dl_rq_load_avg(u64 now, struct rq *rq, int running)
{
	if (___update_load_sum(now, &rq->avg_dl, running, running, running)) {
		___update_load_avg(&rq->avg_dl, 1);
		return 1;
	}
	return 0;
}

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
int update_irq_load_avg(struct rq *rq, u64 running)
{
	int ret = 0;
	if (running > rq->clock)
        running = 0;
	ret = ___update_load_sum(rq->clock - running, &rq->avg_irq, 0, 0, 0);
	ret += ___update_load_sum(rq->clock, &rq->avg_irq, 1, 1, 1);
	if (ret)
		___update_load_avg(&rq->avg_irq, 1);
	return ret;
}
#endif
unsigned long effective_cpu_util(int cpu, unsigned long util_cfs,
				 unsigned long *pmin, unsigned long *pmax)
{
	unsigned long util, irq, capacity;
	struct rq *rq = cpu_rq(cpu);

	capacity = capacity_orig_of(cpu);

	/* INTEGRATED IRQ PRESSURE */
	irq = cpu_util_irq(rq);

	/* BASE UTILIZATION 
	 * Combine CFS (normal), RT, and DL (deadline) tasks.
	 */
	util = util_cfs;
	util += cpu_util_rt_rq(rq); 
	util += cpu_util_dl_rq(rq); 

	/* IRQ COMPENSATION */
	util = (util * capacity) / max(1UL, capacity - irq);

	/* UCLAMP SUPPORT */
	if (pmin) *pmin = uclamp_rq_get(rq, UCLAMP_MIN);
	if (pmax) *pmax = uclamp_rq_get(rq, UCLAMP_MAX);

	return min(util, capacity);
}


void init_sched_avg(struct sched_avg *sa)
{
	memset(sa, 0, sizeof(*sa));
	
}

/* Tracepoint Stubs (Prevent Linker Errors) */
#ifndef trace_pelt_cfs_tp
#define trace_pelt_cfs_tp(cfs_rq) do { } while (0)
#endif
#ifndef trace_pelt_se_tp
#define trace_pelt_se_tp(se) do { } while (0)
#endif
#ifndef trace_pelt_dl_tp
#define trace_pelt_dl_tp(rq) do { } while (0)
#endif
#ifndef trace_pelt_rt_tp
#define trace_pelt_rt_tp(rq) do { } while (0)
#endif
#ifndef trace_pelt_irq_tp
#define trace_pelt_irq_tp(rq) do { } while (0)
#endif
#ifndef trace_sched_util_est_cfs_tp
#define trace_sched_util_est_cfs_tp(cfs_rq) do { } while (0)
#endif
#ifndef trace_sched_util_est_se_tp
#define trace_sched_util_est_se_tp(se) do { } while (0)
#endif
