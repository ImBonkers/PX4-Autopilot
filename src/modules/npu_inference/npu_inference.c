/****************************************************************************
 * PX4 NPU Inference Module
 *
 * Runs YOLOv8n inference on the STM32N6 ATON NPU via /dev/npu0.
 * Feeds fake input data, measures inference time.
 * Used to benchmark NPU impact on flight control loop performance.
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>
#include <px4_platform_common/tasks.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <drivers/drv_hrt.h>
#include <parameters/param.h>
#include <uORB/uORB.h>
#include <uORB/topics/debug_key_value.h>

/* AIE ioctl commands -- match NuttX ai_engine enum + stm32_npu.h */
#define AIE_CMD_LOAD        0
#define AIE_CMD_FEED_INPUT  1
#define AIE_CMD_GET_OUTPUT  2
#define NPUIOC_RUN_SYNC     0x100

/* Input: 192x192x3 INT8, Output: 756x5 INT8 */
#define INPUT_SIZE   110592
#define OUTPUT_SIZE  15120

static bool _running;
static int _run_count;
static uint64_t _total_us;
static uint64_t _last_us;
static uint64_t _min_us;
static uint64_t _max_us;
static int _duty_pct = 0;  /* 0=paused at boot, set via param or NSH */
static uint64_t _start_time;

static int npu_inference_thread(int argc, char *argv[])
{
	int fd;
	int ret;

	fd = open("/dev/npu0", O_RDWR);

	if (fd < 0) {
		PX4_ERR("Failed to open /dev/npu0: %d", errno);
		_running = false;
		return -1;
	}

	ret = ioctl(fd, AIE_CMD_LOAD, 0);

	if (ret < 0) {
		PX4_ERR("AIE_CMD_LOAD failed: %d", errno);
		close(fd);
		_running = false;
		return -1;
	}

	static uint8_t input[INPUT_SIZE] __attribute__((aligned(32)));
	static uint8_t output[OUTPUT_SIZE] __attribute__((aligned(32)));
	memset(input, 128, INPUT_SIZE);

	_run_count = 0;
	_total_us = 0;
	_min_us = UINT64_MAX;
	_max_us = 0;
	_start_time = hrt_absolute_time();

	/* Publish inference stats as NAMED_VALUE_FLOAT via debug_key_value */
	orb_advert_t dbg_pub = orb_advertise(ORB_ID(debug_key_value), NULL);
	hrt_abstime last_publish = 0;

	/* Parameter handle for MAVLink-settable duty cycle */
	param_t duty_param = param_find("NPU_DUTY_PCT");

	while (_running) {
		hrt_abstime start = hrt_absolute_time();

		ret = ioctl(fd, AIE_CMD_FEED_INPUT, (uintptr_t)input);

		if (ret < 0) {
			PX4_ERR("FEED_INPUT failed: %d", errno);
			break;
		}

		ret = ioctl(fd, NPUIOC_RUN_SYNC, 0);

		if (ret < 0) {
			PX4_ERR("RUN_SYNC failed: %d", errno);
			break;
		}

		ret = ioctl(fd, AIE_CMD_GET_OUTPUT, (uintptr_t)output);

		if (ret < 0) {
			PX4_ERR("GET_OUTPUT failed: %d", errno);
			break;
		}

		hrt_abstime elapsed = hrt_absolute_time() - start;
		_last_us = elapsed;
		_total_us += elapsed;
		_run_count++;

		if (elapsed < _min_us) { _min_us = elapsed; }
		if (elapsed > _max_us) { _max_us = elapsed; }

		/* Adaptive duty cycle: sleep = t * (100/d - 1)
		 * d=100%: sleep=0, d=50%: sleep=t, d=25%: sleep=3t */
		if (_duty_pct > 0 && _duty_pct < 100) {
			useconds_t sleep_us = (useconds_t)(elapsed * (100 - _duty_pct) / _duty_pct);

			if (sleep_us > 0) {
				usleep(sleep_us);
			}

		} else if (_duty_pct == 0) {
			/* Paused — poll param until duty changes */
			while (_duty_pct == 0 && _running) {
				int32_t param_duty = 0;

				if (duty_param != PARAM_INVALID) {
					param_get(duty_param, &param_duty);

					if (param_duty > 0 && param_duty <= 100) {
						_duty_pct = param_duty;
					}
				}

				usleep(100000);
			}
		}

		/* Poll parameter + publish stats every ~1s */
		hrt_abstime now = hrt_absolute_time();

		if (now - last_publish > 1000000) {
			/* Check if duty cycle changed via MAVLink PARAM_SET */
			int32_t param_duty = 0;

			if (duty_param != PARAM_INVALID) {
				param_get(duty_param, &param_duty);

				if (param_duty >= 0 && param_duty <= 100 && param_duty != _duty_pct) {
					_duty_pct = param_duty;
				}
			}
			struct debug_key_value_s dbg = {0};
			dbg.timestamp = now;

			strncpy(dbg.key, "npu_ms", 10);
			dbg.value = (float)elapsed / 1000.f;
			orb_publish(ORB_ID(debug_key_value), dbg_pub, &dbg);
			usleep(10);  /* let MAVLink pick it up */

			strncpy(dbg.key, "npu_fps", 10);
			dbg.value = _run_count > 0 ? 1e6f / ((float)_total_us / _run_count) : 0.f;
			dbg.timestamp = hrt_absolute_time();
			orb_publish(ORB_ID(debug_key_value), dbg_pub, &dbg);
			usleep(10);

			strncpy(dbg.key, "npu_avg", 10);
			dbg.value = _run_count > 0 ? (float)(_total_us / _run_count) / 1000.f : 0.f;
			dbg.timestamp = hrt_absolute_time();
			orb_publish(ORB_ID(debug_key_value), dbg_pub, &dbg);
			usleep(10);

			strncpy(dbg.key, "npu_duty", 10);
			dbg.value = (float)_duty_pct;
			dbg.timestamp = hrt_absolute_time();
			orb_publish(ORB_ID(debug_key_value), dbg_pub, &dbg);

			last_publish = now;
		}

		usleep(100);  /* Minimal yield between inferences */
	}

	close(fd);
	_running = false;
	return 0;
}

