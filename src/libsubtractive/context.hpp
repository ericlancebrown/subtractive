#pragma once

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "libsubtractive/actor.hpp"
#include "libsubtractive/communication/usb/hotplug.hpp"
#include "libsubtractive/communication/zmq/zeromq_wrapper.hpp"
#include "libsubtractive/machine.hpp"  // IWYU pragma: keep

extern "C" {
struct LS_options;
}

namespace libsubtractive
{
struct ZMQParent {
    zmq::Context zmq_context_{};
};

class Context final : ZMQParent, Actor<Context>
{
public:
    static std::atomic<Context*> singleton_;

    operator void*() noexcept { return zeromq_; }

    Context(const LS_options& options);

    ~Context();

private:
    friend Actor<Context>;

    using DeviceID = std::string;
    using DeviceMap = std::map<DeviceID, std::pair<zmq::Socket, Machine>>;
    using SubscriberID = std::vector<std::byte>;
    using DeviceSubscribers = boost::container::flat_set<SubscriberID>;
    using MachineSubscribers = boost::container::
        flat_map<DeviceID, boost::container::flat_set<SubscriberID>>;

    enum class Operation : std::int8_t { Remove = -1, Add = 0, MustExist = 1 };

    const zmq::Socket& router_;
    Hotplug hotplug_;
    DeviceMap devices_;
    DeviceSubscribers device_subscribers_;
    MachineSubscribers machine_subscribers_;
    std::vector<DeviceMap::iterator> recognized_devices_;

    auto command_list_devices(zmq::Message&& in) noexcept -> void;
    auto command_subscribe(zmq::Message&& in) noexcept -> void;
    auto command_support_device(zmq::Message&& in) noexcept -> void;
    auto command_support_device(const std::string_view id) noexcept -> void;
    auto command_unsubscribe(zmq::Message&& in) noexcept -> void;
    auto command_usb_device_added(zmq::Message&& in) noexcept -> void;
    auto command_usb_device_removed(zmq::Message&& in) noexcept -> void;
    auto forward_to_machine(zmq::Message&& in) noexcept -> void;
    auto forward_to_subscriber(
        const std::string& machineID,
        zmq::Message&& in) noexcept -> void;
    auto find_or_create(
        const std::string_view serialNum,
        const std::string_view endpoint,
        const Operation op) noexcept -> DeviceMap::iterator;
    auto process_command(zmq::Message&& command) noexcept -> bool;

    Context() = delete;
};
}  // namespace libsubtractive
