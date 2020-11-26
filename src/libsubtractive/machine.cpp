#include "libsubtractive/machine.hpp"  // IWYU pragma: associated

#include <zmq.h>
#include <cstdlib>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include "libsubtractive/communication/zmq/zeromq_wrapper.hpp"
#include "libsubtractive/protocol/Grbl.hpp"

auto make_realtime(const unsigned char in) noexcept -> std::string;
auto make_realtime(const unsigned char in) noexcept -> std::string
{
    return std::string{reinterpret_cast<const char*>(&in), sizeof(in)};
}

const std::map<libsubtractive::Command, std::string> commands_{
    {libsubtractive::Command::GrblHelp, "$\n"},
    {libsubtractive::Command::GrblStatus, make_realtime('?')},
    {libsubtractive::Command::GrblSettings, "$$\n"},
    {libsubtractive::Command::GrblVersion, "$I\n"},
    {libsubtractive::Command::GrblHome, "$H\n"},
    {libsubtractive::Command::GrblParams, "$#\n"},
    {libsubtractive::Command::GrblParserState, "$G\n"},
    {libsubtractive::Command::GrblStartupBlocks, "$N\n"},
    {libsubtractive::Command::GrblCheckModeToggle, "$C\n"},
    {libsubtractive::Command::GrblResetAlarm, "$X\n"},
    {libsubtractive::Command::GrblSoftReset, make_realtime(30)},
    {libsubtractive::Command::GrblCycleToggle, make_realtime('~')},
    {libsubtractive::Command::GrblFeedHold, make_realtime('!')},
    {libsubtractive::Command::GrblJogCancel, make_realtime(133)},
};

namespace libsubtractive
{
Machine::Machine(
    const zmq::Context& zeromq,
    const std::string_view serial,
    const std::string_view endpoint,
    const bool enableSerialPort,
    const std::string serialEndpoint,
    const std::string flowEndpoint)
    : Actor(
          zeromq,
          [&]() -> auto {
              auto output = Sockets{};
              output.emplace_back(
                  zeromq.Socket(ZMQ_PAIR, Direction::Connect, endpoint));
              output.emplace_back(
                  zeromq.Socket(ZMQ_PAIR, Direction::Bind, flowEndpoint));

              return output;
          })
    , usb_address_(serial)
    , parent_socket_(sockets_.at(0))
    , flow_control_socket_(sockets_.at(1))
    , type_(MachineType::Unknown)
    , version_()
    , state_(State::Disconnected)
    , flow_control_(zeromq_, usb_address_, serialEndpoint, flowEndpoint)
    , connection_(zeromq_, serialEndpoint, enableSerialPort)
    , grbl_version_()
    , message_id_(-1)
{
    init_actor();
}

auto Machine::command_init_grbl(zmq::Message&& in) noexcept -> void
{
    if (4 > in.arg_count()) { abort(); }

    grbl_version_ = grbl::VersionData{
        in.arg(1).as<grbl::VersionIndex>(),
        in.arg(2).as<grbl::VersionIndex>(),
        in.arg(3).as<grbl::Subversion>()};
    const auto& [major, minor, sub] = grbl_version_;

    if ((0 == major) && (9 > minor)) { return; }  // unsupported version

    state_ = State::Grbl;
    enable_flow_control();

    {
        constexpr auto command{Command::GrblVersion};
        auto message = zeromq_.Command(command);
        message.emplace_back();
        const auto& text = commands_.at(command);
        message.emplace_back(text.data(), text.size());
        forward_grbl(std::move(message));
    }
}

auto Machine::command_push_received(zmq::Message&& in) noexcept -> void
{
    // TODO it might be important so parse it
    parent_socket_.send(std::move(in));
}

auto Machine::command_response_received(zmq::Message&& in) noexcept -> void
{
    switch (state_) {
        case State::Disconnected:
        case State::Connected: {
            break;
        }
        case State::Grbl: {
            process_response_version(std::move(in));
        } break;
        case State::Identified:
        default: {
            process_response(std::move(in));
        }
    }
}

auto Machine::command_usb_device_added(zmq::Message&& in) noexcept -> void
{
    state_ = State::Connected;
    flow_control_socket_.send(std::move(in));
}

auto Machine::command_usb_device_removed(zmq::Message&& in) noexcept -> void
{
    state_ = State::Disconnected;
    flow_control_socket_.send(std::move(in));
}

auto Machine::Describe(zmq::Message& out) const noexcept -> void
{
    auto text = std::stringstream{};

    switch (type_) {
        case MachineType::GhostGunner: {
            text << "Ghost Gunner " << version_ << " (" << usb_address_ << ')';
        } break;
        case MachineType::Unknown:
        default: {
            const auto& [major, minor, patch] = grbl_version_;
            text << "Generic Grbl " << std::to_string(major) << '.'
                 << std::to_string(minor) << patch << " device ("
                 << usb_address_ << ')';
        }
    }

    const auto string = text.str();
    out.emplace_back(usb_address_.data(), usb_address_.size());
    out.emplace_back(string.data(), string.size());
}

auto Machine::enable_flow_control() const noexcept -> void
{
    flow_control_socket_.send(zeromq_.Command(Command::EnableFlowControl));
}

auto Machine::forward_grbl(zmq::Message&& in) noexcept -> void
{
    if (State::Grbl > state_) {
        // TODO reply with failure message

        return;
    }

    in.emplace_back(++message_id_);
    flow_control_socket_.send(std::move(in));
}

auto Machine::process_command(zmq::Message&& command) noexcept -> bool
{
    auto disconnectAfter{false};

    switch (command.type()) {
        case Command::Shutdown: {
            disconnectAfter = true;
        } break;
        case Command::InitGrbl: {
            command_init_grbl(std::move(command));
        } break;
        case Command::SendGcode: {
            forward_grbl(std::move(command));
        } break;
        case Command::USBDeviceAdded: {
            command_usb_device_added(std::move(command));
        } break;
        case Command::USBDeviceRemoved: {
            command_usb_device_removed(std::move(command));
        } break;
        case Command::ResponseReceived: {
            command_response_received(std::move(command));
        } break;
        case Command::GrblPushReceived: {
            command_push_received(std::move(command));
        } break;
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
            const auto text = commands_.at(command.type());
            command.emplace_back(text.data(), text.size());
            forward_grbl(std::move(command));
        } break;
        case Command::Invalid:
        case Command::ListDevices:
        case Command::Subscribe:
        case Command::Unsubscribe:
        case Command::ExecuteProgram:
        case Command::PushDeviceRemoved:
        case Command::PushDeviceAdded:
        case Command::ListDevicesReply:
        case Command::DeviceIsSupported:
        case Command::EnableFlowControl:
        case Command::DataReceived:
        default: {
            abort();
        }
    }

