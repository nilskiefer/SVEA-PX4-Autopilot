/****************************************************************************
 *
 *   Copyright (c) 2012-2023 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file mavlink_main.cpp
 * MAVLink 1.0 protocol implementation.
 *
 * @author Lorenz Meier <lm@inf.ethz.ch>
 * @author Julian Oes <julian@oes.ch>
 * @author Anton Babushkin <anton.babushkin@me.com>
 */

#include "mavlink_main.h"
#include <uORB/topics/uORBTopics.hpp>
#include <cstdlib>
#include <cmath>

namespace
{
pthread_mutex_t g_uorb_tunnel_mutex = PTHREAD_MUTEX_INITIALIZER;

struct UorbTunnelTopicEntry : public ListNode<UorbTunnelTopicEntry *> {
	UorbTunnelTopicConfig cfg{};
};

List<UorbTunnelTopicEntry *> g_uorb_tunnel_topics{};

static const orb_metadata *topic_from_id(int32_t topic_id)
{
	if (topic_id < 0 || topic_id >= static_cast<int32_t>(ORB_TOPICS_COUNT)) {
		return nullptr;
	}

	const orb_metadata *const *topics = orb_get_topics();
	return topics[topic_id];
}

static int topic_id_from_name(const char *topic_name)
{
	if (topic_name == nullptr || topic_name[0] == '\0') {
		return -1;
	}

	const orb_metadata *const *topics = orb_get_topics();

	for (int i = 0; i < static_cast<int>(ORB_TOPICS_COUNT); i++) {
		const orb_metadata *meta = topics[i];

		if (meta != nullptr && meta->o_name != nullptr && strcmp(meta->o_name, topic_name) == 0) {
			return i;
		}
	}

	return -1;
}

static unsigned topic_entry_count_locked()
{
	unsigned count = 0;

	for (UorbTunnelTopicEntry *entry = g_uorb_tunnel_topics.getHead(); entry != nullptr; entry = entry->getSibling()) {
		count++;
	}

	return count;
}

static UorbTunnelTopicEntry *topic_entry_at_locked(unsigned index)
{
	unsigned i = 0;

	for (UorbTunnelTopicEntry *entry = g_uorb_tunnel_topics.getHead(); entry != nullptr; entry = entry->getSibling()) {
		if (i == index) {
			return entry;
		}

		i++;
	}

	return nullptr;
}

} // namespace

unsigned
Mavlink::uorb_tunnel_count()
{
	pthread_mutex_lock(&g_uorb_tunnel_mutex);
	const unsigned count = topic_entry_count_locked();
	pthread_mutex_unlock(&g_uorb_tunnel_mutex);
	return count;
}

bool Mavlink::uorb_tunnel_get_config(unsigned index, UorbTunnelTopicConfig &out)
{
	pthread_mutex_lock(&g_uorb_tunnel_mutex);
	UorbTunnelTopicEntry *entry = topic_entry_at_locked(index);

	if (entry == nullptr) {
		pthread_mutex_unlock(&g_uorb_tunnel_mutex);
		return false;
	}

	out = entry->cfg;
	pthread_mutex_unlock(&g_uorb_tunnel_mutex);
	return true;
}

