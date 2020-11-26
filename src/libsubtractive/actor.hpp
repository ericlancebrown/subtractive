#pragma once

#include <zmq.h>
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "libsubtractive/communication/zmq/zeromq_wrapper.hpp"

namespace libsubtractive
{
template <typename CRTP>
class Actor
{
protected:
    using Sockets = std::vector<zmq::Socket>;
    // NOTE SocketInit is necessary because c++ doesn't support movable
    // initialization lists, and zmq::Sockets are a move-only type
    using SocketInit = std::function<Sockets()>;

    const zmq::Context& zeromq_;
    Sockets sockets_;
    const bool enabled_;
    std::vector<zmq_pollitem_t> new_poll_items_;

    auto init_actor() noexcept
    {
        if (false == enabled_) { return; }

        const auto previous = running_.exchange(true);

        if (false == previous) {
            zmq_thread_ = std::thread{[this] { zmq_thread(); }};
        }
    }

    auto shutdown_actor() noexcept
    {
        running_ = false;

        if (zmq_thread_.joinable()) { zmq_thread_.join(); }
    }

    Actor(const zmq::Context& zeromq, SocketInit sockets)
        : zeromq_(zeromq)
        , sockets_(sockets())
        , enabled_(0 < sockets_.size())
        , new_poll_items_()
        , poll_items_()
        , running_(false)
        , zmq_thread_()
    {
    }
    virtual ~Actor() { shutdown_actor(); }

private:
    std::vector<zmq_pollitem_t> poll_items_;
    std::atomic_bool running_;
    std::thread zmq_thread_;

    auto child() noexcept -> CRTP& { return static_cast<CRTP&>(*this); }

    auto zmq_thread() noexcept -> void
    {
        assert(0 == poll_items_.size());

        for (auto& socket : sockets_) {
            auto& item = poll_items_.emplace_back();
            item.socket = socket;
            item.events = ZMQ_POLLIN;
        }

        assert(sockets_.size() == poll_items_.size());

        while (running_) {
            const auto events = zmq_poll(
                poll_items_.data(), static_cast<int>(poll_items_.size()), 1);

            if (0 > events) {
                const auto error = zmq_errno();
                std::cerr << zmq_strerror(error) << '\n';

                continue;
            } else if (0 == events) {
                continue;
            }

            auto disconnectAfter{false};

            for (auto& item : poll_items_) {
                if (ZMQ_POLLIN == item.revents) {
                    auto message = zmq::Message{};

                    if (zmq::Socket::receive(message, item)) {
                        disconnectAfter |=
                            child().process_command(std::move(message));
                    }
                }
            }

            poll_items_.reserve(poll_items_.size() + new_poll_items_.size());

            for (auto& item : new_poll_items_) {
                poll_items_.emplace_back(std::move(item));
            }

            new_poll_items_.clear();

            if (disconnectAfter) { running_ = false; }
        }
    }
};
}  // namespace libsubtractive
