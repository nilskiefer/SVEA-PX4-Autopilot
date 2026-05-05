#ifndef PX4_UORB_TUNNEL_HPP
#define PX4_UORB_TUNNEL_HPP

#include "../mavlink_main.h"

#include <uORB/uORB.h>
#include <uORB/topics/uORBTopics.hpp>

#include <cstring>

class MavlinkStreamPx4UorbTunnel : public MavlinkStream
{
public:
	static MavlinkStream *new_instance(Mavlink *mavlink) { return new MavlinkStreamPx4UorbTunnel(mavlink); }

	static constexpr const char *get_name_static() { return "PX4_UORB_TUNNEL"; }
	static constexpr uint16_t get_id_static() { return MAVLINK_MSG_ID_TUNNEL; }

	const char *get_name() const override { return get_name_static(); }
	uint16_t get_id() override { return get_id_static(); }

	unsigned get_size() override
	{
		static constexpr unsigned tunnel_size = MAVLINK_MSG_ID_TUNNEL_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES;
		unsigned frames = 0;

		for (TopicState *state = _topic_states.getHead(); state != nullptr; state = state->getSibling()) {
			if (state->meta != nullptr && state->rate_hz > 0.f) {
				frames += max_frames_for_topic(state->meta);
			}
		}

		return tunnel_size * frames;
	}

private:
	static constexpr uint16_t kTunnelPayloadType = 0xE001;
	static constexpr uint8_t kProtocolVersion = 2;
	static constexpr size_t kTunnelFrameBaseHeaderLen = 14;
	static constexpr size_t kMaxTopicNameLen = MAVLINK_MSG_TUNNEL_FIELD_PAYLOAD_LEN - kTunnelFrameBaseHeaderLen - 1;

	struct TopicState : public ListNode<TopicState *> {
		int32_t topic_id{-1};
		uint8_t instance{0};
		float rate_hz{0.f};
		const orb_metadata *meta{nullptr};
		int subscription_fd{-1};
		hrt_abstime last_sent{0};
		uint8_t *buffer{nullptr};
		bool matched{false};
	};

	explicit MavlinkStreamPx4UorbTunnel(Mavlink *mavlink) : MavlinkStream(mavlink)
	{
		refresh_topics(true);
	}

	~MavlinkStreamPx4UorbTunnel() override
	{
		clear_topic_states();
	}

	List<TopicState *> _topic_states{};
	hrt_abstime _last_refresh{0};
	uint8_t _sequence{0};

	static const orb_metadata *topic_from_orb_id(int32_t topic_id)
	{
		if (topic_id < 0 || topic_id >= static_cast<int32_t>(ORB_TOPICS_COUNT)) {
			return nullptr;
		}

		const orb_metadata *const *topics = orb_get_topics();
		return topics[topic_id];
	}

	void clear_topic_states()
	{
		for (TopicState *state = _topic_states.getHead(); state != nullptr;) {
			TopicState *next = state->getSibling();

			if (state->subscription_fd >= 0) {
				orb_unsubscribe(state->subscription_fd);
			}

			delete[] state->buffer;
			delete state;
			state = next;
		}

		_topic_states = List<TopicState *>{};
	}

	void refresh_topics(bool force = false)
	{
		const hrt_abstime now = hrt_absolute_time();

		if (!force && (now - _last_refresh) < 1_s) {
			return;
		}

		_last_refresh = now;

		for (TopicState *state = _topic_states.getHead(); state != nullptr; state = state->getSibling()) {
			state->matched = false;
		}

		const unsigned count = Mavlink::uorb_tunnel_count();

		for (unsigned i = 0; i < count; i++) {
			UorbTunnelTopicConfig cfg{};

			if (!Mavlink::uorb_tunnel_get_config(i, cfg)) {
				continue;
			}

			if (cfg.rate_hz <= 0.f) {
				continue;
			}

			const orb_metadata *meta = topic_from_orb_id(cfg.topic_id);

			if (meta == nullptr || meta->o_name == nullptr || meta->o_size_no_padding == 0) {
				continue;
			}

			TopicState *state = nullptr;

			for (TopicState *existing = _topic_states.getHead(); existing != nullptr; existing = existing->getSibling()) {
				if (existing->topic_id == cfg.topic_id && existing->instance == cfg.instance) {
					state = existing;
					break;
				}
			}

			if (state == nullptr) {
				state = new TopicState{};

				if (state == nullptr) {
					continue;
				}

				state->topic_id = cfg.topic_id;
				state->instance = cfg.instance;
				state->meta = meta;
				state->subscription_fd = orb_subscribe_multi(meta, cfg.instance);

				if (state->subscription_fd < 0) {
					delete state;
					continue;
				}

				state->buffer = new uint8_t[meta->o_size];

				if (state->buffer == nullptr) {
					orb_unsubscribe(state->subscription_fd);
					delete state;
					continue;
				}

				_topic_states.add(state);
			}

			state->rate_hz = cfg.rate_hz;
			state->matched = true;
		}

		for (TopicState *state = _topic_states.getHead(); state != nullptr;) {
			TopicState *next = state->getSibling();

			if (!state->matched) {
				_topic_states.remove(state);

				if (state->subscription_fd >= 0) {
					orb_unsubscribe(state->subscription_fd);
				}

				delete[] state->buffer;
				delete state;
			}

			state = next;
		}
	}

