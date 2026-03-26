/**
 * C wrapper for NuttX clock_cpuload().
 *
 * NuttX defines 'struct cpuload_s' in nuttx/clock.h when
 * CONFIG_SCHED_CPULOAD is enabled. PX4 also defines 'struct cpuload_s'
 * as a uORB message. They collide in C++ translation units.
 *
 * This file compiles as pure C with only NuttX headers, avoiding the
 * collision. LoadMon.cpp calls this wrapper instead of clock_cpuload()
 * directly.
 */

#include <nuttx/config.h>
#include <nuttx/clock.h>

int nuttx_get_idle_cpuload(unsigned long *out_active, unsigned long *out_total)
{
#if !defined(CONFIG_SCHED_CPULOAD_NONE)
	struct nuttx_cpuload_s load;
	int ret = clock_cpuload(0, &load);

	if (ret == 0) {
		*out_active = (unsigned long)load.active;
		*out_total  = (unsigned long)load.total;
	}

	return ret;
#else
	*out_active = 0;
	*out_total = 0;
	return -1;
#endif
}
