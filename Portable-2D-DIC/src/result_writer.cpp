#include "p2dic/result_writer.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace p2dic {
namespace {

std::string json_escape(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += character; break;
        }
    }
    return output;
}

}  // namespace

ResultWriter::ResultWriter(std::size_t flush_every_frames, bool csv_enabled)
    : flush_every_frames_(flush_every_frames), csv_enabled_(csv_enabled) {
    if (flush_every_frames_ == 0) {
        throw std::invalid_argument("Result flush interval must be at least one frame");
    }
}

ResultWriter::~ResultWriter() {
    try {
        close("interrupted");
    } catch (...) {
    }
}

void ResultWriter::open(const std::filesystem::path& root, const SessionInfo& info) {
    if (session_open_) {
        throw std::logic_error("A result session is already open");
    }
    if (info.session_id.empty() || info.width == 0 || info.height == 0 ||
        info.requested_fps <= 0.0 || !std::isfinite(info.requested_fps) ||
        info.millimeters_per_pixel < 0.0 || !std::isfinite(info.millimeters_per_pixel)) {
        throw std::invalid_argument("Session information is incomplete");
    }
    session_path_ = root / info.session_id;
    if (std::filesystem::exists(session_path_)) {
        throw std::runtime_error("Session directory already exists: " + session_path_.string());
    }
    std::filesystem::create_directories(session_path_);
    info_ = info;
    write_manifest(info_, "recording");

    if (csv_enabled_) {
        results_.open(session_path_ / "results.csv", std::ios::out | std::ios::trunc);
        if (!results_) {
            throw std::runtime_error("Unable to create session results.csv");
        }
        results_ << "frame_sequence,timestamp_ns,processing_ms,x,y,u,v,quality,valid,"
                    "exx,eyy,exy,strain_valid\n";
        results_.flush();
    }
    frames_since_flush_ = 0;
    session_open_ = true;
}

void ResultWriter::append(const DicResult& result) {
    if (!session_open_) {
        throw std::logic_error("Cannot append without an open result session");
    }
    if (!csv_enabled_) return;
    results_ << std::setprecision(9);
    for (const auto& point : result.points) {
        results_ << result.frame_sequence << ',' << result.frame_timestamp_ns << ','
                 << result.processing_ms << ',' << point.x << ',' << point.y << ','
                 << point.u << ',' << point.v << ',' << point.quality << ','
                 << (point.valid ? 1 : 0) << ',' << point.exx << ',' << point.eyy << ','
                 << point.exy << ',' << (point.strain_valid ? 1 : 0) << '\n';
    }
    if (!results_) {
        throw std::runtime_error("Writing DIC results failed");
    }
    if (++frames_since_flush_ >= flush_every_frames_) {
        results_.flush();
        if (!results_) {
            throw std::runtime_error("Flushing DIC results failed");
        }
        frames_since_flush_ = 0;
    }
}

void ResultWriter::close(std::string_view status) {
    if (!session_open_) {
        return;
    }
    if (csv_enabled_) results_.flush();
    if (csv_enabled_ && !results_) {
        results_.close();
        session_open_ = false;
        throw std::runtime_error("Final DIC result flush failed");
    }
    if (csv_enabled_) results_.close();
    if (status != "complete" && status != "interrupted" && status != "faulted") {
        throw std::invalid_argument("Session final status is invalid");
    }
    write_manifest(info_, status);
    session_open_ = false;
}

bool ResultWriter::is_open() const noexcept {
    return session_open_;
}

const std::filesystem::path& ResultWriter::session_path() const noexcept {
    return session_path_;
}

void ResultWriter::write_manifest(const SessionInfo& info, std::string_view status) {
    const auto temporary = session_path_ / "manifest.json.partial";
    const auto final = session_path_ / "manifest.json";
    {
        std::ofstream stream(temporary, std::ios::out | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("Unable to create session manifest");
        }
        stream << "{\n"
               << "  \"schema_version\": 2,\n"
               << "  \"session_id\": \"" << json_escape(info.session_id) << "\",\n"
               << "  \"camera_name\": \"" << json_escape(info.camera_name) << "\",\n"
               << "  \"width\": " << info.width << ",\n"
               << "  \"height\": " << info.height << ",\n"
               << "  \"requested_fps\": " << std::setprecision(8) << info.requested_fps << ",\n"
               << "  \"millimeters_per_pixel\": ";
        if (info.millimeters_per_pixel > 0.0) {
            stream << std::setprecision(12) << info.millimeters_per_pixel;
        } else {
            stream << "null";
        }
        stream << ",\n"
               << "  \"status\": \"" << status << "\"\n"
               << "}\n";
        stream.flush();
        if (!stream) {
            throw std::runtime_error("Writing session manifest failed");
        }
    }
    std::error_code error;
    if (std::filesystem::exists(final, error)) {
        std::filesystem::remove(final, error);
        if (error) {
            throw std::runtime_error("Unable to replace session manifest: " + error.message());
        }
    }
    std::filesystem::rename(temporary, final, error);
    if (error) {
        throw std::runtime_error("Unable to commit session manifest: " + error.message());
    }
}

}  // namespace p2dic