	static bool should_send_now(hrt_abstime now, float hz, hrt_abstime &last_sent)
	{
		if (!(hz > 0.f)) {
			return false;
		}

		const hrt_abstime interval_us = static_cast<hrt_abstime>(1e6f / hz);

		if (last_sent == 0 || now - last_sent >= interval_us) {
			last_sent = now;
			return true;
		}

		return false;
	}

	static void put_u16(uint8_t *dst, uint16_t v)
	{
		dst[0] = static_cast<uint8_t>(v & 0xFFu);
		dst[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
	}

	static void put_u32(uint8_t *dst, uint32_t v)
	{
		dst[0] = static_cast<uint8_t>(v & 0xFFu);
		dst[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
		dst[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
		dst[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
	}

	static size_t bounded_strlen(const char *str, size_t max_len)
	{
		size_t len = 0;

		while (len < max_len && str[len] != '\0') {
			len++;
		}

		return len;
	}

	static unsigned max_chunk_size_for_topic(const orb_metadata *meta)
	{
		if (meta == nullptr || meta->o_name == nullptr) {
			return 0;
		}

		const size_t topic_name_len = bounded_strlen(meta->o_name, kMaxTopicNameLen + 1);

		if (topic_name_len == 0 || topic_name_len > kMaxTopicNameLen) {
			return 0;
		}

		const size_t header_bytes = kTunnelFrameBaseHeaderLen + topic_name_len;
		return static_cast<unsigned>(MAVLINK_MSG_TUNNEL_FIELD_PAYLOAD_LEN - header_bytes);
	}

	static unsigned max_frames_for_topic(const orb_metadata *meta)
	{
		const unsigned chunk_size = max_chunk_size_for_topic(meta);

		if (chunk_size == 0 || meta == nullptr) {
			return 0;
		}

		const unsigned payload_len = static_cast<unsigned>(meta->o_size_no_padding);
		return (payload_len == 0) ? 1 : (payload_len + chunk_size - 1) / chunk_size;
	}

	bool send_uorb_payload(const orb_metadata *meta, uint8_t instance, const void *payload_raw)
	{
		if (meta == nullptr || meta->o_name == nullptr || payload_raw == nullptr) {
			return false;
		}

		const uint16_t payload_len = meta->o_size_no_padding;
		const unsigned chunk_size = max_chunk_size_for_topic(meta);

		if (payload_len == 0 || chunk_size == 0) {
			return false;
		}

		const size_t topic_name_len = bounded_strlen(meta->o_name, kMaxTopicNameLen + 1);
		if (topic_name_len == 0 || topic_name_len > kMaxTopicNameLen) {
			return false;
		}

		const uint8_t sequence = _sequence++;
		const uint8_t *payload = static_cast<const uint8_t *>(payload_raw);
		uint16_t offset = 0;
		bool sent_any = false;

		while (offset < payload_len) {
			const uint16_t remaining = payload_len - offset;
			const uint16_t chunk_len = (remaining < chunk_size) ? remaining : static_cast<uint16_t>(chunk_size);

			mavlink_tunnel_t tunnel{};
			tunnel.target_system = 0;
			tunnel.target_component = 0;
			tunnel.payload_type = kTunnelPayloadType;

			uint8_t *frame = tunnel.payload;
			frame[0] = kProtocolVersion;
			frame[1] = 0;
			frame[2] = instance;
			frame[3] = sequence;
			put_u32(&frame[4], meta->message_hash);
			put_u16(&frame[8], static_cast<uint16_t>(topic_name_len));
			put_u16(&frame[10], payload_len);
			put_u16(&frame[12], offset);

			std::memcpy(&frame[kTunnelFrameBaseHeaderLen], meta->o_name, topic_name_len);
			std::memcpy(&frame[kTunnelFrameBaseHeaderLen + topic_name_len], &payload[offset], chunk_len);

			const uint8_t frame_len = static_cast<uint8_t>(kTunnelFrameBaseHeaderLen + topic_name_len + chunk_len);
			tunnel.payload_length = frame_len;
			mavlink_msg_tunnel_send_struct(_mavlink->get_channel(), &tunnel);

			offset += chunk_len;
			sent_any = true;
		}

		return sent_any;
	}

	bool send() override
	{
		refresh_topics();
		const hrt_abstime now = hrt_absolute_time();
		bool sent_any = false;

		for (TopicState *state = _topic_states.getHead(); state != nullptr; state = state->getSibling()) {
			if (state->meta == nullptr || state->subscription_fd < 0 || state->buffer == nullptr || !(state->rate_hz > 0.f)) {
				continue;
			}

			bool updated = false;

			if (orb_check(state->subscription_fd, &updated) != PX4_OK || !updated) {
				continue;
			}

			if (!should_send_now(now, state->rate_hz, state->last_sent)) {
				continue;
			}

			if (orb_copy(state->meta, state->subscription_fd, state->buffer) == PX4_OK) {
				sent_any |= send_uorb_payload(state->meta, state->instance, state->buffer);
			}
		}

		return sent_any;
	}
};

#endif // PX4_UORB_TUNNEL_HPP
