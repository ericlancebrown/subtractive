#pragma once

#include <zmq.h>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "libsubtractive/libsubtractive.hpp"

namespace libsubtractive
{
namespace zmq
{
class Message;
class Socket;
}  // namespace zmq

auto ContextEndpoint() noexcept -> const std::string&;
auto DeviceEndpoint(const std::string_view serial) noexcept -> std::string;
auto RandomEndpoint() noexcept -> std::string;

enum class Command : std::uint8_t {
    Invalid = 0,
    ListDevices = LS_LISTDEVICES,
    Subscribe = LS_SUBSCRIBE,
    Unsubscribe = LS_UNSUBSCRIBE,
    SendGcode = LS_SENDGCODE,
    ExecuteProgram = LS_EXECUTE_PROGRAM,
    GrblHelp = LS_GRBLHELP,
    GrblStatus = LS_GRBLSTATUS,
    GrblSettings = LS_GRBLSETTINGS,
    GrblVersion = LS_GRBLVERSION,
    GrblHome = LS_GRBLHOME,
    GrblParams = LS_GRBLPARAMS,
    GrblParserState = LS_GRBLPARSERSTATE,
    GrblStartupBlocks = LS_GRBLSTARTUPBLOCKS,
    GrblCheckModeToggle = LS_GRBLCHECKMODETOGGLE,
    GrblResetAlarm = LS_GRBLRESETALARM,
    GrblSoftReset = LS_GRBLSOFTRESET,
    GrblCycleToggle = LS_GRBLCYCLETOGGLE,
    GrblFeedHold = LS_GRBLFEEDHOLD,
    GrblJogCancel = LS_GRBLJOGCANCEL,
    PushDeviceRemoved = LS_DEVICEREMOVED,
    PushDeviceAdded = LS_DEVICEADDED,
    ListDevicesReply = LS_LISTDEVICES_REPLY,
    ResponseReceived = LS_RESPONSERECEIVED,
    GrblPushReceived = 248,
    DeviceIsSupported = 249,
    EnableFlowControl = 250,
    DataReceived = 251,
    InitGrbl = 252,
    USBDeviceRemoved = 253,
    USBDeviceAdded = 254,
    Shutdown = 255,
};

enum class Direction : bool { Connect = false, Bind = true };
}  // namespace libsubtractive

namespace libsubtractive::zmq
{
class Context
{
public:
    operator void*() const noexcept { return data_; }

    auto Command(const libsubtractive::Command type) const noexcept
        -> zmq::Message;
    auto Response(const Message& request) const noexcept -> zmq::Message;
    auto Socket(const int type) const noexcept -> zmq::Socket;
    auto Socket(
        const int type,
        const Direction direction,
        const std::string_view endpoint) const -> zmq::Socket;

    Context()
        : data_(zmq_ctx_new())
    {
        if (nullptr == data_) {
            throw std::runtime_error("Failed to initialize zmq context");
        }
    }

    ~Context() { zmq_ctx_shutdown(data_); }

private:
    void* data_;

    Context(const Context&) = delete;
    Context(Context&&) = delete;
    auto operator=(const Context&) -> Context& = delete;
    auto operator=(Context &&) -> Context& = delete;
};

class Frame
{
public:
    operator zmq_msg_t*() noexcept { return &data_; }

    template <
        typename Output,
        std::enable_if_t<std::is_trivially_copyable<Output>::value, int> = 0>
    auto as() const noexcept(false) -> Output
    {
        if (sizeof(Output) != size()) {
            throw std::runtime_error("Invalid frame");
        }

        Output output{};
        std::memcpy(&output, data(), sizeof(output));

        return output;
    }
    auto data() const noexcept -> const void*
    {
        return const_cast<Frame*>(this)->data();
    }
    auto size() const noexcept -> std::size_t { return zmq_msg_size(&data_); }
    auto str() const noexcept -> std::string_view
    {
        return {static_cast<const char*>(data()), size()};
    }

    auto data() noexcept -> void* { return zmq_msg_data(&data_); }
    auto release() noexcept -> void { sent_ = true; }

    Frame()
        : data_()
    {
        if (0 != zmq_msg_init(&data_)) {
            throw std::runtime_error("Failed to initialize zmq frame");
        }
    }
    Frame(const void* data, const std::size_t size)
        : data_()
    {
        if (0 != zmq_msg_init_size(&data_, size)) {
            throw std::runtime_error("Failed to initialize zmq frame");
        }

        std::memcpy(zmq_msg_data(&data_), data, size);
    }
    template <
        typename Input,
        std::enable_if_t<std::is_trivially_copyable<Input>::value, int> = 0>
    Frame(const Input& in) noexcept
        : Frame(&in, sizeof(in))
    {
    }
    Frame(const Frame& rhs) noexcept
        : Frame(rhs.data(), rhs.size())
    {
    }
    Frame(Frame&& rhs) noexcept
        : Frame(rhs.data(), rhs.size())
    {
    }

    auto operator=(Frame&& rhs) -> Frame&
    {
        if (this != &rhs) {
            zmq_msg_close(&data_);

            if (0 != zmq_msg_init(&data_)) {
                throw std::runtime_error("Failed to initialize zmq frame");
            }

            std::memcpy(zmq_msg_data(&data_), rhs.data(), rhs.size());
        }

        return *this;
    }

    ~Frame()
    {
        if (false == sent_) { zmq_msg_close(&data_); }
    }

private:
    zmq_msg_t data_;
    bool sent_{false};

    auto operator=(const Frame&) -> Frame& = delete;
};

class Message : public std::vector<Frame>
{
public:
    using Identity = std::vector<std::byte>;

    static auto MakePush(const Identity& recipient, const Command type) noexcept
        -> Message;

    auto arg(const std::size_t index) const -> const Frame&;
    auto arg_count() const noexcept -> std::size_t;
    auto identity() const -> Identity;
    auto type() const noexcept -> Command;

    auto change_type(const Command newType) noexcept -> bool;

private:
    mutable bool parsed_{false};
    mutable const_iterator seperator_{};

    auto parse() const noexcept -> bool;
};

class Socket
{
public:
    static auto receive(Message& output, zmq_pollitem_t& poll) noexcept -> bool;

    operator void*() const noexcept { return data_; }

    auto receive(Message& output) const noexcept -> bool;
    auto send(Message&& input) const noexcept -> bool;

    Socket(const Context& context, const int type)
        : data_(zmq_socket(context, type))
    {
    }

    Socket(Socket&& rhs)
        : data_(rhs.data_)
    {
        rhs.data_ = nullptr;
    }

    ~Socket()
    {
        if (nullptr != data_) { zmq_close(data_); }
    }

private:
    void* data_;

    static auto rcvmore(void* socket) noexcept -> bool;
    static auto receive(void* socket, Message& output) noexcept -> bool;

    auto rcvmore() const noexcept -> bool;

    Socket() = delete;
    Socket(const Socket&) = delete;
    auto operator=(const Socket&) -> Socket& = delete;
    auto operator=(Socket &&) -> Socket& = delete;
};
}  // namespace libsubtractive::zmq
