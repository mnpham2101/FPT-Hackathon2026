#include "ada/udp_r4_sender.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace ada {

UdpR4Sender::UdpR4Sender(std::string host, int port) : host_(std::move(host)), port_(port) {}

void UdpR4Sender::send(const std::string& payload) const {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::runtime_error("cannot create ADA R4 UDP sender socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port_));
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("invalid IVI IPv4 address: " + host_);
    }

    const auto sent = ::sendto(fd, payload.data(), payload.size(), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    if (sent < 0 || static_cast<std::size_t>(sent) != payload.size()) {
        throw std::runtime_error("cannot send R4 warning to IVI");
    }
}

}  // namespace ada

