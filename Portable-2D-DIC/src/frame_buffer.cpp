#include "p2dic/frame_buffer.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

#if defined(P2DIC_WITH_CUDA)
#include <cuda_runtime_api.h>
#endif

namespace p2dic {

FrameBuffer::FrameBuffer(std::size_t size) {
    resize(size);
}

FrameBuffer::FrameBuffer(const FrameBuffer& other) {
    resize(other.size_);
    if (size_ != 0) {
        std::memcpy(data_, other.data_, size_);
    }
}

FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      pinned_(std::exchange(other.pinned_, false)) {}

FrameBuffer& FrameBuffer::operator=(const FrameBuffer& other) {
    if (this != &other) {
        resize(other.size_);
        if (size_ != 0) {
            std::memcpy(data_, other.data_, size_);
        }
    }
    return *this;
}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept {
    if (this != &other) {
        release();
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
        pinned_ = std::exchange(other.pinned_, false);
    }
    return *this;
}

FrameBuffer::~FrameBuffer() {
    release();
}

void FrameBuffer::resize(std::size_t size) {
    if (size == size_) {
        return;
    }
    FrameBuffer replacement;
    if (size != 0) {
#if defined(P2DIC_WITH_CUDA)
        void* allocated = nullptr;
        if (cudaHostAlloc(&allocated, size, cudaHostAllocPortable) == cudaSuccess) {
            replacement.data_ = static_cast<value_type*>(allocated);
            replacement.pinned_ = true;
        } else {
            // Clear the runtime's sticky allocation error before falling back.
            (void)cudaGetLastError();
            replacement.data_ = new value_type[size];
        }
#else
        replacement.data_ = new value_type[size];
#endif
        replacement.size_ = size;
        if (data_ != nullptr) {
            std::memcpy(replacement.data_, data_, std::min(size_, size));
        }
    }
    *this = std::move(replacement);
}

void FrameBuffer::release() noexcept {
    if (data_ == nullptr) {
        return;
    }
#if defined(P2DIC_WITH_CUDA)
    if (pinned_) {
        (void)cudaFreeHost(data_);
    } else {
        delete[] data_;
    }
#else
    delete[] data_;
#endif
    data_ = nullptr;
    size_ = 0;
    pinned_ = false;
}

bool operator==(const FrameBuffer& left, const FrameBuffer& right) noexcept {
    return left.size_ == right.size_ &&
           (left.size_ == 0 || std::memcmp(left.data_, right.data_, left.size_) == 0);
}

}  // namespace p2dic
