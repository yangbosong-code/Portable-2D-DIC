#include "p2dic/cuda_grid_engine.hpp"

#include <cuda_runtime.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace p2dic {
namespace {

struct DevicePoint {
    int x;
    int y;
};

struct DeviceResult {
    float u;
    float v;
    float quality;
    int iterations;
    int valid;
};

struct DeviceReferenceStats {
    float h00;
    float h01;
    float h11;
};

void cuda_check(cudaError_t status, const char* action) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(action) + ": " + cudaGetErrorString(status));
    }
}

__device__ bool bilinear_sample(
    const unsigned char* image,
    int width,
    int height,
    int stride,
    float x,
    float y,
    float& value,
    float& gx,
    float& gy) {
    const int x0 = static_cast<int>(floorf(x));
    const int y0 = static_cast<int>(floorf(y));
    if (x0 < 0 || y0 < 0 || x0 + 1 >= width || y0 + 1 >= height) {
        return false;
    }
    const float tx = x - x0;
    const float ty = y - y0;
    const float a = image[y0 * stride + x0];
    const float b = image[y0 * stride + x0 + 1];
    const float c = image[(y0 + 1) * stride + x0];
    const float d = image[(y0 + 1) * stride + x0 + 1];
    value = (1.0F - ty) * ((1.0F - tx) * a + tx * b) +
            ty * ((1.0F - tx) * c + tx * d);
    gx = (1.0F - ty) * (b - a) + ty * (d - c);
    gy = (1.0F - tx) * (c - a) + tx * (d - b);
    return true;
}

__device__ bool bilinear_value(
    const unsigned char* image,
    int width,
    int height,
    int stride,
    float x,
    float y,
    float& value) {
    const int x0 = static_cast<int>(floorf(x));
    const int y0 = static_cast<int>(floorf(y));
    if (x0 < 0 || y0 < 0 || x0 + 1 >= width || y0 + 1 >= height) {
        return false;
    }
    const float tx = x - x0;
    const float ty = y - y0;
    const float a = image[y0 * stride + x0];
    const float b = image[y0 * stride + x0 + 1];
    const float c = image[(y0 + 1) * stride + x0];
    const float d = image[(y0 + 1) * stride + x0 + 1];
    value = (1.0F - ty) * ((1.0F - tx) * a + tx * b) +
            ty * ((1.0F - tx) * c + tx * d);
    return true;
}

__device__ float warp_sum(float value) {
    for (int offset = warpSize / 2; offset > 0; offset >>= 1) {
        value += __shfl_down_sync(0xffffffffU, value, offset);
    }
    return value;
}

// Reduce six independent accumulators together. With the fixed 128-thread
// launch this replaces six full shared-memory tree reductions (and dozens of
// barriers) with warp shuffles plus two block barriers.
__device__ void block_sum_six(
    float value0,
    float value1,
    float value2,
    float value3,
    float value4,
    float value5,
    float* warp_totals,
    float* totals) {
    const int lane = static_cast<int>(threadIdx.x) & (warpSize - 1);
    const int warp = static_cast<int>(threadIdx.x) / warpSize;
    const int warp_count = static_cast<int>(blockDim.x) / warpSize;
    value0 = warp_sum(value0);
    value1 = warp_sum(value1);
    value2 = warp_sum(value2);
    value3 = warp_sum(value3);
    value4 = warp_sum(value4);
    value5 = warp_sum(value5);
    if (lane == 0) {
        warp_totals[0 * warp_count + warp] = value0;
        warp_totals[1 * warp_count + warp] = value1;
        warp_totals[2 * warp_count + warp] = value2;
        warp_totals[3 * warp_count + warp] = value3;
        warp_totals[4 * warp_count + warp] = value4;
        warp_totals[5 * warp_count + warp] = value5;
    }
    __syncthreads();
    if (warp == 0) {
        value0 = lane < warp_count ? warp_totals[0 * warp_count + lane] : 0.0F;
        value1 = lane < warp_count ? warp_totals[1 * warp_count + lane] : 0.0F;
        value2 = lane < warp_count ? warp_totals[2 * warp_count + lane] : 0.0F;
        value3 = lane < warp_count ? warp_totals[3 * warp_count + lane] : 0.0F;
        value4 = lane < warp_count ? warp_totals[4 * warp_count + lane] : 0.0F;
        value5 = lane < warp_count ? warp_totals[5 * warp_count + lane] : 0.0F;
        value0 = warp_sum(value0);
        value1 = warp_sum(value1);
        value2 = warp_sum(value2);
        value3 = warp_sum(value3);
        value4 = warp_sum(value4);
        value5 = warp_sum(value5);
        if (lane == 0) {
            totals[0] = value0;
            totals[1] = value1;
            totals[2] = value2;
            totals[3] = value3;
            totals[4] = value4;
            totals[5] = value5;
        }
    }
    __syncthreads();
}

