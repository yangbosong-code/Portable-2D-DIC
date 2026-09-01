#include "p2dic/subpixel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace p2dic {
namespace {

struct Sample {
    double value{};
    double gx{};
    double gy{};
};

bool sample_bilinear(const Frame& frame, double x, double y, Sample& output) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    if (x0 < 0 || y0 < 0 || x0 + 1 >= static_cast<int>(frame.width) ||
        y0 + 1 >= static_cast<int>(frame.height)) {
        return false;
    }

    const double tx = x - x0;
    const double ty = y - y0;
    const auto index = [&frame](int px, int py) {
        return static_cast<std::size_t>(py) * frame.stride + static_cast<std::size_t>(px);
    };
    const double a = frame.pixels[index(x0, y0)];
    const double b = frame.pixels[index(x0 + 1, y0)];
    const double c = frame.pixels[index(x0, y0 + 1)];
    const double d = frame.pixels[index(x0 + 1, y0 + 1)];

    output.value = (1.0 - ty) * ((1.0 - tx) * a + tx * b) +
                   ty * ((1.0 - tx) * c + tx * d);
    output.gx = (1.0 - ty) * (b - a) + ty * (d - c);
    output.gy = (1.0 - tx) * (c - a) + tx * (d - b);
    return true;
}

}  // namespace

SubpixelTranslationResult refine_translation_znssd(
    const Frame& reference,
    const Frame& deformed,
    int center_x,
    int center_y,
    double initial_u,
    double initial_v,
    const SubpixelConfig& config) {
    if (reference.width != deformed.width || reference.height != deformed.height) {
        throw std::invalid_argument("Reference and deformed images must have equal dimensions");
    }
    if (config.subset_radius < 2 || config.max_iterations < 1 ||
        config.convergence_tolerance <= 0.0 || config.minimum_texture_energy <= 0.0) {
        throw std::invalid_argument("Invalid subpixel refinement configuration");
    }

    const int radius = config.subset_radius;
    if (center_x - radius < 0 || center_y - radius < 0 ||
        center_x + radius >= static_cast<int>(reference.width) ||
        center_y + radius >= static_cast<int>(reference.height)) {
        throw std::invalid_argument("Reference subset exceeds image boundary");
    }

    const int side = 2 * radius + 1;
    const std::size_t count = static_cast<std::size_t>(side) * side;
    std::vector<double> ref(count);
    double ref_mean = 0.0;
    std::size_t k = 0;
    for (int sy = -radius; sy <= radius; ++sy) {
        for (int sx = -radius; sx <= radius; ++sx) {
            const double value = reference.pixels[
                static_cast<std::size_t>(center_y + sy) * reference.stride + center_x + sx];
            ref[k++] = value;
            ref_mean += value;
        }
    }
    ref_mean /= static_cast<double>(count);
    double ref_energy = 0.0;
    for (double& value : ref) {
        value -= ref_mean;
        ref_energy += value * value;
    }
    if (ref_energy < config.minimum_texture_energy) {
        return {};
    }
    const double ref_norm = std::sqrt(ref_energy);

    std::vector<Sample> samples(count);
    std::vector<double> deformed_zero_mean(count);
    std::vector<double> gx_zero_mean(count);
    std::vector<double> gy_zero_mean(count);
    double u = initial_u;
    double v = initial_v;
    SubpixelTranslationResult result;

    for (int iteration = 0; iteration < config.max_iterations; ++iteration) {
        double mean = 0.0;
        double mean_gx = 0.0;
        double mean_gy = 0.0;
        k = 0;
        for (int sy = -radius; sy <= radius; ++sy) {
            for (int sx = -radius; sx <= radius; ++sx) {
                Sample sample;
                if (!sample_bilinear(
                        deformed, center_x + sx + u, center_y + sy + v, sample)) {
                    result.u = u;
                    result.v = v;
                    result.iterations = iteration;
                    return result;
                }
                samples[k++] = sample;
                mean += sample.value;
                mean_gx += sample.gx;
                mean_gy += sample.gy;
            }
        }
        const double inverse_count = 1.0 / static_cast<double>(count);
        mean *= inverse_count;
        mean_gx *= inverse_count;
        mean_gy *= inverse_count;

        double energy = 0.0;
        double dot_x = 0.0;
        double dot_y = 0.0;
        double correlation_numerator = 0.0;
        for (k = 0; k < count; ++k) {
            const double d0 = samples[k].value - mean;
            const double gx0 = samples[k].gx - mean_gx;
            const double gy0 = samples[k].gy - mean_gy;
            deformed_zero_mean[k] = d0;
            gx_zero_mean[k] = gx0;
            gy_zero_mean[k] = gy0;
            energy += d0 * d0;
            dot_x += d0 * gx0;
            dot_y += d0 * gy0;
            correlation_numerator += ref[k] * d0;
        }
        if (energy < config.minimum_texture_energy) {
            result.u = u;
            result.v = v;
            result.iterations = iteration;
            return result;
        }

        const double norm = std::sqrt(energy);
        const double inverse_norm = 1.0 / norm;
        const double inverse_norm_cubed = 1.0 / (energy * norm);
        double h00 = 0.0;
        double h01 = 0.0;
        double h11 = 0.0;
        double b0 = 0.0;
        double b1 = 0.0;
        for (k = 0; k < count; ++k) {
            const double d0 = deformed_zero_mean[k];
            const double jx = gx_zero_mean[k] * inverse_norm - d0 * dot_x * inverse_norm_cubed;
            const double jy = gy_zero_mean[k] * inverse_norm - d0 * dot_y * inverse_norm_cubed;
            const double residual = d0 * inverse_norm - ref[k] / ref_norm;
            h00 += jx * jx;
            h01 += jx * jy;
            h11 += jy * jy;
            b0 += jx * residual;
            b1 += jy * residual;
        }

        const double determinant = h00 * h11 - h01 * h01;
        if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-15) {
            result.u = u;
            result.v = v;
            result.iterations = iteration + 1;
            return result;
        }
        const double delta_u = (-h11 * b0 + h01 * b1) / determinant;
        const double delta_v = (h01 * b0 - h00 * b1) / determinant;
        u += delta_u;
        v += delta_v;

        result.u = u;
        result.v = v;
        result.zncc = std::clamp(correlation_numerator / (ref_norm * norm), -1.0, 1.0);
        result.iterations = iteration + 1;
        result.valid = std::isfinite(u) && std::isfinite(v) && std::isfinite(result.zncc);
        if (!result.valid) {
            return result;
        }
        if (delta_u * delta_u + delta_v * delta_v <=
            config.convergence_tolerance * config.convergence_tolerance) {
            result.converged = true;
            return result;
        }
    }
    return result;
}

}  // namespace p2dic