    return disconnectAfter;
}

auto Machine::process_response(zmq::Message&& response) -> void
{
    if (3 > response.arg_count()) { abort(); }

    switch (response.arg(1).as<Command>()) {
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
            parent_socket_.send(std::move(response));
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
        case Command::DataReceived:
        case Command::InitGrbl:
        case Command::USBDeviceRemoved:
        case Command::USBDeviceAdded:
        case Command::Shutdown:
        default: {
            abort();
        }
    }
}

auto Machine::process_response_version(zmq::Message&& response) -> void
{
    if (4 > response.arg_count()) { abort(); }

    const auto gg2 = std::regex{"DD ([0-9,a-z,A-Z]+)"};
    const auto gg3 = std::regex{"GG:([0-9,a-z,A-Z]+)"};
    const auto ver = std::string{response.arg(3).str()};
    auto matchGG2 = std::smatch{};
    auto matchGG3 = std::smatch{};

    if (std::regex_search(ver, matchGG2, gg2)) {
        type_ = MachineType::GhostGunner;
        version_ = matchGG2[1];
    } else if (std::regex_search(ver, matchGG3, gg3)) {
        type_ = MachineType::GhostGunner;
        version_ = matchGG3[1];
    } else {
        type_ = MachineType::Unknown;
        version_ = ver;
    }

    if (state_ < State::Identified) {
        auto message = zeromq_.Command(Command::DeviceIsSupported);
        message.emplace_back(usb_address_.data(), usb_address_.size());
        parent_socket_.send(std::move(message));
    }

    state_ = State::Identified;
}

Machine::~Machine() { shutdown_actor(); }
}  // namespace libsubtractive