__global__ void precompute_reference_kernel(
    const unsigned char* reference,
    int stride,
    const DevicePoint* points,
    int point_count,
    int radius,
    DeviceReferenceStats* reference_stats) {
    const int point_index = blockIdx.x;
    if (point_index >= point_count) {
        return;
    }
    const DevicePoint point = points[point_index];
    const int side = radius * 2 + 1;
    const int sample_count = side * side;
    float h00 = 0.0F;
    float h01 = 0.0F;
    float h11 = 0.0F;
    for (int sample_index = threadIdx.x; sample_index < sample_count;
         sample_index += blockDim.x) {
        const int sx = sample_index % side - radius;
        const int sy = sample_index / side - radius;
        const int index = (point.y + sy) * stride + point.x + sx;
        const float gx = 0.5F * (reference[index + 1] - reference[index - 1]);
        const float gy = 0.5F * (reference[index + stride] - reference[index - stride]);
        h00 += gx * gx;
        h01 += gx * gy;
        h11 += gy * gy;
    }
    __shared__ float warp_totals[6 * 4];
    __shared__ float totals[6];
    block_sum_six(h00, h01, h11, 0.0F, 0.0F, 0.0F, warp_totals, totals);
    if (threadIdx.x == 0) {
        reference_stats[point_index] = DeviceReferenceStats{totals[0], totals[1], totals[2]};
    }
}

