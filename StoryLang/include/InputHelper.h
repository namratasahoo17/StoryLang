#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

namespace storylang {

// Thrown when stdin is closed (Ctrl-D / end of piped input) so that no prompt
// loop can spin forever on a dead stream.
struct InputClosed : std::runtime_error {
    InputClosed() : std::runtime_error("input closed") {}
};

namespace input {

std::string trim(const std::string& s);
std::string toLower(std::string s);
bool parseInt(const std::string& s, long& out);  // whole string must be a number

std::string readLine(const std::string& prompt);
std::string readNonEmpty(const std::string& prompt);
int readInt(const std::string& prompt, int min, int max);
std::optional<int> readIntOrBlank(const std::string& prompt, int min, int max);
bool confirm(const std::string& prompt);
std::string readMultiline(const std::string& prompt);  // ends with a lone "."
void pause();

// Greedy word-wrap; respects existing newlines.
std::string wrapText(const std::string& text, std::size_t width, const std::string& indent = "");
std::string clip(const std::string& s, std::size_t maxLen);

}  // namespace input
}  // namespace storylang
