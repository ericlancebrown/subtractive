#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "libsubtractive/actor.hpp"
#include "libsubtractive/communication/flowcontrol.hpp"
#include "libsubtractive/communication/serial/serial.hpp"
#include "libsubtractive/communication/zmq/zeromq_wrapper.hpp"  // IWYU pragma: keep

namespace libsubtractive
{
class Machine final : Actor<Machine>
{
public:
    enum class State : std::uint8_t {
        Disconnected = 0,
        Connected = 1,
        Grbl = 2,
        Identified = 3,
    };

    auto Describe(zmq::Message& out) const noexcept -> void;

    Machine(
        const zmq::Context& zeromq,
        const std::string_view serial,
        const std::string_view endpoint,
        const bool enableSerialPort = true,
        const std::string serialEndpoint = RandomEndpoint(),
        const std::string flowEndpoint = RandomEndpoint());

    ~Machine();

private:
    friend Actor<Machine>;

    enum class MachineType {
        Unknown,
        GhostGunner,
    };

    const std::string usb_address_;
    const zmq::Socket& parent_socket_;
    const zmq::Socket& flow_control_socket_;
    MachineType type_;
    std::string version_;
    State state_;
    FlowControl flow_control_;
    SerialConnection connection_;
    grbl::VersionData grbl_version_;
    int message_id_;

    static auto init_sockets(const std::string_view parent) -> Sockets;

    auto command_init_grbl(zmq::Message&& in) noexcept -> void;
    auto command_push_received(zmq::Message&& in) noexcept -> void;
    auto command_response_received(zmq::Message&& in) noexcept -> void;
    auto command_usb_device_added(zmq::Message&& in) noexcept -> void;
    auto command_usb_device_removed(zmq::Message&& in) noexcept -> void;
    auto forward_grbl(zmq::Message&& in) noexcept -> void;
    auto process_command(zmq::Message&& command) noexcept -> bool;
    auto process_response(zmq::Message&& response) -> void;
    auto process_response_version(zmq::Message&& response) -> void;

    auto enable_flow_control() const noexcept -> void;
};
}  // namespace libsubtractive