int Mavlink::uorb_tunnel_command(int argc, char *argv[])
{
	if (argc < 3) {
		PX4_ERR("usage: mavlink uorb_tunnel {list|add|remove}");
		return PX4_ERROR;
	}

	const char *subcmd = argv[2];

	if (!strcmp(subcmd, "list")) {
		pthread_mutex_lock(&g_uorb_tunnel_mutex);

		if (g_uorb_tunnel_topics.getHead() == nullptr) {
			PX4_INFO("uorb tunnel: no topics configured");
			pthread_mutex_unlock(&g_uorb_tunnel_mutex);
			return PX4_OK;
		}

		unsigned i = 0;

		for (UorbTunnelTopicEntry *entry = g_uorb_tunnel_topics.getHead(); entry != nullptr; entry = entry->getSibling()) {
			const UorbTunnelTopicConfig &cfg = entry->cfg;
			const orb_metadata *meta = topic_from_id(cfg.topic_id);

			if (meta == nullptr || meta->o_name == nullptr) {
				PX4_WARN("%u: invalid topic_id=%ld inst=%u rate=%.3f", static_cast<unsigned>(i + 1),
					 static_cast<long>(cfg.topic_id), cfg.instance, static_cast<double>(cfg.rate_hz));

			} else {
				PX4_INFO("%u: topic=%s (id=%ld) inst=%u rate=%.3f Hz", static_cast<unsigned>(i + 1), meta->o_name,
					 static_cast<long>(cfg.topic_id), cfg.instance, static_cast<double>(cfg.rate_hz));
			}

			i++;
		}

		pthread_mutex_unlock(&g_uorb_tunnel_mutex);
		return PX4_OK;
	}

	if (!strcmp(subcmd, "remove")) {
		int entry = -1;

		for (int i = 3; i < argc; i++) {
			if (!strcmp(argv[i], "-s") && (i + 1) < argc) {
				entry = strtol(argv[++i], nullptr, 10);

			} else {
				PX4_ERR("invalid argument: %s", argv[i]);
				return PX4_ERROR;
			}
		}

		if (entry < 1) {
			PX4_ERR("remove requires -s <entry>=1..N");
			return PX4_ERROR;
		}

		pthread_mutex_lock(&g_uorb_tunnel_mutex);

		const size_t idx = static_cast<size_t>(entry - 1);
		UorbTunnelTopicEntry *to_remove = topic_entry_at_locked(idx);

		if (to_remove == nullptr) {
			const unsigned count = topic_entry_count_locked();
			pthread_mutex_unlock(&g_uorb_tunnel_mutex);
			PX4_ERR("entry out of range (N=%u)", count);
			return PX4_ERROR;
		}

		g_uorb_tunnel_topics.remove(to_remove);
		pthread_mutex_unlock(&g_uorb_tunnel_mutex);
		delete to_remove;
		PX4_INFO("uorb tunnel entry %d removed", entry);
		return PX4_OK;
	}

	if (!strcmp(subcmd, "add")) {
		const char *topic_name = nullptr;
		int32_t instance = 0;
		float rate_hz = -1.f;
		bool force = false;

		for (int i = 3; i < argc; i++) {
			if (!strcmp(argv[i], "-t") && (i + 1) < argc) {
				topic_name = argv[++i];

			} else if (!strcmp(argv[i], "-i") && (i + 1) < argc) {
				instance = strtol(argv[++i], nullptr, 10);

			} else if (!strcmp(argv[i], "-r") && (i + 1) < argc) {
				rate_hz = strtof(argv[++i], nullptr);

			} else if (!strcmp(argv[i], "-f")) {
				force = true;

			} else {
				PX4_ERR("invalid argument: %s", argv[i]);
				return PX4_ERROR;
			}
		}

		if (topic_name == nullptr || rate_hz <= 0.f) {
			PX4_ERR("add requires -t <topic> and -r <hz>");
			return PX4_ERROR;
		}

		if (instance < 0) {
			PX4_ERR("instance must be >= 0");
			return PX4_ERROR;
		}

		if (instance > 9) {
			PX4_ERR("instance must be <= 9");
			return PX4_ERROR;
		}

		if (rate_hz > 100.f) {
			PX4_ERR("rate must be <= 100 Hz");
			return PX4_ERROR;
		}

		const int topic_id = topic_id_from_name(topic_name);

		if (topic_id < 0) {
			PX4_ERR("unknown uORB topic: %s", topic_name);
			return PX4_ERROR;
		}

		const orb_metadata *meta = topic_from_id(topic_id);

		if (meta == nullptr || meta->o_name == nullptr) {
			PX4_ERR("invalid uORB topic metadata: %s", topic_name);
			return PX4_ERROR;
		}

		if (!force && orb_exists(meta, instance) != PX4_OK) {
			PX4_ERR("uorb tunnel add rejected: topic=%s instance=%ld is not advertised", meta->o_name,
				static_cast<long>(instance));
			return PX4_ERROR;
		}

		pthread_mutex_lock(&g_uorb_tunnel_mutex);

		for (UorbTunnelTopicEntry *entry = g_uorb_tunnel_topics.getHead(); entry != nullptr; entry = entry->getSibling()) {
			UorbTunnelTopicConfig &cfg = entry->cfg;

			if (cfg.topic_id == topic_id && cfg.instance == static_cast<uint8_t>(instance)) {
				cfg.rate_hz = rate_hz;
				pthread_mutex_unlock(&g_uorb_tunnel_mutex);
				PX4_INFO("uorb tunnel updated%s: topic=%s id=%d inst=%ld rate=%.3f Hz", force ? " (forced)" : "",
					 topic_name, topic_id, static_cast<long>(instance), static_cast<double>(rate_hz));
				return PX4_OK;
			}
		}

		UorbTunnelTopicEntry *new_entry = new UorbTunnelTopicEntry{};

		if (new_entry == nullptr) {
			pthread_mutex_unlock(&g_uorb_tunnel_mutex);
			PX4_ERR("uorb tunnel add failed: allocation");
			return PX4_ERROR;
		}

		new_entry->cfg.topic_id = topic_id;
		new_entry->cfg.instance = static_cast<uint8_t>(instance);
		new_entry->cfg.rate_hz = rate_hz;
		g_uorb_tunnel_topics.add(new_entry);
		const unsigned new_index = topic_entry_count_locked();
		pthread_mutex_unlock(&g_uorb_tunnel_mutex);
		PX4_INFO("uorb tunnel added%s (%u): topic=%s id=%d inst=%ld rate=%.3f Hz", force ? " (forced)" : "",
			 new_index, topic_name, topic_id, static_cast<long>(instance), static_cast<double>(rate_hz));
		return PX4_OK;
	}

	PX4_ERR("unknown uorb_tunnel command: %s", subcmd);
	return PX4_ERROR;
}
