#pragma once

#include <string>

namespace ada {

class UdpR4Sender {
public:
    UdpR4Sender(std::string host, int port);

    void send(const std::string& payload) const;

private:
    std::string host_;
    int port_ = 0;
};

}  // namespace ada

