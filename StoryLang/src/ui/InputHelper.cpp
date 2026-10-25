#include "InputHelper.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace storylang {
namespace input {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool parseInt(const std::string& s, long& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) throw InputClosed();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

std::string readNonEmpty(const std::string& prompt) {
    for (;;) {
        std::string s = trim(readLine(prompt));
        if (!s.empty()) return s;
        std::cout << "  Input cannot be empty.\n";
    }
}

int readInt(const std::string& prompt, int min, int max) {
    for (;;) {
        std::string s = trim(readLine(prompt));
        long v = 0;
        if (parseInt(s, v) && v >= min && v <= max) return static_cast<int>(v);
        std::cout << "  Please enter a whole number between " << min << " and " << max << ".\n";
    }
}

std::optional<int> readIntOrBlank(const std::string& prompt, int min, int max) {
    for (;;) {
        std::string s = trim(readLine(prompt));
        if (s.empty()) return std::nullopt;
        long v = 0;
        if (parseInt(s, v) && v >= min && v <= max) return static_cast<int>(v);
        std::cout << "  Please enter a whole number between " << min << " and " << max
                  << " (or leave blank).\n";
    }
}

bool confirm(const std::string& prompt) {
    for (;;) {
        std::string s = toLower(trim(readLine(prompt + " [y/n]: ")));
        if (s == "y" || s == "yes") return true;
        if (s == "n" || s == "no") return false;
        std::cout << "  Please answer y or n.\n";
    }
}

std::string readMultiline(const std::string& prompt) {
    std::cout << prompt << " (finish with a single '.' on its own line)\n";
    std::string text;
    for (;;) {
        std::string line = readLine("| ");
        if (trim(line) == ".") break;
        if (!text.empty()) text += "\n";
        text += line;
    }
    return text;
}

void pause() { readLine("\nPress Enter to continue..."); }

std::string wrapText(const std::string& text, std::size_t width, const std::string& indent) {
    std::istringstream paragraphs(text);
    std::string para, out;
    while (std::getline(paragraphs, para)) {
        std::istringstream words(para);
        std::string word, line = indent;
        bool lineHasWord = false;
        while (words >> word) {
            if (lineHasWord && line.size() + 1 + word.size() > width) {
                out += line + "\n";
                line = indent;
                lineHasWord = false;
            }
            if (lineHasWord) line += " ";
            line += word;
            lineHasWord = true;
        }
        out += line + "\n";
    }
    if (!out.empty()) out.pop_back();  // drop the final newline
    return out;
}

std::string clip(const std::string& s, std::size_t maxLen) {
    if (s.size() <= maxLen) return s;
    if (maxLen < 4) return s.substr(0, maxLen);
    return s.substr(0, maxLen - 3) + "...";
}

}  // namespace input
}  // namespace storylang
