#include "p2dic/galaxy_camera.hpp"

#if defined(P2DIC_WITH_GALAXY)

#include <GxIAPI.h>

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

namespace p2dic {
namespace {

std::string galaxy_error(GX_STATUS status) {
    std::size_t size = 0;
    GX_STATUS reported = status;
    if (GXGetLastError(&reported, nullptr, &size) != GX_STATUS_SUCCESS || size == 0) {
        return "GalaxySDK status " + std::to_string(static_cast<int>(status));
    }
    std::string message(size, '\0');
    if (GXGetLastError(&reported, message.data(), &size) != GX_STATUS_SUCCESS) {
        return "GalaxySDK status " + std::to_string(static_cast<int>(status));
    }
    while (!message.empty() && message.back() == '\0') {
        message.pop_back();
    }
    return message;
}

void require_success(GX_STATUS status, const char* operation) {
    if (status != GX_STATUS_SUCCESS) {
        throw std::runtime_error(std::string(operation) + ": " + galaxy_error(status));
    }
}

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

class BufferReturnGuard {
public:
    BufferReturnGuard(GX_DEV_HANDLE device, PGX_FRAME_BUFFER buffer)
        : device_(device), buffer_(buffer) {}
    ~BufferReturnGuard() {
        if (device_ != nullptr && buffer_ != nullptr) {
            GXQBuf(device_, buffer_);
        }
    }

private:
    GX_DEV_HANDLE device_{};
    PGX_FRAME_BUFFER buffer_{};
};

}  // namespace

GalaxyCamera::GalaxyCamera(GalaxyCameraConfig config) : config_(std::move(config)) {}

GalaxyCamera::~GalaxyCamera() {
    try {
        close();
    } catch (...) {
    }
}

void GalaxyCamera::open() {
    if (device_ != nullptr) {
        return;
    }
    require_success(GXInitLib(), "GXInitLib");
    library_open_ = true;

    try {
        std::uint32_t device_count = 0;
        require_success(GXUpdateAllDeviceList(&device_count, 1000), "GXUpdateAllDeviceList");
        if (device_count == 0) {
            throw std::runtime_error("No GalaxySDK camera was detected");
        }

        std::string selector = config_.serial_number.empty()
                                   ? std::to_string(config_.device_index)
                                   : config_.serial_number;
        GX_OPEN_PARAM open_parameter{};
        open_parameter.pszContent = selector.data();
        open_parameter.openMode = config_.serial_number.empty() ? GX_OPEN_INDEX : GX_OPEN_SN;
        open_parameter.accessMode = GX_ACCESS_EXCLUSIVE;

        GX_DEV_HANDLE opened_device = nullptr;
        require_success(GXOpenDevice(&open_parameter, &opened_device), "GXOpenDevice");
        device_ = opened_device;

        auto handle = static_cast<GX_DEV_HANDLE>(device_);
        require_success(GXSetEnumValueByString(handle, "PixelFormat", "Mono8"), "Set Mono8");
        require_success(GXSetIntValue(handle, "Width", config_.width), "Set Width");
        require_success(GXSetIntValue(handle, "Height", config_.height), "Set Height");
        require_success(GXSetIntValue(handle, "OffsetX", config_.offset_x), "Set OffsetX");
        require_success(GXSetIntValue(handle, "OffsetY", config_.offset_y), "Set OffsetY");
        require_success(GXSetFloatValue(handle, "ExposureTime", config_.exposure_us), "Set ExposureTime");
        require_success(GXSetFloatValue(handle, "Gain", config_.gain_db), "Set Gain");
        require_success(
            GXSetEnumValueByString(handle, "TriggerMode", config_.external_trigger ? "On" : "Off"),
            "Set TriggerMode");
    } catch (...) {
        close();
        throw;
    }
}

void GalaxyCamera::start() {
    if (device_ == nullptr) {
        throw std::logic_error("Galaxy camera must be opened before streaming");
    }
    if (!streaming_) {
        require_success(
            GXSetCommandValue(static_cast<GX_DEV_HANDLE>(device_), "AcquisitionStart"),
            "AcquisitionStart");
        streaming_ = true;
    }
}

bool GalaxyCamera::grab(Frame& destination, std::chrono::milliseconds timeout) {
    if (!streaming_) {
        throw std::logic_error("Galaxy camera is not streaming");
    }
    PGX_FRAME_BUFFER buffer = nullptr;
    const GX_STATUS status = GXDQBuf(
        static_cast<GX_DEV_HANDLE>(device_),
        &buffer,
        static_cast<std::uint32_t>(timeout.count()));
    if (status == GX_STATUS_TIMEOUT) {
        return false;
    }
    require_success(status, "GXDQBuf");
    BufferReturnGuard return_guard(static_cast<GX_DEV_HANDLE>(device_), buffer);
    if (buffer == nullptr || buffer->nStatus != GX_FRAME_STATUS_SUCCESS) {
        throw std::runtime_error("Galaxy camera returned an incomplete frame");
    }
    if (buffer->nWidth != static_cast<int>(destination.width) ||
        buffer->nHeight != static_cast<int>(destination.height)) {
        throw std::runtime_error("Galaxy frame dimensions do not match the configured frame pool");
    }
    const auto expected_bytes = static_cast<std::size_t>(destination.width) * destination.height;
    if (buffer->nImgSize < 0 || static_cast<std::size_t>(buffer->nImgSize) < expected_bytes) {
        throw std::runtime_error("Galaxy Mono8 frame payload is smaller than expected");
    }
    const auto image_address = static_cast<std::uintptr_t>(buffer->pImgBuf);
    std::memcpy(
        destination.pixels.data(),
        reinterpret_cast<const void*>(image_address),
        expected_bytes);
    destination.sequence = buffer->nFrameID;
    destination.timestamp_ns = now_ns();
    return true;
}

void GalaxyCamera::stop() {
    if (device_ != nullptr && streaming_) {
        require_success(
            GXSetCommandValue(static_cast<GX_DEV_HANDLE>(device_), "AcquisitionStop"),
            "AcquisitionStop");
        streaming_ = false;
    }
}

void GalaxyCamera::close() {
    if (device_ != nullptr) {
        if (streaming_) {
            GXSetCommandValue(static_cast<GX_DEV_HANDLE>(device_), "AcquisitionStop");
            streaming_ = false;
        }
        GXCloseDevice(static_cast<GX_DEV_HANDLE>(device_));
        device_ = nullptr;
    }
    if (library_open_) {
        GXCloseLib();
        library_open_ = false;
    }
}

std::string_view GalaxyCamera::name() const noexcept {
    return "DAHENG-GalaxySDK";
}

}  // namespace p2dic

#endif
