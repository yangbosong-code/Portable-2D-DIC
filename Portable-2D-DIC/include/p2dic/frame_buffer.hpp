#pragma once

#include <cstddef>
#include <cstdint>

namespace p2dic {

// Contiguous image storage. CUDA builds first try page-locked host memory so
// the camera can copy straight into an async-transfer-capable frame pool.
// Allocation safely falls back to ordinary memory when no CUDA device/driver
// is available, which keeps diagnostics and CPU fallbacks usable.
class FrameBuffer {
public:
    using value_type = std::uint8_t;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    FrameBuffer() = default;
    explicit FrameBuffer(std::size_t size);
    FrameBuffer(const FrameBuffer& other);
    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(const FrameBuffer& other);
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;
    ~FrameBuffer();

    void resize(std::size_t size);
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] bool pinned() const noexcept { return pinned_; }
    [[nodiscard]] value_type* data() noexcept { return data_; }
    [[nodiscard]] const value_type* data() const noexcept { return data_; }
    [[nodiscard]] iterator begin() noexcept { return data_; }
    [[nodiscard]] iterator end() noexcept { return data_ + size_; }
    [[nodiscard]] const_iterator begin() const noexcept { return data_; }
    [[nodiscard]] const_iterator end() const noexcept { return data_ + size_; }
    [[nodiscard]] value_type& front() noexcept { return data_[0]; }
    [[nodiscard]] const value_type& front() const noexcept { return data_[0]; }
    [[nodiscard]] value_type& operator[](std::size_t index) noexcept { return data_[index]; }
    [[nodiscard]] const value_type& operator[](std::size_t index) const noexcept {
        return data_[index];
    }

    friend bool operator==(const FrameBuffer& left, const FrameBuffer& right) noexcept;

private:
    void release() noexcept;

    value_type* data_{};
    std::size_t size_{};
    bool pinned_{false};
};

}  // namespace p2dic
