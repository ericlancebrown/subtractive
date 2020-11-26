#include "libsubtractive/context.hpp"  // IWYU pragma: associated

#include <zmq.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include "libsubtractive/libsubtractive.hpp"

std::mutex init_mutex_{};

extern "C" {
LS_options libsubtractive_default_options()
{
    auto output = LS_options{};
    output.init_usb_ = true;

    return output;
}
const char* libsubtractive_endpoint()
{
    return libsubtractive::ContextEndpoint().c_str();
}
void* libsubtractive_init_context(const LS_options* options)
{
    std::lock_guard<std::mutex> lock(init_mutex_);
    auto& singleton = libsubtractive::Context::singleton_;

    if (nullptr == singleton.load()) {
        static const auto defaultOpts = libsubtractive_default_options();

        auto opt = (nullptr == options) ? &defaultOpts : options;

        singleton.store(new libsubtractive::Context(*opt));
    }

    return *singleton.load();
}

void libsubtractive_close_context()
{
    std::lock_guard<std::mutex> lock(init_mutex_);
    auto context = libsubtractive::Context::singleton_.exchange(nullptr);

    if (nullptr != context) { delete context; }
}
}

namespace libsubtractive
{
std::atomic<Context*> Context::singleton_{};

Context::Context(const LS_options& options)
    : ZMQParent()
    , Actor(
          zmq_context_,
          [&]() -> auto {
              auto output = Sockets{};
              output.emplace_back(zeromq_.Socket(
                  ZMQ_ROUTER, Direction::Bind, ContextEndpoint()));

              return output;
          })
    , router_(sockets_.at(0))
    , hotplug_(zeromq_, options.init_usb_)
    , devices_()
    , device_subscribers_()
    , machine_subscribers_()
    , recognized_devices_()
{
    init_actor();
}

auto Context::command_list_devices(zmq::Message&& in) noexcept -> void
{
    device_subscribers_.emplace(in.identity());
    auto reply = zmq_context_.Response(in);
    reply.emplace_back();
    reply.emplace_back(Command::ListDevicesReply);

    for (const auto& device : recognized_devices_) {
        const auto& [key, value] = *device;
        value.second.Describe(reply);
    }

    router_.send(std::move(reply));
}

auto Context::command_subscribe(zmq::Message&& in) noexcept -> void
{
    for (auto i = std::size_t{0}; i < in.arg_count(); ++i) {
        const auto address = std::string{in.arg(i).str()};

        if (0u == i) {
            machine_subscribers_[address].emplace(in.identity());
        } else {
            machine_subscribers_[address].erase(in.identity());
        }
    }
}

auto Context::command_support_device(const std::string_view id) noexcept -> void
{
    if ((nullptr == id.data()) || (0 == id.size())) { abort(); }

    auto endpoint = std::string{};
    auto it = find_or_create(id, endpoint, Operation::MustExist);
    const auto sort = [this](const auto& lhs, const auto& rhs) -> auto
    {
        return std::distance(devices_.begin(), lhs) <
               std::distance(devices_.begin(), rhs);
    };

    auto& vector = recognized_devices_;
    vector.emplace_back(it);
    std::sort(vector.begin(), vector.end(), sort);
    vector.erase(std::unique(vector.begin(), vector.end()), vector.end());
}

auto Context::command_support_device(zmq::Message&& in) noexcept -> void
{
    if (1 > in.arg_count()) { abort(); }

    const auto& address = in.arg(0);
    const auto addressV = address.str();
    auto endpoint = std::string{};
    auto it = find_or_create(addressV, endpoint, Operation::MustExist);
    const auto sort = [this](const auto& lhs, const auto& rhs) -> auto
    {
        return std::distance(devices_.begin(), lhs) <
               std::distance(devices_.begin(), rhs);
    };

    auto& vector = recognized_devices_;
    vector.emplace_back(it);
    std::sort(vector.begin(), vector.end(), sort);
    vector.erase(std::unique(vector.begin(), vector.end()), vector.end());
    auto& [socket, device] = it->second;

    for (const auto& id : device_subscribers_) {
        auto push = zmq::Message::MakePush(id, Command::PushDeviceAdded);
        device.Describe(push);
        router_.send(std::move(push));
    }
}

auto Context::command_unsubscribe(zmq::Message&& in) noexcept -> void
{
    for (auto i = std::size_t{0}; i < in.arg_count(); ++i) {
        const auto machineID = std::string{in.arg(i).str()};
        machine_subscribers_[machineID].erase(in.identity());
    }
}

auto Context::command_usb_device_added(zmq::Message&& in) noexcept -> void
{
    if (2 > in.arg_count()) { abort(); }

    const auto& address = in.arg(0);
    const auto addressV = address.str();
    auto endpoint = std::string{};

    if (2 < in.arg_count()) { endpoint = in.arg(2).str(); }

    auto& [socket, device] =
        find_or_create(addressV, endpoint, Operation::Add)->second;
    socket.send(std::move(in));
}

auto Context::command_usb_device_removed(zmq::Message&& in) noexcept -> void
{
    if (2 > in.arg_count()) { abort(); }

    const auto& address = in.arg(0);
    const auto addressV = address.str();
    auto endpoint = std::string{};

    if (2 < in.arg_count()) { endpoint = in.arg(2).str(); }

    for (const auto& id : device_subscribers_) {
        auto push = zmq::Message::MakePush(id, Command::PushDeviceRemoved);
        push.emplace_back(address);
        router_.send(std::move(push));
    }

    auto& [socket, device] =
        find_or_create(addressV, endpoint, Operation::Remove)->second;
    socket.send(std::move(in));
}

auto Context::forward_to_machine(zmq::Message&& in) noexcept -> void
{
    if (1 > in.arg_count()) { abort(); }

    const auto address = std::string{in.arg(0).str()};
    machine_subscribers_[address].emplace(in.identity());

    try {
        const auto& [socket, machine] = devices_.at(address);
        socket.send(std::move(in));
    } catch (...) {
        std::cout << "Unknown device: " << address << '\n';
    }
}

auto Context::forward_to_subscriber(
    const std::string& machineID,
    zmq::Message&& in) noexcept -> void
{
    try {
        const auto& subscribers = machine_subscribers_.at(machineID);

        for (const auto& id : subscribers) {
            auto push = zmq::Message::MakePush(id, in.type());
            const auto stop{in.arg_count()};

            for (auto i = std::size_t{0}; i < stop; ++i) {
                push.emplace_back(in.arg(i));
            }

            router_.send(std::move(push));
        }
    } catch (...) {
    }
}

auto Context::find_or_create(
    const std::string_view address,
    const std::string_view port,
    const Operation op) noexcept -> DeviceMap::iterator
{
    auto output = devices_.find(std::string{address});

    if (devices_.end() != output) {
        if (Operation::Remove == op) {
            recognized_devices_.erase(
                std::remove(
                    recognized_devices_.begin(),
                    recognized_devices_.end(),
                    output),
                recognized_devices_.end());
        }

        return output;
    } else if (Operation::MustExist == op) {
        abort();
    }

    const auto useProvided = (nullptr != port.data()) && (0 < port.size());
    const auto internal = RandomEndpoint();
    auto [it, added] = devices_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(address),
        std::forward_as_tuple(
            std::piecewise_construct,
            std::forward_as_tuple(
                zeromq_.Socket(ZMQ_PAIR, Direction::Bind, internal)),
            std::forward_as_tuple(
                zeromq_,
                address,
                internal,
                !useProvided,
                useProvided ? std::string{port} : RandomEndpoint())));

