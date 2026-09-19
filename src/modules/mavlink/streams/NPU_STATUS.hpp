#ifndef NPU_STATUS_HPP
#define NPU_STATUS_HPP

#include <uORB/topics/npu_status.h>

/**
 * Sends NPU inference stats as 4 NAMED_VALUE_FLOAT messages from a single
 * npu_status uORB topic.  One publish → one subscription → no overwrite race.
 */
class MavlinkStreamNpuStatus : public MavlinkStream
{
public:
	static MavlinkStream *new_instance(Mavlink *mavlink) { return new MavlinkStreamNpuStatus(mavlink); }

	static constexpr const char *get_name_static() { return "NPU_STATUS"; }
	static constexpr uint16_t get_id_static() { return MAVLINK_MSG_ID_NAMED_VALUE_FLOAT; }

	const char *get_name() const override { return get_name_static(); }
	uint16_t get_id() override { return get_id_static(); }

	unsigned get_size() override
	{
		return _npu_status_sub.advertised() ?
		       4 * (MAVLINK_MSG_ID_NAMED_VALUE_FLOAT_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES) : 0;
	}

private:
	explicit MavlinkStreamNpuStatus(Mavlink *mavlink) : MavlinkStream(mavlink) {}

	uORB::Subscription _npu_status_sub{ORB_ID(npu_status)};

	bool send() override
	{
		npu_status_s npu;

		if (_npu_status_sub.update(&npu)) {
			const uint32_t boot_ms = npu.timestamp / 1000ULL;

			mavlink_named_value_float_t msg{};
			msg.time_boot_ms = boot_ms;

			memcpy(msg.name, "npu_ms\0\0\0", 10);
			msg.value = npu.inference_ms;
			mavlink_msg_named_value_float_send_struct(_mavlink->get_channel(), &msg);

			memcpy(msg.name, "npu_fps\0\0", 10);
			msg.value = npu.fps;
			mavlink_msg_named_value_float_send_struct(_mavlink->get_channel(), &msg);

			memcpy(msg.name, "npu_avg\0\0", 10);
			msg.value = npu.avg_ms;
			mavlink_msg_named_value_float_send_struct(_mavlink->get_channel(), &msg);

			memcpy(msg.name, "npu_duty\0", 10);
			msg.value = (float)npu.duty_pct;
			mavlink_msg_named_value_float_send_struct(_mavlink->get_channel(), &msg);

			return true;
		}

		return false;
	}
};

#endif // NPU_STATUS_HPP
