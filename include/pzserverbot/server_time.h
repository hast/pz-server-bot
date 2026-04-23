#pragma once

#include <optional>
#include <string>

namespace pzserverbot {

struct ServerTime {
    int year{};
    int month{};
    int day{};
    int hour{};
    int minute{};
    std::string date;
    std::string time;
};

std::optional<ServerTime> read_server_time(const std::string& path);
std::string format_server_time(const ServerTime& server_time);

} // namespace pzserverbot
