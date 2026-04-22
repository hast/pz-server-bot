#include <pzserverbot/pzserverbot.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using json = nlohmann::json;

int main(int argc, char const *argv[])
{
    json configdocument;
    std::ifstream configfile("../config.json");
    configfile >> configdocument;

    const std::string BOT_TOKEN = configdocument["token"];
    const std::string WEBHOOK_URL = configdocument["webhook_url"];
    const std::string RCON_HOST = configdocument.value("rcon_host", "127.0.0.1");
    const int RCON_PORT = configdocument.value("rcon_port", 16262);
    const std::string RCON_PASSWORD = configdocument.value("rcon_password", "");
    const int POLL_SECONDS = configdocument.value("poll_seconds", 60);

    dpp::cluster bot(BOT_TOKEN);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping") {
            event.reply("Pong!");
        }
    });

    std::atomic_bool polling_started = false;
    // TODO: Persist this message id so bot restarts edit the same Discord status message.
    pzserverbot::WebhookStatusMessage status_message;

    bot.on_ready([&bot, &WEBHOOK_URL, &RCON_HOST, RCON_PORT, &RCON_PASSWORD, POLL_SECONDS, &polling_started, &status_message](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
        }

        if (dpp::run_once<struct start_player_status_polling>()) {
            if (RCON_PASSWORD.empty()) {
                std::cerr << "Missing rcon_password in ../config.json\n";
                return;
            }

            if (polling_started.exchange(true)) {
                return;
            }

            std::thread([&bot, WEBHOOK_URL, RCON_HOST, RCON_PORT, RCON_PASSWORD, POLL_SECONDS, &status_message] {
                while (true) {
                    const pzserverbot::PlayerStatus status =
                        pzserverbot::fetch_player_status(RCON_HOST, RCON_PORT, RCON_PASSWORD);

                    pzserverbot::publish_player_status_webhook(bot, WEBHOOK_URL, status, status_message);

                    std::this_thread::sleep_for(std::chrono::seconds(std::max(5, POLL_SECONDS)));
                }
            }).detach();
        }
    });

    bot.start(dpp::st_wait);
}
