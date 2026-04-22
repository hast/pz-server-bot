#include <pzserverbot/rcon_client.h>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pzserverbot {
namespace {
constexpr int RCON_AUTH = 3;
constexpr int RCON_AUTH_RESPONSE = 2;
constexpr int RCON_COMMAND = 2;

int32_t read_le_i32(const char* data)
{
    const auto b0 = static_cast<unsigned char>(data[0]);
    const auto b1 = static_cast<unsigned char>(data[1]);
    const auto b2 = static_cast<unsigned char>(data[2]);
    const auto b3 = static_cast<unsigned char>(data[3]);
    return static_cast<int32_t>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
}

void append_le_i32(std::vector<char>& buffer, int32_t value)
{
    buffer.push_back(static_cast<char>(value & 0xff));
    buffer.push_back(static_cast<char>((value >> 8) & 0xff));
    buffer.push_back(static_cast<char>((value >> 16) & 0xff));
    buffer.push_back(static_cast<char>((value >> 24) & 0xff));
}

void send_all(int socket_fd, const std::vector<char>& data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t result = send(socket_fd, data.data() + sent, data.size() - sent, 0);
        if (result <= 0) {
            throw std::runtime_error("Failed to send RCON packet");
        }
        sent += static_cast<std::size_t>(result);
    }
}

void recv_all(int socket_fd, char* data, std::size_t size)
{
    std::size_t received = 0;
    while (received < size) {
        const ssize_t result = recv(socket_fd, data + received, size - received, 0);
        if (result <= 0) {
            throw std::runtime_error("Failed to receive RCON packet");
        }
        received += static_cast<std::size_t>(result);
    }
}
} // namespace

RconClient::RconClient(std::string host, int port, std::string password)
    : host_(std::move(host)), port_(port), password_(std::move(password))
{
}

RconClient::~RconClient()
{
    close_socket();
}

void RconClient::connect_and_auth()
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* addresses = nullptr;
    const std::string port = std::to_string(port_);
    const int gai_result = getaddrinfo(host_.c_str(), port.c_str(), &hints, &addresses);
    if (gai_result != 0) {
        throw std::runtime_error(std::string("RCON address lookup failed: ") + gai_strerror(gai_result));
    }

    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        socket_fd_ = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket_fd_ < 0) {
            continue;
        }

        if (::connect(socket_fd_, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }

        close_socket();
    }

    freeaddrinfo(addresses);

    if (socket_fd_ < 0) {
        throw std::runtime_error("Could not connect to RCON server");
    }

    send_packet(1, RCON_AUTH, password_);

    for (int attempt = 0; attempt < 3; ++attempt) {
        const RconResponse auth_response = receive_packet();
        if (auth_response.id == -1) {
            throw std::runtime_error("RCON authentication failed");
        }

        if (auth_response.type == RCON_AUTH_RESPONSE) {
            if (auth_response.id != 1) {
                throw std::runtime_error("RCON authentication failed");
            }
            return;
        }
    }

    throw std::runtime_error("RCON authentication response was not received");
}

std::string RconClient::command(const std::string& command)
{
    send_packet(2, RCON_COMMAND, command);
    return receive_packet().body;
}

void RconClient::close_socket()
{
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

void RconClient::send_packet(int id, int type, const std::string& body)
{
    std::vector<char> packet;
    append_le_i32(packet, static_cast<int32_t>(body.size() + 10));
    append_le_i32(packet, id);
    append_le_i32(packet, type);
    packet.insert(packet.end(), body.begin(), body.end());
    packet.push_back('\0');
    packet.push_back('\0');
    send_all(socket_fd_, packet);
}

RconClient::RconResponse RconClient::receive_packet()
{
    char size_buffer[4]{};
    recv_all(socket_fd_, size_buffer, sizeof(size_buffer));

    const int32_t packet_size = read_le_i32(size_buffer);
    if (packet_size < 10 || packet_size > 4096) {
        throw std::runtime_error("Invalid RCON packet size");
    }

    std::vector<char> packet(static_cast<std::size_t>(packet_size));
    recv_all(socket_fd_, packet.data(), packet.size());

    RconResponse response;
    response.id = read_le_i32(packet.data());
    response.type = read_le_i32(packet.data() + 4);
    response.body.assign(packet.data() + 8, packet.data() + packet.size() - 2);
    return response;
}

} // namespace pzserverbot
