#include "ada/udp_r2_receiver.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <stdexcept>

namespace ada {
namespace {

sockaddr_in make_address(const std::string& host, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("invalid IPv4 address: " + host);
    }
    return addr;
}

}  // namespace

UdpR2Receiver::UdpR2Receiver(const std::string& listen_host, int listen_port) {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("cannot create ADA UDP receiver socket");
    }

    int reuse = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    const auto addr = make_address(listen_host, listen_port);
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        const auto error = std::string(std::strerror(errno));
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("cannot bind ADA UDP receiver: " + error);
    }
}

UdpR2Receiver::~UdpR2Receiver() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

std::optional<std::string> UdpR2Receiver::receive_one(std::chrono::milliseconds timeout) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_, &read_fds);

    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    const auto ready = ::select(fd_ + 1, &read_fds, nullptr, nullptr, &tv);
    if (ready < 0) {
        throw std::runtime_error("ADA UDP receiver select failed");
    }
    if (ready == 0) {
        return std::nullopt;
    }

    std::array<char, 8192> buffer{};
    const auto size = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, nullptr, nullptr);
    if (size < 0) {
        throw std::runtime_error("ADA UDP receiver recvfrom failed");
    }
    return std::string(buffer.data(), static_cast<std::size_t>(size));
}

void send_udp_datagram(const std::string& host, int port, const std::string& payload) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::runtime_error("cannot create UDP sender socket");
    }

    const auto addr = make_address(host, port);
    const auto sent = ::sendto(fd, payload.data(), payload.size(), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    if (sent < 0 || static_cast<std::size_t>(sent) != payload.size()) {
        throw std::runtime_error("cannot send UDP datagram");
    }
}

}  // namespace ada
