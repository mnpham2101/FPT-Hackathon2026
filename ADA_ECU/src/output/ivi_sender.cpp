#include "output/ivi_sender.hpp"

#include <exception>
#include <utility>

#include <nlohmann/json.hpp>

namespace ada::output {

IviSender::IviSender(net::UdpSocket& socket, std::string host, std::uint16_t port,
                     log::EventLog& eventLog)
    : socket_(socket), host_(std::move(host)), port_(port), eventLog_(eventLog) {}

bool IviSender::send(const contracts::R4WarningEvent& event) {
  const nlohmann::json body = event;  // the frozen binding is the only serializer (D7)
  const std::string bytes = body.dump();

  bool sendOk = false;
  try {
    sendOk = socket_.sendTo(host_, port_, bytes);
  } catch (const std::exception&) {
    // The socket throws on a malformed address (a setup error by its
    // contract); at this seam it is one more counted failure — egress never
    // throws into the fusion tick (header comment).
    sendOk = false;
  }
  if (!sendOk) {
    ++failures_;
  }

  // The designated r4_tx payload: the full body as a JSON OBJECT (not a
  // string), the serialized length, "<host>:<port>", and the outcome.
  eventLog_.emit(log::events::kR4Tx, {{"body", body},
                                      {"bytes", static_cast<std::int64_t>(bytes.size())},
                                      {"dest", host_ + ":" + std::to_string(port_)},
                                      {"send_ok", sendOk}});
  return sendOk;
}

}  // namespace ada::output
