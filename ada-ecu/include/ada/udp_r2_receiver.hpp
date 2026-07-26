#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace ada {

class UdpR2Receiver {
public:
    UdpR2Receiver(const std::string& listen_host, int listen_port);
    ~UdpR2Receiver();

    UdpR2Receiver(const UdpR2Receiver&) = delete;
    UdpR2Receiver& operator=(const UdpR2Receiver&) = delete;

    std::optional<std::string> receive_one(std::chrono::milliseconds timeout);

private:
    int fd_ = -1;
};

void send_udp_datagram(const std::string& host, int port, const std::string& payload);

}  // namespace ada