__global__ void gauss_newton_kernel(
    const unsigned char* reference,
    const unsigned char* deformed,
    int width,
    int height,
    int stride,
    const DevicePoint* points,
    const DeviceReferenceStats* reference_stats,
    int point_count,
    int radius,
    int max_iterations,
    float tolerance,
    float quality_threshold,
    float maximum_step,
    float fallback_u,
    float fallback_v,
    bool inverse_compositional,
    DeviceResult* results) {
    const int point_index = blockIdx.x;
    if (point_index >= point_count) {
        return;
    }
    const DevicePoint point = points[point_index];
    const DeviceReferenceStats fixed = reference_stats[point_index];
    const int side = radius * 2 + 1;
    const int sample_count = side * side;
    __shared__ float warp_totals[6 * 4];
    __shared__ float totals[7];
    __shared__ float displacement_u;
    __shared__ float displacement_v;
    __shared__ int stop_iteration;
    __shared__ int completed_iterations;

    if (threadIdx.x == 0) {
        const DeviceResult previous = results[point_index];
        // A rejected local track is restarted from the robust median motion of
        // the previously accepted grid, rather than zero or its rejected value.
        // This makes a point recoverable after a transient low-quality frame.
        displacement_u = previous.valid ? previous.u : fallback_u;
        displacement_v = previous.valid ? previous.v : fallback_v;
        stop_iteration = 0;
        completed_iterations = 0;
    }
    __syncthreads();

    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        float h00 = 0.0F;
        float h01 = 0.0F;
        float h11 = 0.0F;
        float b0 = 0.0F;
        float b1 = 0.0F;
        float invalid = 0.0F;
        for (int sample_index = threadIdx.x; sample_index < sample_count;
             sample_index += blockDim.x) {
            const int sx = sample_index % side - radius;
            const int sy = sample_index / side - radius;
            const float reference_value = reference[(point.y + sy) * stride + point.x + sx];
            const int reference_index = (point.y + sy) * stride + point.x + sx;
            float value = 0.0F;
            float gx = 0.0F;
            float gy = 0.0F;
            const bool sampled = inverse_compositional
                ? bilinear_value(
                      deformed,
                      width,
                      height,
                      stride,
                      point.x + sx + displacement_u,
                      point.y + sy + displacement_v,
                      value)
                : bilinear_sample(
                      deformed,
                      width,
                      height,
                      stride,
                      point.x + sx + displacement_u,
                      point.y + sy + displacement_v,
                      value,
                      gx,
                      gy);
            if (!sampled) {
                invalid += 1.0F;
                continue;
            }
            if (inverse_compositional) {
                gx = 0.5F * (reference[reference_index + 1] -
                             reference[reference_index - 1]);
                gy = 0.5F * (reference[reference_index + stride] -
                             reference[reference_index - stride]);
            }
            const float residual = value - reference_value;
            if (!inverse_compositional) {
                h00 += gx * gx;
                h01 += gx * gy;
                h11 += gy * gy;
            }
            b0 += gx * residual;
            b1 += gy * residual;
        }
        block_sum_six(h00, h01, h11, b0, b1, invalid, warp_totals, totals);
        if (threadIdx.x == 0) {
            const float matrix_h00 = inverse_compositional ? fixed.h00 : totals[0];
            const float matrix_h01 = inverse_compositional ? fixed.h01 : totals[1];
            const float matrix_h11 = inverse_compositional ? fixed.h11 : totals[2];
            const float determinant = matrix_h00 * matrix_h11 - matrix_h01 * matrix_h01;
            if (totals[5] > 0.0F || !isfinite(determinant) || fabsf(determinant) < 1.0e-8F) {
                stop_iteration = 2;
            } else {
                float delta_u = (-matrix_h11 * totals[3] + matrix_h01 * totals[4]) / determinant;
                float delta_v = (matrix_h01 * totals[3] - matrix_h00 * totals[4]) / determinant;
                const float length = sqrtf(delta_u * delta_u + delta_v * delta_v);
                if (length > maximum_step) {
                    const float scale = maximum_step / length;
                    delta_u *= scale;
                    delta_v *= scale;
                }
                displacement_u += delta_u;
                displacement_v += delta_v;
                completed_iterations = iteration + 1;
                if (delta_u * delta_u + delta_v * delta_v <= tolerance * tolerance) {
                    stop_iteration = 1;
                }
            }
        }
        __syncthreads();
        if (stop_iteration != 0) {
            break;
        }
    }

    float sum_reference = 0.0F;
    float sum_deformed = 0.0F;
    float sum_reference_squared = 0.0F;
    float sum_deformed_squared = 0.0F;
    float sum_cross = 0.0F;
    float invalid = 0.0F;
    for (int sample_index = threadIdx.x; sample_index < sample_count;
         sample_index += blockDim.x) {
        const int sx = sample_index % side - radius;
        const int sy = sample_index / side - radius;
        const float reference_value = reference[(point.y + sy) * stride + point.x + sx];
        float value = 0.0F;
        float gx = 0.0F;
        float gy = 0.0F;
        if (!bilinear_sample(
                deformed,
                width,
                height,
                stride,
                point.x + sx + displacement_u,
                point.y + sy + displacement_v,
                value,
                gx,
                gy)) {
            invalid += 1.0F;
            continue;
        }
        sum_reference += reference_value;
        sum_deformed += value;
        sum_reference_squared += reference_value * reference_value;
        sum_deformed_squared += value * value;
        sum_cross += reference_value * value;
    }
    block_sum_six(
        sum_reference,
        sum_deformed,
        sum_reference_squared,
        sum_deformed_squared,
        sum_cross,
        invalid,
        warp_totals,
        totals);
    if (threadIdx.x == 0) {
        const float count = static_cast<float>(sample_count);
        const float numerator = totals[4] - totals[0] * totals[1] / count;
        const float energy_reference = totals[2] - totals[0] * totals[0] / count;
        const float energy_deformed = totals[3] - totals[1] * totals[1] / count;
        const float denominator = sqrtf(fmaxf(energy_reference * energy_deformed, 0.0F));
        const float quality = denominator > 1.0e-6F ? numerator / denominator : -1.0F;
        // Reaching the iteration budget is not itself a failed correlation.
        // ZNCC is the acceptance gate; only singular/out-of-bounds solves are
        // rejected unconditionally. This avoids discarding accurate estimates
        // that converge more slowly on hard-edged 8-bit speckle images.
        const bool solver_usable = completed_iterations > 0 && stop_iteration != 2;
        const bool valid = totals[5] == 0.0F && solver_usable && isfinite(quality) &&
                           quality >= quality_threshold;
        results[point_index] = DeviceResult{
            displacement_u,
            displacement_v,
            fminf(fmaxf(quality, -1.0F), 1.0F),
            completed_iterations,
            valid ? 1 : 0};
    }
}

}  // namespace

