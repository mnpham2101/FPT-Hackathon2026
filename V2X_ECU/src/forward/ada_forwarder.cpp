// Implementation of the 2.1.2.4 ADA forwarder — see ada_forwarder.hpp for
// the contract (one compact-JSON datagram per R2 message, never-throws send).

#include "forward/ada_forwarder.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <utility>

#include <nlohmann/json.hpp>

namespace v2x::forward {

AdaForwarder::AdaForwarder(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

bool AdaForwarder::send(const v2x::contracts::R2Message& message) {
  try {
    const nlohmann::json document = message;
    const std::string body = document.dump();  // compact — one datagram per message
    socket_.sendTo(host_, port_, reinterpret_cast<const std::uint8_t*>(body.data()),
                   body.size());
    return true;
  } catch (const v2x::net::SocketError& error) {
    std::cerr << "[FWD-ERR] R2 forward to " << host_ << ":" << port_
              << " failed: " << error.what() << "\n";
    return false;
  } catch (const std::exception& error) {
    // Defensive net for the never-throws contract (e.g. nlohmann dump()
    // rejecting invalid UTF-8 in a string field) — same failure surface.
    std::cerr << "[FWD-ERR] R2 serialization failed: " << error.what() << "\n";
    return false;
  }
}

}  // namespace v2x::forward
