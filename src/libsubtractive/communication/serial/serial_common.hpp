#pragma once

#include "serial.hpp"  // IWYU pragma: associated

#include <zmq.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "libsubtractive/actor.hpp"
#include "libsubtractive/communication/zmq/zeromq_wrapper.hpp"

namespace libsubtractive
{
struct SerialConnection::Imp : Actor<SerialConnection::Imp> {
    static auto Factory(
        const zmq::Context& zeromq,
        const std::string_view endpoint,
        const bool enabled) noexcept -> std::unique_ptr<Imp>;

    virtual auto connect(const std::string_view path) -> void = 0;
    virtual auto disconnect() -> void = 0;
    virtual auto transmit(const std::string_view data) -> void = 0;

    virtual ~Imp() = default;

protected:
    const zmq::Socket& internal_push_;

    auto shutdown() noexcept -> void
    {
        shutdown_actor();
        disconnect();
    }

    Imp(const zmq::Context& zeromq,
        const std::string_view endpoint,
        const bool enabled)
        : Actor(
              zeromq,
              [&]() -> auto {
                  if (false == enabled) { return Sockets{}; }

                  const auto internal = RandomEndpoint();
                  auto output = Sockets{};
                  output.emplace_back(
                      zeromq_.Socket(ZMQ_PAIR, Direction::Connect, endpoint));
                  output.emplace_back(
                      zeromq_.Socket(ZMQ_PULL, Direction::Bind, internal));
                  output.emplace_back(
                      zeromq_.Socket(ZMQ_PUSH, Direction::Connect, internal));

                  return output;
              })
        , internal_push_(enabled ? sockets_.at(2) : null_socket(zeromq_))
        , internal_pull_(enabled ? sockets_.at(1) : null_socket(zeromq_))
        , parent_socket_(enabled ? sockets_.at(0) : null_socket(zeromq_))
    {
        init_actor();
    }

private:
    friend Actor<SerialConnection::Imp>;

    static auto null_socket(const zmq::Context& zeromq) noexcept
        -> const zmq::Socket&
    {
        static const auto null = zeromq.Socket(
            ZMQ_PAIR, Direction::Bind, "inproc://null_serial_socket");

        return null;
    }

    const zmq::Socket& internal_pull_;
    const zmq::Socket& parent_socket_;

    auto command_data_received(zmq::Message&& in) noexcept -> void
    {
        parent_socket_.send(std::move(in));
    }
    auto command_send_message(zmq::Message&& in) noexcept -> void
    {
        if (2 > in.arg_count()) { return; }

        transmit(in.arg(1).str());
    }
    auto command_usb_device_added(zmq::Message&& in) noexcept -> void
    {
        if (2 > in.arg_count()) { return; }

        const auto serial = in.arg(0).str();
        const auto port = in.arg(1).str();

        connect(port);
        std::cout << "Device " << serial << " connected via: " << port << '\n';
    }
    auto command_usb_device_removed(zmq::Message&& in) noexcept -> void
    {
        if (2 > in.arg_count()) { return; }

        const auto serial = in.arg(0).str();
        const auto port = in.arg(1).str();

        disconnect();
        std::cout << "Device " << serial << " no longer available via: " << port
                  << '\n';
    }
    auto process_command(zmq::Message&& command) noexcept -> bool
    {
        auto disconnectAfter{false};

        switch (command.type()) {
            case Command::Shutdown: {
                disconnectAfter = true;
            } break;
            case Command::DataReceived: {
                command_data_received(std::move(command));
            } break;
            case Command::USBDeviceAdded: {
                command_usb_device_added(std::move(command));
            } break;
            case Command::USBDeviceRemoved: {
                command_usb_device_removed(std::move(command));
            } break;
            case Command::SendGcode:
            case Command::GrblHelp:
            case Command::GrblStatus:
            case Command::GrblSettings:
            case Command::GrblVersion:
            case Command::GrblHome:
            case Command::GrblParams:
            case Command::GrblParserState:
            case Command::GrblStartupBlocks:
            case Command::GrblCheckModeToggle:
            case Command::GrblResetAlarm:
            case Command::GrblSoftReset:
            case Command::GrblCycleToggle:
            case Command::GrblFeedHold:
            case Command::GrblJogCancel: {
                command_send_message(std::move(command));
            } break;
            case Command::Invalid:
            case Command::ListDevices:
            case Command::Subscribe:
            case Command::Unsubscribe:
            case Command::ExecuteProgram:
            case Command::PushDeviceRemoved:
            case Command::PushDeviceAdded:
            case Command::ListDevicesReply:
            case Command::ResponseReceived:
            case Command::GrblPushReceived:
            case Command::DeviceIsSupported:
            case Command::EnableFlowControl:
            case Command::InitGrbl:
            default: {
                abort();
            }
        }

        return disconnectAfter;
    }
};

SerialConnection::SerialConnection(
    const zmq::Context& zeromq,
    const std::string_view endpoint,
    const bool enabled)
    : imp_(Imp::Factory(zeromq, endpoint, enabled))
{
    if (!imp_) {
        throw std::runtime_error("Failed to initialize SerialConnection");
    }
}

SerialConnection::~SerialConnection() = default;
}  // namespace libsubtractive
