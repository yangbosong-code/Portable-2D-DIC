#pragma once

#include "p2dic/frame.hpp"

#include <chrono>
#include <string_view>

namespace p2dic {

class ICamera {
public:
    virtual ~ICamera() = default;

    virtual void open() = 0;
    virtual void start() = 0;
    virtual bool grab(Frame& destination, std::chrono::milliseconds timeout) = 0;
    virtual void stop() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

}  // namespace p2dic
