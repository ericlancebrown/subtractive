#pragma once

// IWYU pragma: no_include <ext/type_traits>

#include <atomic>
#include <deque>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>  // IWYU pragma: keep
#include <utility>
#include <vector>

#include "libsubtractive/actor.hpp"
#include "libsubtractive/protocol/Grbl.hpp"

namespace libsubtractive
{
namespace zmq
{
class Context;
class Message;
class Socket;
}  // namespace zmq
}  // namespace libsubtractive

namespace libsubtractive
{
class FlowControl final : Actor<FlowControl>
{
public:
    enum class Queue : std::int8_t {
        Reconnect = -2,
        Reset = -1,
        Back = 0,
        Front = 1,
    };

    FlowControl(
        const zmq::Context& zeromq,
        const std::string_view serialNumber,
        const std::string serialEndpoint,
        const std::string flowEndpoint);

    ~FlowControl();

private:
    friend Actor<FlowControl>;

    enum class Flag : bool {
        Realtime = true,
        Queued = false,
        NoBuffer = true,
        CanBuffer = false,
        Multiline = true,
        SingleLine = false,
        Planned = true,
        Unplanned = false,
    };

    struct SendFlags {
        Queue position_{Queue::Back};
        Flag realtime_{Flag::Queued};
        Flag requires_empty_buffer_{Flag::CanBuffer};
        Flag multiline_{Flag::SingleLine};
        Flag planned_{Flag::Planned};
    };

    using Bytes = std::vector<std::byte>;
    using Request = std::pair<Command, Bytes>;
    using Queued = std::tuple<Request, SendFlags>;
    using IncomingBuffer = std::deque<Queued>;
    using Pending = std::tuple<SendFlags, std::size_t, Request>;
    using OutgoingBuffer = std::deque<Pending>;

    struct Classifier {
        enum class Type : std::uint8_t {
            Empty,
            Unknown,
            Startup,
            Response,
            Push,
            Status,
            Multiline,
            MultilineDone,
            Alarm,
        };

        auto version() const noexcept -> grbl::VersionData;

        auto operator()(const std::string_view line) noexcept -> Type;

        auto dump(zmq::Message& out) noexcept -> void;
        auto reset() noexcept -> void;
        auto start_multiline() noexcept -> void;

    private:
        enum class Mode : std::uint8_t {
            Normal,
            Help,
        };

        std::vector<std::string> buffer_{};
        Mode mode_{Mode::Normal};

        void unit_test() const noexcept;
    };

    const std::string usb_id_;
    const std::size_t limit_;
    const zmq::Socket& parent_socket_;
    const zmq::Socket& serial_socket_;
    Classifier parse_;
    std::atomic_bool active_;
    std::atomic_bool alarm_;
    IncomingBuffer incoming_;
    OutgoingBuffer outgoing_;
    std::optional<Pending> realtime_;
    std::size_t used_;

    static auto buffer(const std::string_view bytes) -> std::vector<std::byte>;
    static constexpr auto validate(const SendFlags& flags)
    {
        const auto& [position, realtime, greedy, multiline, planned] = flags;

        if (Queue::Reset == position) {
            assert(Flag::Queued == realtime);
            assert(Flag::Unplanned == planned);
            assert(Flag::SingleLine == multiline);
        }

        if (Flag::Realtime == realtime) {
            assert(Flag::Unplanned == planned);
            assert(Flag::SingleLine == multiline);
        }

        if (Flag::Planned == planned) {
            assert(Flag::CanBuffer == greedy);
            assert(Flag::SingleLine == multiline);
        }
    }
    static constexpr auto value(const Flag in) noexcept
    {
        return static_cast<bool>(in);
    }

    auto command_data_received(zmq::Message&& in) noexcept -> void;
    auto command_enable_flow_control(zmq::Message&& in) noexcept -> void;
    auto command_send_message(zmq::Message&& in) noexcept -> void;
    auto command_usb_device_added(zmq::Message&& in) noexcept -> void;
    auto command_usb_device_removed(zmq::Message&& in) noexcept -> void;
    auto process_command(zmq::Message&& command) noexcept -> bool;
    auto queue(
        const Request request,
        const SendFlags flags,
        const bool clearsAlarm) noexcept -> void;
    auto receive(const bool realtime) noexcept -> void;
    auto receive_normal() noexcept -> Request;
    auto receive_realtime() noexcept -> Request;
    auto response_received(const Request request) noexcept -> void;
    auto run(const bool clearsAlarm = false) noexcept -> void;
    auto transmit(const Bytes& bytes) noexcept -> void;
};
}  // namespace libsubtractive