__EXPORT int npu_main(int argc, char *argv[])
{
	if (argc > 1 && !strcmp(argv[1], "stop")) {
		if (_running) {
			_running = false;
			PX4_INFO("stopping");
		}

		return 0;
	}

	if (argc > 1 && !strcmp(argv[1], "status")) {
		if (!_running) {
			PX4_INFO("not running");
		} else {
			PX4_INFO("running, duty=%d%%", _duty_pct);
			PX4_INFO("  inferences: %d", _run_count);
			PX4_INFO("  last: %llu us", (unsigned long long)_last_us);
			PX4_INFO("  avg:  %llu us",
				 _run_count > 0 ? (unsigned long long)(_total_us / _run_count) : 0ULL);
			PX4_INFO("  min:  %llu us", (unsigned long long)_min_us);
			PX4_INFO("  max:  %llu us", (unsigned long long)_max_us);
			double wall_s = (double)(hrt_absolute_time() - _start_time) / 1e6;
			PX4_INFO("  fps:  %.1f (wall), %.1f (pure)",
				 wall_s > 0.0 ? _run_count / wall_s : 0.0,
				 _run_count > 0 ? 1e6 / (double)(_total_us / _run_count) : 0.0);
		}

		return 0;
	}

	if (argc > 1 && !strcmp(argv[1], "reset")) {
		_run_count = 0;
		_total_us = 0;
		_last_us = 0;
		_min_us = UINT64_MAX;
		_max_us = 0;
		_start_time = hrt_absolute_time();
		PX4_INFO("stats reset");
		return 0;
	}

	if (argc > 1 && !strcmp(argv[1], "duty")) {
		if (argc > 2) {
			int d = atoi(argv[2]);

			if (d < 0) { d = 0; }

			if (d > 100) { d = 100; }

			_duty_pct = d;

			/* Sync to parameter so MAVLink reads back correct value */
			param_t p = param_find("NPU_DUTY_PCT");

			if (p != PARAM_INVALID) {
				int32_t val = d;
				param_set(p, &val);
			}
		}

		PX4_INFO("duty=%d%%", _duty_pct);
		return 0;
	}

	if (_running) {
		PX4_WARN("already running");
		return -1;
	}

	_running = true;

	int task = px4_task_spawn_cmd("npu_inference",
				      SCHED_DEFAULT,
				      SCHED_PRIORITY_DEFAULT - 10,
				      4096,
				      npu_inference_thread,
				      NULL);

	if (task < 0) {
		PX4_ERR("task spawn failed: %d", task);
		_running = false;
		return -1;
	}

	return 0;
}
