#include <linux/sched.h>
#include "sched.h"
#include "pelt.h"
#include "sched-pelt.h"

#ifndef entity_is_task
#define entity_is_task(se)  (!se->my_q)
#endif

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
accumulate_sum(u64 delta, int cpu, struct sched_avg *sa,
	       unsigned long weight, int running, struct cfs_rq *cfs_rq)
{
	unsigned long scale_cpu;
	u32 contrib = (u32)delta; /* p == 0 -> delta < 1024 */
	u64 periods;

	scale_cpu = arch_scale_cpu_capacity(NULL, cpu);

	delta += sa->period_contrib;
	periods = delta / 1024; /* A period is 1024us (~1ms) */

	/*
	 * Step 1: decay old *_sum if we crossed period boundaries.
	 */
	if (periods) {
		sa->load_sum = decay_load(sa->load_sum, periods);
		if (cfs_rq) {
			cfs_rq->runnable_sum =
				decay_load(cfs_rq->runnable_sum, periods);
		}
		sa->util_sum = decay_load((u64)(sa->util_sum), periods);

		/*
		 * Step 2
		 */
		delta %= 1024;
		contrib = __accumulate_pelt_segments(periods,
				1024 - sa->period_contrib, delta);
	}
	sa->period_contrib = delta;

	if (weight) {
		sa->load_sum += weight * contrib;
		if (cfs_rq)
			cfs_rq->runnable_sum += weight * contrib;
	}
	if (running)
		sa->util_sum += contrib * scale_cpu;

	return periods;
}
/*
 * We can represent the historical contribution to runnable average as the
 * coefficients of a geometric series.  To do this we sub-divide our runnable
 * history into segments of approximately 1ms (1024us); label the segment that
 * occurred N-ms ago p_N, with p_0 corresponding to the current period, e.g.
 *
 * [<- 1024us ->|<- 1024us ->|<- 1024us ->| ...
 *      p0            p1           p2
 *     (now)       (~1ms ago)  (~2ms ago)
 *
 * Let u_i denote the fraction of p_i that the entity was runnable.
 *
 * We then designate the fractions u_i as our co-efficients, yielding the
 * following representation of historical load:
 *   u_0 + u_1*y + u_2*y^2 + u_3*y^3 + ...
 *
 * We choose y based on the with of a reasonably scheduling period, fixing:
 *   y^32 = 0.5
 *
 * This means that the contribution to load ~32ms ago (u_32) will be weighted
 * approximately half as much as the contribution to load within the last ms
 * (u_0).
 *
 * When a period "rolls over" and we have new u_0`, multiplying the previous
 * sum again by y is sufficient to update:
 *   load_avg = u_0` + y*(u_0 + u_1*y + u_2*y^2 + ... )
 *            = u_0 + u_1*y + u_2*y^2 + ... [re-labeling u_i --> u_{i+1}]
 */
static __always_inline int
___update_load_sum(u64 now, struct sched_avg *sa,
		  unsigned long load, unsigned long runnable, int running)
{
	u64 delta;

	delta = now - sa->last_update_time;
	/*
	 * This should only happen when time goes backwards, which it
	 * unfortunately does during sched clock init when we swap over to TSC.
	 */
	if ((s64)delta < 0) {
		sa->last_update_time = now;
		return 0;
	}

	/*
	 * Use 1024ns as the unit of measurement since it's a reasonable
	 * approximation of 1us and fast to compute.
	 */
	delta >>= 10;
	if (!delta)
		return 0;

	sa->last_update_time += delta << 10;

	/*
	 * running is a subset of runnable (weight) so running can't be set if
	 * runnable is clear. But there are some corner cases where the current
	 * se has been already dequeued but cfs_rq->curr still points to it.
	 * This means that weight will be 0 but not running for a sched_entity
	 * but also for a cfs_rq if the latter becomes idle. As an example,
	 * this happens during idle_balance() which calls
	 * update_blocked_averages()
	 *
	 * Also see the comment in accumulate_sum().
	 */
	if (!load)
		runnable = running = 0;

	/*
	 * Now we know we crossed measurement unit boundaries. The *_avg
	 * accrues by two steps:
	 *
	 * Step 1: accumulate *_sum since last_update_time. If we haven't
	 * crossed period boundaries, finish.
	 */
	if (!accumulate_sum(delta, sa, load, runnable, running))
		return 0;

	return 1;
}

static __always_inline void
___update_load_avg(struct sched_avg *sa, unsigned long load, unsigned long runnable)
{
	u32 divider = get_pelt_divider(sa);

	sa->load_avg = div_u64(load * sa->load_sum, divider);
	sa->runnable_avg = div_u64(runnable * sa->runnable_sum, divider);
	WRITE_ONCE(sa->util_avg, sa->util_sum / divider);
}

int __update_load_avg_blocked_se(u64 now, struct sched_entity *se)
{
	if (entity_is_task(se))
		se->runnable_weight = se->load.weight;

	if (___update_load_sum(now, &se->avg, 0, 0, 0)) {
		___update_load_avg(&se->avg, scale_load_down(se->load.weight), se->runnable_weight);
		return 1;
	}

	return 0;
}

int __update_load_avg_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (entity_is_task(se))
		se->runnable_weight = se->load.weight;

	if (___update_load_sum(now, &se->avg, !!se->on_rq, !!se->on_rq,
				cfs_rq->curr == se)) {

		___update_load_avg(&se->avg, scale_load_down(se->load.weight), se->runnable_weight);
		cfs_se_util_change(&se->avg);
		return 1;
	}

	return 0;
}

int __update_load_avg_cfs_rq(u64 now, struct cfs_rq *cfs_rq)
{
	if (___update_load_sum(now, &cfs_rq->avg,
				scale_load_down(cfs_rq->load.weight),
				scale_load_down(cfs_rq->runnable_weight),
				cfs_rq->curr != NULL)) {

		___update_load_avg(&cfs_rq->avg, 1, 1);
	}

	return 0;
}

int update_rt_rq_load_avg(u64 now, struct rq *rq, int running)
{
	if (___update_load_sum(now, &rq->avg_rt,
				running,
				running,
				running)) {

		___update_load_avg(&rq->avg_rt, 1, 1);
		return 1;
	}

	return 0;
}

int update_dl_rq_load_avg(u64 now, struct rq *rq, int running)
{
	if (___update_load_sum(now, &rq->avg_dl,
				running,
				running,
				running)) {

		___update_load_avg(&rq->avg_dl, 1, 1);
		return 1;
	}

	return 0;
}

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
int update_irq_load_avg(struct rq *rq, u64 running)
{
	int ret = 0;
	/*
	 * We know the time that has been used by interrupt since last update
	 * but we don't when. Let be pessimistic and assume that interrupt has
	 * happened just before the update. This is not so far from reality
	 * because interrupt will most probably wake up task and trig an update
	 * of rq clock during which the metric si updated.
	 * We start to decay with normal context time and then we add the
	 * interrupt context time.
	 * We can safely remove running from rq->clock because
	 * rq->clock += delta with delta >= running
	 */
	ret = ___update_load_sum(rq->clock - running, &rq->avg_irq, 0, 0, 0);
	ret += ___update_load_sum(rq->clock, &rq->avg_irq, 1, 1, 1);

	if (ret)
		___update_load_avg(&rq->avg_irq, 1, 1);
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
