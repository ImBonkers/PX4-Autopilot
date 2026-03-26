/**
 * @file npu_inference_params.c
 * NPU inference module parameters.
 */

/**
 * NPU inference duty cycle.
 *
 * Controls NPU utilization as percentage of time spent inferring.
 * 0 = paused, 25/50/75 = proportional load, 100 = continuous.
 * Settable via MAVLink PARAM_SET for automated test scenarios.
 *
 * @min 0
 * @max 100
 * @group NPU
 */
PARAM_DEFINE_INT32(NPU_DUTY_PCT, 0);
