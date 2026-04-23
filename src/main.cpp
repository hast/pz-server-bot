#include <pzserverbot/pzserverbot.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <variant>

using json = nlohmann::json;

namespace {
std::string resolve_config_relative_path(const std::filesystem::path& config_path, const std::string& path)
{
    if (path.empty()) {
        return "";
    }

    const std::filesystem::path configured_path(path);
    if (configured_path.is_absolute()) {
        return configured_path.lexically_normal().string();
    }

    return (config_path.parent_path() / configured_path).lexically_normal().string();
}
} // namespace

int main(int argc, char const *argv[])
{
    const std::filesystem::path config_path("../config.json");
    json configdocument;
    std::ifstream configfile(config_path);
    configfile >> configdocument;

    const std::string BOT_TOKEN = configdocument["token"];
    const std::string WEBHOOK_URL = configdocument["webhook_url"];
    const std::string RCON_HOST = configdocument.value("rcon_host", "127.0.0.1");
    const int RCON_PORT = configdocument.value("rcon_port", 16262);
    const std::string RCON_PASSWORD = configdocument.value("rcon_password", "");
    const int POLL_SECONDS = configdocument.value("poll_seconds", 60);
    const uint64_t GUILD_ID = configdocument.value("guild_id", 0ULL);
    const std::string TELEMETRY_PATH =
        resolve_config_relative_path(config_path, configdocument.value("telemetry_path", ""));

    dpp::cluster bot(BOT_TOKEN);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&RCON_HOST, RCON_PORT, &RCON_PASSWORD](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping") {
            event.reply("Pong!");
            return;
        }

        if (event.command.get_command_name() != "rcon") {
            return;
        }

        const std::string command = std::get<std::string>(event.get_parameter("command"));

        try {
            pzserverbot::RconClient rcon(RCON_HOST, RCON_PORT, RCON_PASSWORD);
            rcon.connect_and_auth();

            std::string response = rcon.command(command);
            if (response.empty()) {
                response = "(empty response)";
            }

            if (response.size() > 1900) {
                dpp::message msg("RCON response was too long, attached as a text file.");
                msg.add_file("rcon-response.txt", response);
                event.reply(msg);
                return;
            }

            event.reply("```text\n" + response + "\n```");
        } catch (const std::exception& exception) {
            event.reply(std::string("RCON failed: ") + exception.what());
        }
    });

    std::atomic_bool polling_started = false;
    // TODO: Persist this message id so bot restarts edit the same Discord status message.
    pzserverbot::WebhookStatusMessage status_message;

    bot.on_ready([&bot, &WEBHOOK_URL, &RCON_HOST, RCON_PORT, &RCON_PASSWORD, POLL_SECONDS, GUILD_ID, &TELEMETRY_PATH, &polling_started, &status_message](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            const dpp::slashcommand ping_command("ping", "Ping pong!", bot.me.id);
            const dpp::slashcommand rcon_command =
                dpp::slashcommand("rcon", "Run a Project Zomboid RCON command", bot.me.id)
                    .add_option(
                        dpp::command_option(
                            dpp::co_string,
                            "command",
                            "RCON command to run",
                            true
                        )
                    );

            if (GUILD_ID != 0) {
                bot.guild_command_create(ping_command, dpp::snowflake(GUILD_ID));
                bot.guild_command_create(rcon_command, dpp::snowflake(GUILD_ID));
            } else {
                bot.global_command_create(ping_command);
                bot.global_command_create(rcon_command);
            }
        }

        if (dpp::run_once<struct start_player_status_polling>()) {
            if (RCON_PASSWORD.empty()) {
                std::cerr << "Missing rcon_password in ../config.json\n";
                return;
            }

            if (polling_started.exchange(true)) {
                return;
            }

            std::thread([&bot, WEBHOOK_URL, RCON_HOST, RCON_PORT, RCON_PASSWORD, POLL_SECONDS, TELEMETRY_PATH, &status_message] {
                while (true) {
                    const pzserverbot::PlayerStatus status =
                        pzserverbot::fetch_player_status(RCON_HOST, RCON_PORT, RCON_PASSWORD);
                    const std::optional<pzserverbot::ServerTime> server_time =
                        pzserverbot::read_server_time(TELEMETRY_PATH);

                    pzserverbot::publish_player_status_webhook(
                        bot,
                        WEBHOOK_URL,
                        status,
                        server_time,
                        status_message
                    );

                    std::this_thread::sleep_for(std::chrono::seconds(std::max(5, POLL_SECONDS)));
                }
            }).detach();
        }
    });

    bot.start(dpp::st_wait);
}
