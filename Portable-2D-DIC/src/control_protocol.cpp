#include "p2dic/control_protocol.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace p2dic {
namespace {

bool safe_token(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-' ||
               character == '.' || character == '=' || character == ':' || character == '/';
    });
}

std::string sanitize_message(std::string_view message) {
    std::string output(message);
    std::replace_if(output.begin(), output.end(), [](unsigned char character) {
        return character == '\r' || character == '\n' || character < 0x20;
    }, ' ');
    return output;
}

}  // namespace

ControlCommand parse_control_command(std::string_view line) {
    if (line.empty() || line.size() > 4096) {
        throw std::invalid_argument("Control command length is invalid");
    }
    if (line.find('\r') != std::string_view::npos || line.find('\n') != std::string_view::npos ||
        line.find('\0') != std::string_view::npos) {
        throw std::invalid_argument("Control command contains a forbidden character");
    }

    std::istringstream stream{std::string(line)};
    ControlCommand command;
    std::string token;
    while (stream >> token) {
        if (!safe_token(token)) {
            throw std::invalid_argument("Control command contains an invalid token");
        }
        if (command.verb.empty()) {
            command.verb = std::move(token);
        } else {
            command.arguments.push_back(std::move(token));
        }
    }
    if (command.verb.empty()) {
        throw std::invalid_argument("Control command is empty");
    }
    std::transform(command.verb.begin(), command.verb.end(), command.verb.begin(),
                   [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
    return command;
}

std::string make_ok_response(std::string_view code, std::string_view fields) {
    if (!safe_token(code)) {
        throw std::invalid_argument("Response code is invalid");
    }
    std::string output = "OK ";
    output += code;
    if (!fields.empty()) {
        output += ' ';
        output += sanitize_message(fields);
    }
    output += '\n';
    return output;
}

std::string make_error_response(std::string_view code, std::string_view message) {
    if (!safe_token(code)) {
        throw std::invalid_argument("Error code is invalid");
    }
    std::string output = "ERR ";
    output += code;
    if (!message.empty()) {
        output += ' ';
        output += sanitize_message(message);
    }
    output += '\n';
    return output;
}

}  // namespace p2dic
