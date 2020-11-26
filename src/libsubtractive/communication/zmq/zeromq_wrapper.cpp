#include "zeromq_wrapper.hpp"  // IWYU pragma: associated

#include <atomic>
#include <iostream>
#include <iterator>

namespace libsubtractive
{
constexpr std::string_view EndpointNamespace{"inproc://libsubtractive/"};
constexpr std::string_view ContextSuffix{"context"};
constexpr std::string_view DeviceSuffix{"device/"};
constexpr std::string_view UnstableSuffix{"unstable/"};

const std::string context_endpoint_{
    std::string{EndpointNamespace} + ContextSuffix.data()};
const std::string device_prefix_{
    std::string{EndpointNamespace} + DeviceSuffix.data()};
const std::string random_prefix_{
    std::string{EndpointNamespace} + UnstableSuffix.data()};

auto ContextEndpoint() noexcept -> const std::string&
{
    return context_endpoint_;
}

auto DeviceEndpoint(const std::string_view serial) noexcept -> std::string
{
    return device_prefix_ + serial.data();
}

auto RandomEndpoint() noexcept -> std::string
{
    static auto counter = std::atomic<int>{-1};

    return random_prefix_ + std::to_string(++counter);
}
}  // namespace libsubtractive

namespace libsubtractive::zmq
{
auto Context::Command(const libsubtractive::Command type) const noexcept
    -> zmq::Message
{
    auto output = Message();
    output.emplace_back();
    output.emplace_back(type);

    return output;
}

auto Context::Response(const Message& request) const noexcept -> zmq::Message
{
    auto output = Message();
    auto body{false};

    for (const auto& frame : request) {
        if (0 == frame.size()) {
            body = true;
            break;
        }

        output.emplace_back(frame.data(), frame.size());
    }

    if (false == body) { output.clear(); }

    if (0 == output.size()) { output.emplace_back(); }

    return output;
}

auto Context::Socket(const int type) const noexcept -> zmq::Socket
{
    return zmq::Socket{*this, type};
}

auto Context::Socket(
    const int type,
    const Direction direction,
    const std::string_view endpoint) const -> zmq::Socket
{
    auto output = Socket(type);

    const auto rc =
        ((direction == Direction::Connect) ? zmq_connect
                                           : zmq_bind)(output, endpoint.data());

    if (0 != rc) {
        throw std::runtime_error("Failed to connect or bind socket");
    }

    return output;
}

auto Message::arg(const std::size_t index) const -> const Frame&
{
    if (false == parse()) {
        throw std::out_of_range("No discernible message body");
    }

    auto it{seperator_};
    std::advance(it, static_cast<const_iterator::difference_type>(2u + index));

    if (it >= cend()) { throw std::out_of_range("Invalid index"); }

    return *it;
}

auto Message::arg_count() const noexcept -> std::size_t
{
    if (false == parse()) { return 0; }

    auto it{seperator_};
    std::advance(it, 2);

    if (it >= cend()) { return 0; }

    return static_cast<std::size_t>(std::distance(it, cend()));
}

auto Message::change_type(const Command newType) noexcept -> bool
{
    if (false == parse()) { return false; }

    auto it{seperator_};
    std::advance(it, 1);

    if (it >= cend()) { return false; }

    auto& frame = const_cast<Frame&>(*it);
    frame = Frame{newType};

    return true;
}

auto Message::identity() const -> std::vector<std::byte>
{
    const auto& frame = at(0);
    const auto size = frame.size();

    if (0 == size) { return {}; }

    const auto data = static_cast<const std::byte*>(frame.data());

    return {data, data + size};
}

auto Message::MakePush(const Identity& recipient, const Command type) noexcept
    -> Message
{
    auto output = Message{};
    output.emplace_back(recipient.data(), recipient.size());
    output.emplace_back();
    output.emplace_back(type);

    return output;
}

auto Message::parse() const noexcept -> bool
{
    if (parsed_) { return cend() != seperator_; }

    seperator_ = cend();

    for (auto i{cbegin()}; i != cend(); ++i) {
        if (0 == i->size()) {
            parsed_ = true;
            seperator_ = i;

            return true;
        }
    }

    return false;
}

auto Message::type() const noexcept -> Command
{
    if (false == parse()) { return Command::Invalid; }

    auto it{seperator_};
    std::advance(it, 1);

    if (it >= cend()) { return Command::Invalid; }

    try {

        return it->as<Command>();
    } catch (...) {
        return Command::Invalid;
    }
}

auto Socket::rcvmore(void* socket) noexcept -> bool
{
    if (nullptr == socket) { return false; }

    auto data = int{};
    auto size = sizeof(data);

    if (0 > zmq_getsockopt(socket, ZMQ_RCVMORE, &data, &size)) {
        const auto error = zmq_errno();
        std::cerr << zmq_strerror(error) << '\n';

        return false;
    }

    return 0 != data;
}

auto Socket::rcvmore() const noexcept -> bool { return rcvmore(data_); }

auto Socket::receive(void* socket, Message& output) noexcept -> bool
{
    if (nullptr == socket) { return false; }

    auto more{true};

    while (more) {
        auto& frame = output.emplace_back();

        if (0 > zmq_msg_recv(frame, socket, ZMQ_DONTWAIT)) {
            const auto error = zmq_errno();
            std::cerr << zmq_strerror(error) << '\n';

            return false;
        }

        more = rcvmore(socket);
    }

    return true;
}

auto Socket::receive(Message& output, zmq_pollitem_t& poll) noexcept -> bool
{
    return receive(poll.socket, output);
}

auto Socket::receive(Message& output) const noexcept -> bool
{
    return receive(data_, output);
}

auto Socket::send(Message&& input) const noexcept -> bool
{
    auto counter = std::size_t{0};

    for (auto& frame : input) {
        const auto more = (++counter < input.size()) ? ZMQ_SNDMORE : 0;

        if (0 > zmq_msg_send(frame, data_, more)) {
            const auto error = zmq_errno();
            std::cerr << zmq_strerror(error) << '\n';

            return false;
        } else {
            frame.release();
        }
    }

    return true;
}
}  // namespace libsubtractive::zmq
