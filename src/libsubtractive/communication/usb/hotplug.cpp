#include "libsubtractive/communication/usb/hotplug.hpp"  // IWYU pragma: associated

#include <libusbp-1/libusbp.hpp>
#include <zmq.h>
#include <chrono>
#include <type_traits>
#include <utility>
#include <vector>

namespace libsubtractive
{
constexpr auto MaxUSBInterfaces = std::uint8_t{1};

Hotplug::Hotplug(const zmq::Context& zeromq, const bool enabled)
    : zeromq_(zeromq)
    , socket_(zeromq_.Socket(ZMQ_PUSH, Direction::Connect, ContextEndpoint()))
    , running_(enabled)
    , device_list_()
    , thread_([this] { thread(); })
{
}

auto Hotplug::enumerate_usb() noexcept -> void
{
    const auto devices = libusbp::list_connected_devices();
    auto map = DeviceMap{};

    for (const auto& device : devices) {
        const auto addr = get_usb_address(device);

        auto success{false};

        for (auto i = std::uint8_t{0}; i < MaxUSBInterfaces; ++i) {
            if (get_port_path(device, i, true, addr, map)) { break; }
        }

        if (!success) { get_port_path(device, 0, false, addr, map); }
    }

    for (const auto& [addr, port] : map) {
        if (0 == device_list_.count(addr)) {
            auto command = zeromq_.Command(Command::USBDeviceAdded);
            command.emplace_back(addr.data(), addr.size());
            command.emplace_back(port.data(), port.size());
            socket_.send(std::move(command));
        }
    }

    for (const auto& [addr, port] : device_list_) {
        if (0 == map.count(addr)) {
            auto command = zeromq_.Command(Command::USBDeviceRemoved);
            command.emplace_back(addr.data(), addr.size());
            command.emplace_back(port.data(), port.size());
            socket_.send(std::move(command));
        }
    }

    device_list_.swap(map);
}

auto Hotplug::get_usb_address(const libusbp::device& device) noexcept
    -> std::string
{
    try {
        return device.get_serial_number();
    } catch (...) {
        return {};
    }
}

auto Hotplug::get_port_path(
    const libusbp::device& device,
    std::uint8_t interface,
    bool composite,
    const std::string address,
    DeviceMap& devices) noexcept -> bool
{
    try {
        devices.emplace(
            address,
            libusbp::serial_port{device, interface, composite}.get_name());
    } catch (...) {
        return false;
    }

    return true;
}

auto Hotplug::thread() noexcept -> void
{
    while (running_) {
        enumerate_usb();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

Hotplug::~Hotplug()
{
    running_ = false;

    if (thread_.joinable()) { thread_.join(); }
}
}  // namespace libsubtractive