struct CudaGridEngine::Impl {
    explicit Impl(CudaGridConfig value) : config(value) {
        cuda_check(cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking), "Creating CUDA stream");
        cuda_check(cudaEventCreate(&upload_begin), "Creating CUDA upload event");
        cuda_check(cudaEventCreate(&upload_end), "Creating CUDA upload event");
        cuda_check(cudaEventCreate(&kernel_begin), "Creating CUDA kernel event");
        cuda_check(cudaEventCreate(&kernel_end), "Creating CUDA kernel event");
        cuda_check(cudaEventCreate(&download_begin), "Creating CUDA download event");
        cuda_check(cudaEventCreate(&download_end), "Creating CUDA download event");
    }
    ~Impl() {
        cudaStreamSynchronize(stream);
        cudaFree(reference);
        cudaFree(deformed);
        cudaFree(points);
        cudaFree(results);
        cudaFree(reference_stats);
        cudaFreeHost(reference_staging);
        cudaFreeHost(deformed_staging);
        cudaFreeHost(host_results);
        cudaEventDestroy(upload_begin);
        cudaEventDestroy(upload_end);
        cudaEventDestroy(kernel_begin);
        cudaEventDestroy(kernel_end);
        cudaEventDestroy(download_begin);
        cudaEventDestroy(download_end);
        cudaStreamDestroy(stream);
    }

    void allocate(const Frame& frame) {
        const std::size_t image_bytes = static_cast<std::size_t>(frame.stride) * frame.height;
        std::vector<DevicePoint> host_points;
        const int margin = config.subset_radius + 2;
        for (int y = margin; y + margin < static_cast<int>(frame.height); y += config.grid_step) {
            for (int x = margin; x + margin < static_cast<int>(frame.width); x += config.grid_step) {
                host_points.push_back(DevicePoint{x, y});
            }
        }
        if (host_points.empty()) {
            throw std::invalid_argument("CUDA DIC grid has no valid points");
        }
        cuda_check(cudaFree(reference), "Releasing prior CUDA reference image");
        cuda_check(cudaFree(deformed), "Releasing prior CUDA deformed image");
        cuda_check(cudaFree(points), "Releasing prior CUDA grid");
        cuda_check(cudaFree(results), "Releasing prior CUDA results");
        cuda_check(cudaFree(reference_stats), "Releasing prior CUDA reference statistics");
        cuda_check(cudaFreeHost(reference_staging), "Releasing prior pinned reference image");
        cuda_check(cudaFreeHost(deformed_staging), "Releasing prior pinned deformed image");
        cuda_check(cudaFreeHost(host_results), "Releasing prior pinned CUDA results");
        reference = nullptr;
        deformed = nullptr;
        points = nullptr;
        results = nullptr;
        reference_stats = nullptr;
        reference_staging = nullptr;
        deformed_staging = nullptr;
        host_results = nullptr;
        cuda_check(cudaMalloc(reinterpret_cast<void**>(&reference), image_bytes),
                   "Allocating CUDA reference image");
        cuda_check(cudaMalloc(reinterpret_cast<void**>(&deformed), image_bytes),
                   "Allocating CUDA deformed image");
        cuda_check(cudaMalloc(reinterpret_cast<void**>(&points), host_points.size() * sizeof(DevicePoint)),
                   "Allocating CUDA grid");
        cuda_check(cudaMalloc(reinterpret_cast<void**>(&results), host_points.size() * sizeof(DeviceResult)),
                   "Allocating CUDA results");
        cuda_check(cudaMalloc(
            reinterpret_cast<void**>(&reference_stats),
            host_points.size() * sizeof(DeviceReferenceStats)),
            "Allocating CUDA reference statistics");
        cuda_check(cudaHostAlloc(
            reinterpret_cast<void**>(&reference_staging), image_bytes, cudaHostAllocPortable),
            "Allocating pinned reference staging image");
        cuda_check(cudaHostAlloc(
            reinterpret_cast<void**>(&deformed_staging), image_bytes, cudaHostAllocPortable),
            "Allocating pinned deformed staging image");
        cuda_check(cudaHostAlloc(
            reinterpret_cast<void**>(&host_results),
            host_points.size() * sizeof(DeviceResult),
            cudaHostAllocPortable),
            "Allocating pinned CUDA result buffer");
        cuda_check(cudaMemcpy(
            points,
            host_points.data(),
            host_points.size() * sizeof(DevicePoint),
            cudaMemcpyHostToDevice), "Uploading CUDA grid");
        cuda_check(cudaMemset(results, 0, host_points.size() * sizeof(DeviceResult)),
                   "Initializing CUDA displacement state");
        host_result_count = host_points.size();
        seed_scratch_u.reserve(host_points.size());
        seed_scratch_v.reserve(host_points.size());
        grid = std::move(host_points);
        width = frame.width;
        height = frame.height;
        stride = frame.stride;
        reference_pixels = nullptr;
        reference_timestamp = 0;
        fallback_u = 0.0F;
        fallback_v = 0.0F;
    }

    CudaGridConfig config;
    unsigned char* reference{};
    unsigned char* deformed{};
    unsigned char* reference_staging{};
    unsigned char* deformed_staging{};
    DevicePoint* points{};
    DeviceResult* results{};
    DeviceReferenceStats* reference_stats{};
    DeviceResult* host_results{};
    std::size_t host_result_count{};
    std::vector<DevicePoint> grid;
    std::vector<float> seed_scratch_u;
    std::vector<float> seed_scratch_v;
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride{};
    const unsigned char* reference_pixels{};
    std::uint64_t reference_timestamp{};
    float fallback_u{};
    float fallback_v{};
    cudaStream_t stream{};
    cudaEvent_t upload_begin{};
    cudaEvent_t upload_end{};
    cudaEvent_t kernel_begin{};
    cudaEvent_t kernel_end{};
    cudaEvent_t download_begin{};
    cudaEvent_t download_end{};
    EngineStageTiming timing{};
};

