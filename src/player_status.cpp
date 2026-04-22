#include <pzserverbot/player_status.h>

#include <pzserverbot/rcon_client.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <exception>
#include <iostream>
#include <sstream>

namespace pzserverbot {
namespace {
std::string trim(std::string value)
{
    const auto is_not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
    return value;
}

std::string to_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
} // namespace

std::vector<std::string> parse_players(const std::string& response)
{
    std::vector<std::string> players;
    std::istringstream lines(response);
    std::string line;

    while (std::getline(lines, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::string lower_line = to_lower(line);
        if (lower_line.find("players connected") != std::string::npos ||
            lower_line.find("no players") != std::string::npos ||
            lower_line == "players") {
            continue;
        }

        players.push_back(line);
    }

    return players;
}

PlayerStatus fetch_player_status(const std::string& host, int port, const std::string& password)
{
    try {
        RconClient rcon(host, port, password);
        rcon.connect_and_auth();

        PlayerStatus status;
        status.ok = true;
        status.raw_response = rcon.command("players");
        status.players = parse_players(status.raw_response);
        return status;
    } catch (const std::exception& exception) {
        return PlayerStatus{
            .ok = false,
            .error = exception.what(),
        };
    }
}

dpp::embed make_player_embed(const PlayerStatus& status)
{
    dpp::embed embed = dpp::embed()
        .set_title("Project Zomboid Server")
        .set_timestamp(std::time(nullptr));

    if (!status.ok) {
        return embed
            .set_color(0xff5555)
            .set_description("RCON check failed.")
            .add_field("Error", status.error.empty() ? "Unknown error" : status.error);
    }

    std::string player_list = "Nobody is online.";
    if (!status.players.empty()) {
        player_list.clear();
        for (const std::string& player : status.players) {
            player_list += "- " + player + "\n";
        }
    }

    return embed
        .set_color(dpp::colors::sti_blue)
        .set_description("Current server population")
        .add_field("Players", std::to_string(status.players.size()), true)
        .add_field("Player List", player_list);
}

namespace {
dpp::message make_player_status_message(const PlayerStatus& status)
{
    dpp::message msg;
    msg.add_embed(make_player_embed(status));
    return msg;
}
} // namespace

void publish_player_status_webhook(
    dpp::cluster& bot,
    const std::string& webhook_url,
    const PlayerStatus& status,
    WebhookStatusMessage& status_message
)
{
    dpp::message msg = make_player_status_message(status);
    const uint64_t message_id = status_message.message_id.load();

    if (message_id != 0) {
        msg.id = dpp::snowflake(message_id);
        bot.edit_webhook_message(
            dpp::webhook(webhook_url),
            msg,
            0,
            [](const dpp::confirmation_callback_t& callback) {
                if (callback.is_error()) {
                    std::cerr << "Webhook edit failed: " << callback.get_error().message << '\n';
                }
            }
        );
        return;
    }

    if (status_message.create_in_progress.exchange(true)) {
        return;
    }

    bot.execute_webhook(
        dpp::webhook(webhook_url),
        msg,
        true,
        0,
        "",
        [&status_message](const dpp::confirmation_callback_t& callback) {
            status_message.create_in_progress = false;

            if (callback.is_error()) {
                std::cerr << "Webhook failed: " << callback.get_error().message << '\n';
                return;
            }

            const dpp::message sent_message = callback.get<dpp::message>();
            status_message.message_id = static_cast<uint64_t>(sent_message.id);
        }
    );
}

} // namespace pzserverbot
