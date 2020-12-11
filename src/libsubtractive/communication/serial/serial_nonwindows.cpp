#include "libsubtractive/communication/serial/serial_common.hpp"  // IWYU pragma: associated

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>  // IWYU pragma: keep
#include <boost/bind/mem_fn.hpp>
#include <boost/system/error_code.hpp>
#include <cctype>
#include <thread>

#include "libsubtractive/communication/serial/serial.hpp"

namespace libsubtractive
{
struct Nonwindows final : virtual public SerialConnection::Imp {
    auto connect(const std::string_view path) -> void final
    {
        disconnect();
        asio_context_ = std::make_unique<boost::asio::io_context>();
        work_ = std::make_unique<boost::asio::io_context::work>(*asio_context_);
        asio_thread_ = std::thread([this]() { asio_context_->run(); });

        try {
            serial_port_ = std::make_unique<boost::asio::serial_port>(
                *asio_context_, std::string{path});
        } catch (...) {
            throw std::runtime_error("Serial port does not exist");
        }

        try {
            using serial = boost::asio::serial_port_base;
            serial_port_->set_option(serial::baud_rate(115200));
            serial_port_->set_option(serial::character_size(8));
            serial_port_->set_option(serial::parity(serial::parity::none));
            serial_port_->set_option(serial::stop_bits(serial::stop_bits::one));
        } catch (...) {
            throw std::runtime_error("Failed to configure serial port");
        }

        get_next_byte();
    }
    auto disconnect() -> void final
    {
        work_.restart();

        if (asio_context_) {
            asio_context_->stop();

            while (false == asio_context_->stopped()) { ; }
        }

        if (asio_thread_.joinable()) { asio_thread_.join(); }

        serial_port_.restart();
        asio_context_.restart();
    }
    auto transmit(const std::string_view data) -> void final
    {
        std::cout << __FUNCTION__ << ": " << data << '\n';
        boost::asio::write(
            *serial_port_, boost::asio::buffer(data.data(), data.size()));
    }

    Nonwindows(
        const zmq::Context& zeromq,
        const std::string_view endpoint,
        const bool enabled)
        : SerialConnection::Imp(zeromq, endpoint, enabled)
        , asio_context_()
        , serial_port_()
        , receive_buffer_()
        , work_()
        , asio_thread_()
        , next_byte_()
    {
        receive_buffer_.reserve(256);
    }
    ~Nonwindows() final { shutdown(); }

private:
    mutable std::unique_ptr<boost::asio::io_context> asio_context_;
    mutable std::unique_ptr<boost::asio::serial_port> serial_port_;
    mutable std::vector<char> receive_buffer_;
    std::unique_ptr<boost::asio::io_context::work> work_;
    std::thread asio_thread_;
    char next_byte_;

    auto flush_receive_buffer() noexcept -> void
    {
        if (1 < receive_buffer_.size()) {
            auto data = std::vector<char>{};
            data.reserve(receive_buffer_.size());

            for (const auto& byte : receive_buffer_) {
                if (std::isprint(byte)) { data.emplace_back(byte); }
            }

            auto message = zeromq_.Command(Command::DataReceived);
            std::cout << "receive: " << std::string{data.data(), data.size()}
                      << '\n';
            message.emplace_back(data.data(), data.size());
            internal_push_.send(std::move(message));
        }

        receive_buffer_.clear();
        next_byte_ = {};
    }
    auto get_next_byte() noexcept -> void
    {
        if (serial_port_ && work_) {
            serial_port_->async_read_some(
                boost::asio::buffer(&next_byte_, sizeof(next_byte_)),
                boost::bind(
                    &Nonwindows::read_cb,
                    this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }
    auto read_cb(
        const boost::system::error_code& error,
        const std::size_t bytes) noexcept -> void
    {
        if ((0 < bytes) && !error) {
            if ('\r' == next_byte_) {
                next_byte_ = {};
            } else {
                receive_buffer_.emplace_back(next_byte_);
            }
        }

        if ('\n' == next_byte_) { flush_receive_buffer(); }

        get_next_byte();
    }
};

auto SerialConnection::Imp::Factory(
    const zmq::Context& zeromq,
    const std::string_view endpoint,
    const bool enabled) noexcept -> std::unique_ptr<Imp>
{
    return std::make_unique<Nonwindows>(zeromq, endpoint, enabled);
}
}  // namespace libsubtractive
