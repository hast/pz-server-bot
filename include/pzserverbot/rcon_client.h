#pragma once

#include <string>

namespace pzserverbot {

class RconClient {
public:
    RconClient(std::string host, int port, std::string password);
    ~RconClient();

    RconClient(const RconClient&) = delete;
    RconClient& operator=(const RconClient&) = delete;

    void connect_and_auth();
    std::string command(const std::string& command);

private:
    struct RconResponse {
        int id{};
        int type{};
        std::string body;
    };

    void close_socket();
    void send_packet(int id, int type, const std::string& body);
    RconResponse receive_packet();

    std::string host_;
    int port_{};
    std::string password_;
    int socket_fd_{-1};
};

} // namespace pzserverbot