    assert(added);

    auto& [socket, device] = it->second;

    assert(nullptr != socket);

    auto& poll = new_poll_items_.emplace_back();
    poll.socket = socket;
    poll.events = ZMQ_POLLIN;

    return it;
}

auto Context::process_command(zmq::Message&& command) noexcept -> bool
{
    auto disconnectAfter{false};

    switch (command.type()) {
        case Command::Shutdown: {
            disconnectAfter = true;
        } break;
        case Command::ListDevices: {
            command_list_devices(std::move(command));
        } break;
        case Command::Subscribe: {
            command_subscribe(std::move(command));
        } break;
        case Command::Unsubscribe: {
            command_unsubscribe(std::move(command));
        } break;
        case Command::USBDeviceAdded: {
            command_usb_device_added(std::move(command));
        } break;
        case Command::USBDeviceRemoved: {
            command_usb_device_removed(std::move(command));
        } break;
        case Command::DeviceIsSupported: {
            command_support_device(std::move(command));
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
        case Command::GrblJogCancel:
        case Command::SendGcode: {
            forward_to_machine(std::move(command));
        } break;
        case Command::GrblPushReceived:
        case Command::ResponseReceived: {
            assert(1 <= command.arg_count());

            forward_to_subscriber(
                std::string{command.arg(0).str()}, std::move(command));
        } break;
        case Command::Invalid:
        case Command::ExecuteProgram:
        case Command::PushDeviceRemoved:
        case Command::PushDeviceAdded:
        case Command::ListDevicesReply:
        case Command::EnableFlowControl:
        case Command::DataReceived:
        case Command::InitGrbl:
        default: {
            std::cout << "Unsupported command: "
                      << std::to_string(
                             static_cast<std::uint8_t>(command.type()))
                      << '\n';
        }
    }

    return disconnectAfter;
}

Context::~Context() { shutdown_actor(); }
}  // namespace libsubtractive
