#include "net/udp_socket.hpp"

// POSIX socket headers live here and nowhere else under ADA_ECU/src.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace ada::net {

namespace {

// Maximum UDP payload; every R2/R4 message is far smaller.
constexpr std::size_t kMaxDatagramBytes = 65535;

sockaddr_in makeAddr(const std::string& host, std::uint16_t port, const char* what) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    throw std::invalid_argument(std::string("UdpSocket::") + what +
                                ": not a numeric IPv4 address: '" + host + "'");
  }
  return addr;
}

}  // namespace

UdpSocket::UdpSocket() {
  fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    throw std::runtime_error(std::string("UdpSocket: socket() failed: ") + std::strerror(errno));
  }
}

UdpSocket::~UdpSocket() { closeFd(); }

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : fd_(other.fd_), boundPort_(other.boundPort_), transientErrors_(other.transientErrors_) {
  other.fd_ = -1;
  other.boundPort_ = 0;
  other.transientErrors_ = 0;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
  if (this != &other) {
    closeFd();
    fd_ = other.fd_;
    boundPort_ = other.boundPort_;
    transientErrors_ = other.transientErrors_;
    other.fd_ = -1;
    other.boundPort_ = 0;
    other.transientErrors_ = 0;
  }
  return *this;
}

void UdpSocket::closeFd() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

void UdpSocket::bind(const std::string& host, std::uint16_t port) {
  if (fd_ < 0) {
    throw std::runtime_error("UdpSocket::bind: socket is not open (moved-from?)");
  }
  const sockaddr_in addr = makeAddr(host, port, "bind");
  if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
    throw std::runtime_error("UdpSocket::bind(" + host + ":" + std::to_string(port) +
                             ") failed: " + std::strerror(errno));
  }
  sockaddr_in actual{};
  socklen_t len = sizeof(actual);
  if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&actual), &len) == 0) {
    boundPort_ = ntohs(actual.sin_port);
  } else {
    boundPort_ = port;
  }
}

std::optional<std::string> UdpSocket::recvFrom(std::chrono::milliseconds timeout) {
  if (fd_ < 0) {
    throw std::runtime_error("UdpSocket::recvFrom: socket is not open (moved-from?)");
  }
  pollfd pfd{};
  pfd.fd = fd_;
  pfd.events = POLLIN;
  const int ready = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
  if (ready == 0) {
    return std::nullopt;  // timeout — not an error
  }
  if (ready < 0) {
    ++transientErrors_;  // EINTR and friends — counted, never thrown
    return std::nullopt;
  }
  std::vector<char> buffer(kMaxDatagramBytes);
  const ssize_t received = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, nullptr, nullptr);
  if (received < 0) {
    ++transientErrors_;
    return std::nullopt;
  }
  return std::string(buffer.data(), static_cast<std::size_t>(received));
}

bool UdpSocket::sendTo(const std::string& host, std::uint16_t port, const std::string& bytes) {
  if (fd_ < 0) {
    throw std::runtime_error("UdpSocket::sendTo: socket is not open (moved-from?)");
  }
  const sockaddr_in addr = makeAddr(host, port, "sendTo");
  const ssize_t sent = ::sendto(fd_, bytes.data(), bytes.size(), 0,
                                reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  if (sent < 0 || static_cast<std::size_t>(sent) != bytes.size()) {
    ++transientErrors_;
    return false;
  }
  return true;
}

}  // namespace ada::net
