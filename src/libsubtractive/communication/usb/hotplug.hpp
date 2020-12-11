#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <thread>

#include "libsubtractive/communication/zmq/zeromq_wrapper.hpp"

namespace libusbp
{
class device;
}  // namespace libusbp

namespace libsubtractive
{
class Hotplug
{
public:
    Hotplug(const zmq::Context& zeromq, const bool enabled);

    ~Hotplug();

private:
    using SerialPortPath = std::string;
    using DeviceSerialNumber = std::string;
    using DeviceMap = std::map<DeviceSerialNumber, SerialPortPath>;

    const zmq::Context& zeromq_;
    zmq::Socket socket_;
    std::atomic_bool running_;
    DeviceMap device_list_;
    std::thread thread_;

    static auto get_usb_address(const libusbp::device& device) noexcept
        -> std::string;
    static auto get_port_path(
        const libusbp::device& device,
        std::uint8_t interface,
        const bool composite,
        const std::string address,
        DeviceMap& devices) noexcept -> bool;

    auto enumerate_usb() noexcept -> void;
    auto thread() noexcept -> void;
};
}  // namespace libsubtractive