CudaGridEngine::CudaGridEngine(CudaGridConfig config) : impl_(std::make_unique<Impl>(config)) {
    if (config.subset_radius < 2 || config.grid_step < 1 || config.max_iterations < 1 ||
        config.convergence_tolerance <= 0.0F || config.quality_threshold < -1.0F ||
        config.quality_threshold > 1.0F || config.maximum_iteration_step <= 0.0F ||
        config.recovery_trigger_valid_ratio < 0.0F ||
        config.recovery_trigger_valid_ratio > 1.0F || config.max_recovery_passes < 0 ||
        config.max_recovery_passes > 4) {
        throw std::invalid_argument("Invalid CUDA DIC grid configuration");
    }
}

CudaGridEngine::~CudaGridEngine() = default;

std::string_view CudaGridEngine::name() const noexcept {
    return impl_->config.inverse_compositional ? "CUDA-IC-GN-Grid" : "CUDA-FA-GN-Grid";
}

DicResult CudaGridEngine::process(const Frame& reference, const Frame& deformed) {
    if (reference.width != deformed.width || reference.height != deformed.height ||
        reference.stride != deformed.stride) {
        throw std::invalid_argument("Reference and deformed images must have equal layout");
    }
    const auto start = std::chrono::steady_clock::now();
    if (impl_->width != reference.width || impl_->height != reference.height ||
        impl_->stride != reference.stride) {
        impl_->allocate(reference);
    }
    const std::size_t image_bytes = static_cast<std::size_t>(reference.stride) * reference.height;
    impl_->timing = {};
    const auto staging_start = std::chrono::steady_clock::now();
    const bool upload_reference = impl_->reference_pixels != reference.pixels.data() ||
                                  impl_->reference_timestamp != reference.timestamp_ns;
    const unsigned char* reference_source = reference.pixels.data();
    const unsigned char* deformed_source = deformed.pixels.data();
    if (upload_reference && !reference.pixels.pinned()) {
        std::memcpy(impl_->reference_staging, reference.pixels.data(), image_bytes);
        reference_source = impl_->reference_staging;
    }
    if (!deformed.pixels.pinned()) {
        std::memcpy(impl_->deformed_staging, deformed.pixels.data(), image_bytes);
        deformed_source = impl_->deformed_staging;
    }
    impl_->timing.host_staging_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - staging_start).count();

    cuda_check(cudaEventRecord(impl_->upload_begin, impl_->stream), "Recording CUDA upload start");
    if (upload_reference) {
        cuda_check(cudaMemcpyAsync(
            impl_->reference,
            reference_source,
            image_bytes,
            cudaMemcpyHostToDevice,
            impl_->stream), "Uploading CUDA reference image");
        cuda_check(cudaMemsetAsync(
            impl_->results,
            0,
            impl_->grid.size() * sizeof(DeviceResult),
            impl_->stream), "Resetting CUDA displacement state");
        impl_->reference_pixels = reference.pixels.data();
        impl_->reference_timestamp = reference.timestamp_ns;
        if (impl_->config.inverse_compositional) {
            precompute_reference_kernel<<<
                static_cast<unsigned int>(impl_->grid.size()), 128, 0, impl_->stream>>>(
                impl_->reference,
                static_cast<int>(impl_->stride),
                impl_->points,
                static_cast<int>(impl_->grid.size()),
                impl_->config.subset_radius,
                impl_->reference_stats);
            cuda_check(cudaGetLastError(), "Launching CUDA reference precomputation");
        }
    }
    cuda_check(cudaMemcpyAsync(
        impl_->deformed,
        deformed_source,
        image_bytes,
        cudaMemcpyHostToDevice,
        impl_->stream), "Uploading CUDA deformed image");
    cuda_check(cudaEventRecord(impl_->upload_end, impl_->stream), "Recording CUDA upload end");

    const std::size_t minimum_seed_points = std::max<std::size_t>(8, impl_->grid.size() / 100);
    const auto launch_and_download = [&]() {
        cuda_check(cudaEventRecord(impl_->kernel_begin, impl_->stream), "Recording CUDA kernel start");
        gauss_newton_kernel<<<static_cast<unsigned int>(impl_->grid.size()), 128, 0, impl_->stream>>>(
            impl_->reference,
            impl_->deformed,
            static_cast<int>(impl_->width),
            static_cast<int>(impl_->height),
            static_cast<int>(impl_->stride),
            impl_->points,
            impl_->reference_stats,
            static_cast<int>(impl_->grid.size()),
            impl_->config.subset_radius,
            impl_->config.max_iterations,
            impl_->config.convergence_tolerance,
            impl_->config.quality_threshold,
            impl_->config.maximum_iteration_step,
            impl_->fallback_u,
            impl_->fallback_v,
            impl_->config.inverse_compositional,
            impl_->results);
        cuda_check(cudaGetLastError(), "Launching CUDA DIC kernel");
        cuda_check(cudaEventRecord(impl_->kernel_end, impl_->stream), "Recording CUDA kernel end");
        cuda_check(cudaEventRecord(impl_->download_begin, impl_->stream), "Recording CUDA download start");
        cuda_check(cudaMemcpyAsync(
            impl_->host_results,
            impl_->results,
            impl_->host_result_count * sizeof(DeviceResult),
            cudaMemcpyDeviceToHost,
            impl_->stream), "Downloading CUDA DIC results");
        cuda_check(cudaEventRecord(impl_->download_end, impl_->stream), "Recording CUDA download end");
        cuda_check(cudaEventSynchronize(impl_->download_end), "Waiting for CUDA DIC result");
        float milliseconds = 0.0F;
        cuda_check(cudaEventElapsedTime(&milliseconds, impl_->kernel_begin, impl_->kernel_end),
                   "Measuring CUDA kernel time");
        impl_->timing.kernel_ms += milliseconds;
        cuda_check(cudaEventElapsedTime(&milliseconds, impl_->download_begin, impl_->download_end),
                   "Measuring CUDA download time");
        impl_->timing.d2h_ms += milliseconds;
        ++impl_->timing.kernel_launches;
    };
    const auto update_robust_seed = [&]() {
        impl_->seed_scratch_u.clear();
        impl_->seed_scratch_v.clear();
        for (std::size_t index = 0; index < impl_->host_result_count; ++index) {
            const auto& result = impl_->host_results[index];
            if (result.valid != 0 && std::isfinite(result.u) && std::isfinite(result.v)) {
                impl_->seed_scratch_u.push_back(result.u);
                impl_->seed_scratch_v.push_back(result.v);
            }
        }
        if (impl_->seed_scratch_u.size() >= minimum_seed_points) {
            const std::size_t middle = impl_->seed_scratch_u.size() / 2;
            std::nth_element(
                impl_->seed_scratch_u.begin(),
                impl_->seed_scratch_u.begin() + static_cast<std::ptrdiff_t>(middle),
                impl_->seed_scratch_u.end());
            std::nth_element(
                impl_->seed_scratch_v.begin(),
                impl_->seed_scratch_v.begin() + static_cast<std::ptrdiff_t>(middle),
                impl_->seed_scratch_v.end());
            impl_->fallback_u = impl_->seed_scratch_u[middle];
            impl_->fallback_v = impl_->seed_scratch_v[middle];
        }
        return impl_->seed_scratch_u.size();
    };

    launch_and_download();
    {
        float milliseconds = 0.0F;
        cuda_check(cudaEventElapsedTime(&milliseconds, impl_->upload_begin, impl_->upload_end),
                   "Measuring CUDA upload time");
        impl_->timing.h2d_ms = milliseconds;
    }
    std::size_t valid_count = update_robust_seed();
    for (int pass = 0; pass < impl_->config.max_recovery_passes; ++pass) {
        const float valid_ratio = static_cast<float>(valid_count) /
                                  static_cast<float>(impl_->grid.size());
        if (valid_count < minimum_seed_points ||
            valid_ratio >= impl_->config.recovery_trigger_valid_ratio) {
            break;
        }
        launch_and_download();
        valid_count = update_robust_seed();
    }

    DicResult output;
    output.frame_sequence = deformed.sequence;
    output.frame_timestamp_ns = deformed.timestamp_ns;
    output.points.reserve(impl_->grid.size());
    for (std::size_t index = 0; index < impl_->grid.size(); ++index) {
        const auto& point = impl_->grid[index];
        const auto& result = impl_->host_results[index];
        output.points.push_back(DicPoint{
            point.x,
            point.y,
            result.u,
            result.v,
            result.quality,
            result.valid != 0});
    }
    output.processing_ms = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    return output;
}

EngineStageTiming CudaGridEngine::last_stage_timing() const noexcept {
    return impl_->timing;
}

}  // namespace p2dic
