#pragma once

#include <dpp/dpp.h>

#include <pzserverbot/server_time.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pzserverbot {

struct PlayerStatus {
    bool ok{};
    std::string error;
    std::string raw_response;
    std::vector<std::string> players;
};

std::vector<std::string> parse_players(const std::string& response);
PlayerStatus fetch_player_status(const std::string& host, int port, const std::string& password);
dpp::embed make_player_embed(
    const PlayerStatus& status,
    const std::optional<ServerTime>& server_time = std::nullopt
);

struct WebhookStatusMessage {
    std::atomic<uint64_t> message_id{0};
    std::atomic_bool create_in_progress{false};
};

void publish_player_status_webhook(
    dpp::cluster& bot,
    const std::string& webhook_url,
    const PlayerStatus& status,
    const std::optional<ServerTime>& server_time,
    WebhookStatusMessage& status_message
);

} // namespace pzserverbot
