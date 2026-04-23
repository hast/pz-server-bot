#include <pzserverbot/server_time.h>

#include <dpp/nlohmann/json.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

namespace pzserverbot {
namespace {
using json = nlohmann::json;

std::string format_date(int year, int month, int day)
{
    std::ostringstream output;
    output << std::setfill('0')
           << std::setw(4) << year << '-'
           << std::setw(2) << month << '-'
           << std::setw(2) << day;
    return output.str();
}

std::string format_time(int hour, int minute)
{
    std::ostringstream output;
    output << std::setfill('0')
           << std::setw(2) << hour << ':'
           << std::setw(2) << minute;
    return output.str();
}
} // namespace

std::optional<ServerTime> read_server_time(const std::string& path)
{
    if (path.empty()) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    json document;
    file >> document;

    ServerTime server_time;
    server_time.year = document.value("year", 0);
    server_time.month = document.value("month", 0);
    server_time.day = document.value("day", 0);
    server_time.hour = document.value("hour", 0);
    server_time.minute = document.value("minute", 0);
    server_time.date = document.value("date", "");
    server_time.time = document.value("time", "");

    if (server_time.year == 0 || server_time.month == 0 || server_time.day == 0) {
        return std::nullopt;
    }

    if (server_time.date.empty()) {
        server_time.date = format_date(server_time.year, server_time.month, server_time.day);
    }

    if (server_time.time.empty()) {
        server_time.time = format_time(server_time.hour, server_time.minute);
    }

    return server_time;
}

std::string format_server_time(const ServerTime& server_time)
{
    return server_time.date + " " + server_time.time;
}

} // namespace pzserverbot
