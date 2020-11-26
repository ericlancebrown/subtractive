#pragma once

#include <memory>
#include <string_view>

namespace libsubtractive
{
namespace zmq
{
class Context;
}  // namespace zmq
}  // namespace libsubtractive

namespace libsubtractive
{
class SerialConnection
{
public:
    struct Imp;

    SerialConnection(
        const zmq::Context& zeromq,
        const std::string_view endpoint,
        const bool enabled);
    ~SerialConnection();

private:
    std::unique_ptr<Imp> imp_;
};
}  // namespace libsubtractive
