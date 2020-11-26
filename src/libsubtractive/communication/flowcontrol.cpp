#include "libsubtractive/communication/flowcontrol.hpp"  // IWYU pragma: associated

#include <boost/container/flat_map.hpp>
#include <zmq.h>
#include <cassert>
#include <cstring>
#include <regex>
#include <type_traits>
#include <utility>
#include <vector>

#include "libsubtractive/communication/zmq/zeromq_wrapper.hpp"

namespace libsubtractive
{
constexpr auto AlarmPrefix{"ALARM"};
constexpr auto GrblPrefix{"Grbl"};
constexpr auto PushPrefix{"["};
constexpr auto PushSuffix{"]"};
constexpr auto StatusPrefix{"<"};
constexpr auto StatusSuffix{">"};
constexpr auto ResponseGood{"ok"};
constexpr auto ResponseBad{"error:"};

constexpr auto ends_with(
    const std::string_view text,
    const std::string_view suffix) noexcept -> bool
{
    if (text.size() < suffix.size()) { return false; }

    auto it{text.data()};
    std::advance(
        it,
        static_cast<std::iterator_traits<const char*>::difference_type>(
            text.size() - suffix.size()));

    return 0 == std::memcmp(it, suffix.data(), suffix.size());
}
constexpr auto starts_with(
    const std::string_view text,
    const std::string_view prefix) noexcept -> bool
{
    if (text.size() < prefix.size()) { return false; }

    return 0 == std::memcmp(text.data(), prefix.data(), prefix.size());
}

auto FlowControl::Classifier::dump(zmq::Message& out) noexcept -> void
{
    for (const auto& line : buffer_) {
        out.emplace_back(line.data(), line.size());
    }

    buffer_.clear();
}

auto FlowControl::Classifier::operator()(const std::string_view line) noexcept
    -> Type
{
    if (0 == line.size()) { return Type::Empty; }

    if (starts_with(line, AlarmPrefix)) {
        mode_ = Mode::Normal;
        buffer_.clear();
        buffer_.emplace_back(line);

        return Type::Alarm;
    }

    if (starts_with(line, GrblPrefix)) {
        mode_ = Mode::Normal;
        buffer_.clear();
        buffer_.emplace_back(line);

        return Type::Startup;
    }

    if (starts_with(line, PushPrefix) && ends_with(line, PushSuffix)) {
        if (Mode::Help == mode_) {
            buffer_.emplace_back(line);

            return Type::Multiline;
        }

        mode_ = Mode::Normal;
        buffer_.clear();
        buffer_.emplace_back(line);

        return Type::Push;
    }

    if (starts_with(line, StatusPrefix) && ends_with(line, StatusSuffix)) {
        mode_ = Mode::Normal;
        buffer_.clear();
        buffer_.emplace_back(line);

        return Type::Startup;
    }

    if (starts_with(line, ResponseGood) || starts_with(line, ResponseBad)) {
        if (Mode::Help == mode_) {
            mode_ = Mode::Normal;
            buffer_.emplace_back(line);

            return Type::MultilineDone;
        } else {
            mode_ = Mode::Normal;
            buffer_.clear();
            buffer_.emplace_back(line);

            return Type::Response;
        }
    }

    if (Mode::Help == mode_) {
        buffer_.emplace_back(line);

        return Type::Multiline;
    }

    buffer_.emplace_back(line);

    return Type::Unknown;
}

auto FlowControl::Classifier::reset() noexcept -> void { buffer_.clear(); }

auto FlowControl::Classifier::start_multiline() noexcept -> void
{
    mode_ = Mode::Help;
}

#ifndef __APPLE__
void FlowControl::Classifier::unit_test() const noexcept
{
    static_assert(starts_with("Grbl 0.9g ['$' for help]", GrblPrefix));
    static_assert(starts_with("Grbl 1.1h [help:'$']", GrblPrefix));
    static_assert(ends_with("Grbl 0.9g ['$' for help]", PushSuffix));

    constexpr auto version1 = grbl::VersionData{0, 9, 'g'};
    constexpr auto version2 = grbl::VersionData{1, 1, 'h'};
    constexpr auto version3 = grbl::VersionData{1, 1, 0};

    static_assert(version1 < version2);
    static_assert(version2 > version1);
    static_assert(version1 != version2);
    static_assert(version1 == version1);
    static_assert(version2 > version3);
}
#endif

auto FlowControl::Classifier::version() const noexcept -> grbl::VersionData
{
    auto output = grbl::VersionData{};

    if (0 == buffer_.size()) { return output; }

    auto& line = buffer_.at(0);
    auto& [major, minor, sub] = output;
    auto match = std::smatch{};
    auto regex = std::regex{"Grbl ([0-9]+)\\.([0-9]+)([a-z])"};

    if (std::regex_search(line, match, regex)) {
        try {
            major = std::stoul(match[1]);
        } catch (...) {
        }

        try {
            minor = std::stoul(match[2]);
        } catch (...) {
        }

        if (2 < match.size()) {
            if (const auto temp = std::string{match[3]}; 0 < temp.size()) {
                sub = temp.front();
            }
        }
    }

    return output;
}

FlowControl::FlowControl(
    const zmq::Context& zeromq,
    const std::string_view serialNumber,
    const std::string serialEndpoint,
    const std::string flowEndpoint)
    : Actor(
          zeromq,
          [&]() -> auto {
              auto output = Sockets{};
              output.emplace_back(
                  zeromq_.Socket(ZMQ_PAIR, Direction::Connect, flowEndpoint));
              output.emplace_back(
                  zeromq_.Socket(ZMQ_PAIR, Direction::Bind, serialEndpoint));

              return output;
          })
    , usb_id_(serialNumber)
    , limit_(127)
    , parent_socket_(sockets_.at(0))
    , serial_socket_(sockets_.at(1))
    , parse_()
    , active_(false)
    , alarm_(false)
    , incoming_()
    , outgoing_()
    , realtime_(std::nullopt)
    , used_()
{
    init_actor();
}

auto FlowControl::buffer(const std::string_view bytes) -> std::vector<std::byte>
{
    const auto it = reinterpret_cast<const std::byte*>(bytes.data());

    return std::vector<std::byte>{it, it + bytes.size()};
}

auto FlowControl::command_data_received(zmq::Message&& in) noexcept -> void
{
    if (0 == in.arg_count()) { abort(); }

    auto process{false};
    auto realtime{false};

    switch (parse_(in.arg(0).str())) {
        case Classifier::Type::Empty: {
            // std::cout << "Classify: empty\n";  // FIXME
            parse_.reset();
            process = false;
        } break;
        case Classifier::Type::Startup: {
            // std::cout << "Classify: startup\n";  // FIXME
            const auto version = parse_.version();
            const auto& [major, minor, sub] = version;
            auto message = zeromq_.Command(Command::InitGrbl);
            message.emplace_back(usb_id_.data(), usb_id_.size());
            message.emplace_back(major);
            message.emplace_back(minor);
            message.emplace_back(sub);
            parse_.dump(message);
            parent_socket_.send(std::move(message));
            alarm_ = false;
            process = false;
        } break;
        case Classifier::Type::Push: {
            // std::cout << "Classify: push\n";  // FIXME
            auto message = zeromq_.Command(Command::GrblPushReceived);
            message.emplace_back(usb_id_.data(), usb_id_.size());
            parse_.dump(message);
            parent_socket_.send(std::move(message));
            process = false;
        } break;
        case Classifier::Type::Status: {
            // std::cout << "Classify: status\n";  // FIXME
            process = true;
            realtime = true;
        } break;
        case Classifier::Type::Multiline: {
            // std::cout << "Classify: multiline\n";  // FIXME
            process = false;
        } break;
        case Classifier::Type::Alarm: {
            // std::cout << "Classify: alarm\n";  // FIXME
            // FIXME relay to parent socket
            parse_.reset();  // FIXME
            alarm_ = true;
            process = false;
        } break;
        case Classifier::Type::Response: {
            // std::cout << "Classify: response\n";  // FIXME
            process = true;
        } break;
        case Classifier::Type::MultilineDone: {
            // std::cout << "Classify: multiline done\n";  // FIXME
            process = true;
        } break;
        case Classifier::Type::Unknown:
        default: {
            // std::cout << "Classify: unknown\n";  // FIXME
            process = true;
        }
    }

    if (process) { receive(realtime); }
}

auto FlowControl::command_enable_flow_control(zmq::Message&&) noexcept -> void
{
    active_ = true;
}

auto FlowControl::command_send_message(zmq::Message&& in) noexcept -> void
{
    static const auto flags = boost::container::flat_map<Command, SendFlags>{
        {Command::GrblHelp,  // $
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::Multiline,
          Flag::Unplanned}},
        {Command::GrblParams,  // $#
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::Multiline,
          Flag::Unplanned}},
        {Command::GrblSettings,  // $$
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::Multiline,
          Flag::Unplanned}},
        {Command::GrblStartupBlocks,  // $N
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::Multiline,
          Flag::Unplanned}},
        {Command::GrblStatus,  // ?
         {Queue::Front,
          Flag::Realtime,
          Flag::CanBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblVersion,  // $I
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::Multiline,
          Flag::Unplanned}},
        {Command::GrblHome,  // $H
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblParserState,  // $G
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblCheckModeToggle,  // $C
         {Queue::Back,
          Flag::Queued,
          Flag::NoBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblResetAlarm,  // $X
         {Queue::Front,
          Flag::Queued,
          Flag::CanBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblSoftReset,  // 0x18
         {Queue::Reset,
          Flag::Queued,
          Flag::CanBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblCycleToggle,  // ~
         {Queue::Front,
          Flag::Realtime,
          Flag::CanBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblFeedHold,  // !
         {Queue::Front,
          Flag::Realtime,
          Flag::CanBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::GrblJogCancel,  // 0x85
         {Queue::Front,
          Flag::Realtime,
          Flag::CanBuffer,
          Flag::SingleLine,
          Flag::Unplanned}},
        {Command::SendGcode,
         {Queue::Back,
          Flag::Queued,
          Flag::CanBuffer,
          Flag::SingleLine,
          Flag::Planned}},
    };

    if (2 > in.arg_count()) { abort(); }

    const auto type = in.type();
    const auto clearsAlarm =
        (Command::GrblSoftReset == type) || (Command::GrblResetAlarm == type);

    if (active_) {
        queue({type, buffer(in.arg(1).str())}, flags.at(type), clearsAlarm);
    } else {
        serial_socket_.send(std::move(in));
    }
}

auto FlowControl::command_usb_device_added(zmq::Message&& in) noexcept -> void
{
    if (active_) { queue({}, {Queue::Reconnect}, true); }

    serial_socket_.send(std::move(in));
}

auto FlowControl::command_usb_device_removed(zmq::Message&& in) noexcept -> void
{
    if (active_) { queue({}, {Queue::Reconnect}, true); }

    serial_socket_.send(std::move(in));
}

auto FlowControl::process_command(zmq::Message&& command) noexcept -> bool
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
        case Command::EnableFlowControl: {
            command_enable_flow_control(std::move(command));
        } break;
        case Command::GrblHelp:
        case Command::GrblParams:
        case Command::GrblSettings:
        case Command::GrblStartupBlocks:
        case Command::GrblStatus:
        case Command::GrblVersion:
        case Command::GrblHome:
        case Command::GrblParserState:
        case Command::GrblCheckModeToggle:
        case Command::GrblCycleToggle:
        case Command::GrblFeedHold:
        case Command::GrblJogCancel:
        case Command::SendGcode: {
            if (alarm_) {
                std::cout << "Reset alarm first\n";

                return disconnectAfter;
            }
            [[fallthrough]];
        }
        case Command::GrblResetAlarm:
        case Command::GrblSoftReset: {
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
        case Command::InitGrbl:
        default: {
            abort();
        }
    }

    return disconnectAfter;
}

auto FlowControl::queue(
    const Request request,
    const SendFlags flags,
    const bool clearsAlarm) noexcept -> void
{
    const auto& [type, bytes] = request;

    assert(bytes.size() <= limit_);

    validate(flags);

    switch (flags.position_) {
        case Queue::Reconnect: {
            outgoing_.clear();
            used_ = 0;
            [[fallthrough]];
        }
        case Queue::Reset: {
            incoming_.clear();
            [[fallthrough]];
        }
        case Queue::Front: {
            if (0 < bytes.size()) { incoming_.emplace_front(request, flags); }
        } break;
        case Queue::Back: {
            if (0 < bytes.size()) { incoming_.emplace_back(request, flags); }
        } break;
        default: {
        }
    }

    run(clearsAlarm);
}

auto FlowControl::receive(const bool realtime) noexcept -> void
{
    response_received(realtime ? receive_realtime() : receive_normal());
    run();
}

auto FlowControl::receive_normal() noexcept -> Request
{
    auto output = Request{};

    if (false == outgoing_.empty()) {
        const auto& [flags, size, request] = outgoing_.front();
        output = request;
        const auto& [position, realtime, greedy, multiline, planned] = flags;

        if (value(planned)) { used_ -= size; }

        outgoing_.pop_front();
    }

    return output;
}

auto FlowControl::receive_realtime() noexcept -> Request
{
    auto output = Request{};

    if (realtime_.has_value()) {
        const auto& [flags, size, request] = realtime_.value();
        output = request;
        realtime_ = std::nullopt;
    }

    return output;
}

auto FlowControl::response_received(const Request request) noexcept -> void
{
    const auto [type, bytes] = request;
    auto message = zeromq_.Command(Command::ResponseReceived);
    message.emplace_back(usb_id_.data(), usb_id_.size());
    message.emplace_back(type);
    message.emplace_back(
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
    parse_.dump(message);
    parent_socket_.send(std::move(message));
}

auto FlowControl::run(const bool clearsAlarm) noexcept -> void
{
    auto available = limit_ - used_;

    if (alarm_ && (false == clearsAlarm)) { return; }

    while (false == incoming_.empty()) {
        const auto& [request, flags] = incoming_.front();
        const auto& [type, bytes] = request;
        const auto& [position, realtime, greedy, multiline, planned] = flags;
        const auto size = bytes.size();

        if (value(planned)) {
            if (size > available) { return; }

            used_ += size;
            available -= size;
        } else {
            if (value(greedy) && (0 < outgoing_.size())) { return; }

            if (value(realtime) && realtime_.has_value()) { return; }
        }

        transmit(bytes);

        if (Queue::Reset == position) {
            active_ = false;
            outgoing_.clear();
            incoming_.clear();
            realtime_ = std::nullopt;
            used_ = 0;
        } else {
            if (value(realtime)) {
                assert(false == realtime_.has_value());

                realtime_.emplace(flags, size, request);
            } else {
                outgoing_.emplace_back(flags, size, request);
            }

            incoming_.pop_front();
            const auto& next = outgoing_.front();
            const auto& nextFlags = std::get<0>(next);

            if (value(nextFlags.multiline_)) { parse_.start_multiline(); }
        }
    }
}

auto FlowControl::transmit(const Bytes& bytes) noexcept -> void
{
    auto message = zeromq_.Command(Command::SendGcode);
    message.emplace_back();
    message.emplace_back(bytes.data(), bytes.size());
    serial_socket_.send(std::move(message));
}

FlowControl::~FlowControl() { shutdown_actor(); }
}  // namespace libsubtractive
