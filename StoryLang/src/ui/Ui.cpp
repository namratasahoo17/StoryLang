#include "Ui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#define SL_ISATTY(f) _isatty(_fileno(f))
#else
#include <unistd.h>
#define SL_ISATTY(f) isatty(fileno(f))
#endif

namespace storylang {
namespace ui {

namespace {

bool g_color = false, g_unicode = false, g_interactive = false, g_classic = false;

const Glyphs kUnicode = {"╭", "╮", "╰", "╯", "─", "│",
                         "●", "❯", "✓", "✗", "★",
                         "█", "░",
                         "├── ", "└── ", "│",
                         "▸", "→", "·", "“", "”"};
// Safe for the legacy Windows console font (Consolas / Lucida): only characters it has.
const Glyphs kClassic = {"┌", "┐", "└", "┘", "─", "│",
                         "•", "»", "√", "×", "*",
                         "█", "░",
                         "├── ", "└── ", "│",
                         "»", "→", "·", "“", "”"};
const Glyphs kAscii = {"+", "+", "+", "+", "-", "|",
                       "*", ">", "+", "x", "*",
                       "#", ".",
                       "|-- ", "`-- ", "|",
                       ">", "->", "|", "\"", "\""};

int code(Color c) {  // xterm 256-colour palette numbers
    switch (c) {
        case Color::Primary: return 81;   // sky cyan
        case Color::Accent:  return 141;  // soft violet
        case Color::Pink:    return 213;
        case Color::Gold:    return 221;
        case Color::Good:    return 78;   // mint green
        case Color::Warn:    return 214;  // amber
        case Color::Bad:     return 203;  // coral red
        case Color::Dim:     return 244;  // grey
        default:             return -1;
    }
}

void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    usleep(static_cast<useconds_t>(ms) * 1000);
#endif
}

void typeOut(const std::string& s, int ms) {
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == '\033') {  // colour code: print instantly, no delay
            std::size_t j = i + 1;
            while (j < s.size() && !std::isalpha(static_cast<unsigned char>(s[j]))) ++j;
            if (j < s.size()) ++j;
            std::cout << s.substr(i, j - i);
            i = j;
            continue;
        }
        unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        std::cout << s.substr(i, len) << std::flush;
        sleepMs(ms);
        i += len;
    }
}

}  // namespace

// ------------------------------------------------------------------ setup
void init(bool forceColor, bool forcePlain) {
    const bool outTty = SL_ISATTY(stdout) != 0;
    const bool inTty = SL_ISATTY(stdin) != 0;
    const char* term = std::getenv("TERM");
    const bool dumb = term && std::string(term) == "dumb";
    const bool noColor = std::getenv("NO_COLOR") != nullptr;

    g_interactive = outTty && inTty && !forcePlain;
    g_color = !forcePlain && (forceColor || (outTty && !noColor && !dumb));
    g_unicode = !forcePlain && (forceColor || (outTty && !dumb));

#ifdef _WIN32
    if (outTty && !forcePlain) {
        SetConsoleOutputCP(65001);  // UTF-8
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (!(GetConsoleMode(h, &mode) && SetConsoleMode(h, mode | 0x0004))) {  // no ANSI support
            g_color = false;
            g_unicode = false;
        }
        g_classic = std::getenv("WT_SESSION") == nullptr;  // old console font => safe glyphs
    }
#endif
}

bool colorOn() { return g_color; }
bool unicodeOn() { return g_unicode; }
bool interactive() { return g_interactive; }
const Glyphs& g() { return !g_unicode ? kAscii : (g_classic ? kClassic : kUnicode); }
void setClassic(bool on) { g_classic = on; }

// ----------------------------------------------------------- text helpers
std::string paint(Color c, const std::string& s, bool bold) {
    if (!g_color) return s;
    int k = code(c);
    if (k < 0 && !bold) return s;
    std::string out = "\033[";
    if (bold) out += "1";
    if (k >= 0) out += std::string(bold ? ";" : "") + "38;5;" + std::to_string(k);
    return out + "m" + s + "\033[0m";
}

std::string badge(const std::string& label, Color background) {
    if (!g_color) return "[" + label + "]";
    return "\033[1;38;5;16;48;5;" + std::to_string(code(background)) + "m " + label + " \033[0m";
}

std::size_t displayWidth(const std::string& s) {
    std::size_t w = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0x1B) {  // skip an ANSI escape sequence
            while (i < s.size() && !std::isalpha(static_cast<unsigned char>(s[i])) ) ++i;
            continue;
        }
        if ((c & 0xC0) != 0x80) ++w;  // count every UTF-8 lead byte once
    }
    return w;
}

std::string repeat(const std::string& s, int n) {
    std::string r;
    for (int i = 0; i < n; ++i) r += s;
    return r;
}

std::string padRight(const std::string& s, std::size_t width) {
    std::size_t w = displayWidth(s);
    return w >= width ? s : s + std::string(width - w, ' ');
}

std::string padLeft(const std::string& s, std::size_t width) {
    std::size_t w = displayWidth(s);
    return w >= width ? s : std::string(width - w, ' ') + s;
}

std::string center(const std::string& s, std::size_t width) {
    std::size_t w = displayWidth(s);
    if (w >= width) return s;
    std::size_t left = (width - w) / 2;
    return std::string(left, ' ') + s;
}

std::string clip(const std::string& s, std::size_t maxWidth) {
    if (displayWidth(s) <= maxWidth) return s;
    std::size_t keep = maxWidth > 3 ? maxWidth - 3 : maxWidth;
    std::string out;
    std::size_t count = 0;
    for (std::size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        if (count == keep) break;
        out += s.substr(i, len);
        i += len;
        ++count;
    }
    return maxWidth > 3 ? out + "..." : out;
}

std::vector<std::string> wrapLines(const std::string& text, std::size_t width,
                                   const std::string& indent) {
    std::vector<std::string> out;
    std::istringstream paragraphs(text);
    std::string para;
    while (std::getline(paragraphs, para)) {
        std::istringstream words(para);
        std::string word, line;
        bool has = false;
        while (words >> word) {
            if (has && displayWidth(indent) + displayWidth(line) + 1 + displayWidth(word) > width) {
                out.push_back(indent + line);
                line.clear();
                has = false;
            }
            if (has) line += " ";
            line += word;
            has = true;
        }
        out.push_back(has ? indent + line : std::string());
    }
    if (out.empty()) out.push_back("");
    return out;
}

// ---------------------------------------------------------------- widgets
void clearScreen() {
    if (g_color && g_interactive) std::cout << "\033[2J\033[3J\033[H" << std::flush;
}

void banner() {
    std::cout << "\n";
    if (g_color && g_unicode) {
        const char* art[3] = {"╔═╗╔╦╗╔═╗╦═╗╦ ╦╦  ╔═╗╔╗╔╔═╗",
                              "╚═╗ ║ ║ ║╠╦╝╚╦╝║  ╠═╣║║║║ ╦",
                              "╚═╝ ╩ ╚═╝╩╚═ ╩ ╩═╝╩ ╩╝╚╝╚═╝"};
        const int fg[3] = {81, 141, 213};
        const std::string bg = "\033[48;5;17m", reset = "\033[0m";
        auto blank = [&]() { std::cout << bg << std::string(kWidth, ' ') << reset << "\n"; };
        auto line = [&](const std::string& text, int w, const std::string& fgCode) {
            int left = (kWidth - w) / 2;
            std::cout << bg << std::string(left, ' ') << fgCode << text
                      << std::string(kWidth - left - w, ' ') << reset << "\n";
        };
        blank();
        for (int i = 0; i < 3; ++i) line(art[i], 27, "\033[1;38;5;" + std::to_string(fg[i]) + "m");
        blank();
        const std::string tag = "interactive narrative design & exploration";
        line(tag, static_cast<int>(tag.size()), "\033[38;5;250m");
        blank();
    } else {
        rule();
        std::cout << center("S T O R Y L A N G", kWidth) << "\n";
        std::cout << center("interactive narrative design & exploration", kWidth) << "\n";
        rule();
    }
    std::cout << "\n";
}

namespace {
std::string spaced(const std::string& s) {  // "Inn Door" -> "I N N   D O O R"
    std::string out;
    bool first = true;
    for (std::size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        if (s[i] == ' ') { out += "   "; i += 1; first = true; continue; }
        if (!first) out += " ";
        out += s.substr(i, len);
        first = false;
        i += len;
    }
    return out;
}
}  // namespace

// The "stage" of a scene: a coloured band with the place name and scene title.
void backdrop(const std::string& place, const std::string& subtitle, int bgColor, bool ending) {
    std::string up = place;
    for (char& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    std::string title = displayWidth(up) * 2 + 4 <= static_cast<std::size_t>(kWidth) ? spaced(up) : up;
    std::string sub = clip(subtitle, kWidth - 8);

    if (!g_color) {
        rule(up);
        std::cout << center(sub, kWidth) << "\n\n";
        return;
    }
    const int fgMain = ending ? 16 : 231, fgSub = ending ? 16 : 252;
    auto row = [&](const std::string& text, bool bold, int fg) {
        int w = static_cast<int>(displayWidth(text));
        int left = (kWidth - w) / 2;
        std::cout << "\033[48;5;" << bgColor << "m" << std::string(left, ' ') << "\033["
                  << (bold ? "1;" : "") << "38;5;" << fg << "m" << text
                  << std::string(kWidth - left - w, ' ') << "\033[0m\n";
    };
    row("", false, fgMain);
    row(title, true, fgMain);
    row(sub, false, fgSub);
    row("", false, fgMain);
    std::cout << "\n";
}

void rule(const std::string& title, Color c) {
    const Glyphs& G = g();
    if (title.empty()) {
        std::cout << paint(c, repeat(G.h, kWidth)) << "\n";
        return;
    }
    std::string t = " " + clip(title, kWidth - 10) + " ";
    int fill = kWidth - 2 - static_cast<int>(displayWidth(t));
    std::cout << paint(c, repeat(G.h, 2)) << paint(c, t, true) << paint(c, repeat(G.h, fill)) << "\n";
}

void box(const std::string& title, const std::vector<std::string>& lines, Color border,
         const std::string& rightTitle, int typeMs) {
    const Glyphs& G = g();
    const int inner = kWidth - 2;

    const std::string t = clip(title, inner - 12);
    const std::string r = clip(rightTitle, 30);
    const std::string left = t.empty() ? "" : " " + t + " ";
    const std::string right = r.empty() ? "" : " " + r + " ";
    int fill = inner - 2 - static_cast<int>(displayWidth(left)) - static_cast<int>(displayWidth(right));
    if (fill < 1) fill = 1;

    std::cout << paint(border, std::string(G.tl) + G.h) << paint(border, left, true)
              << paint(border, repeat(G.h, fill)) << paint(Color::Dim, right)
              << paint(border, std::string(G.h) + G.tr) << "\n";

    const std::string vb = paint(border, G.v);
    const bool typing = typeMs > 0 && g_interactive;
    for (const std::string& line : lines) {
        std::cout << vb << " ";
        if (typing) typeOut(line, typeMs);
        else std::cout << line;
        int pad = inner - 2 - static_cast<int>(displayWidth(line));
        if (pad > 0) std::cout << std::string(static_cast<std::size_t>(pad), ' ');
        std::cout << " " << vb << "\n";
    }
    std::cout << paint(border, std::string(G.bl) + repeat(G.h, inner) + G.br) << "\n";
}

void menu(const std::string& title, const std::vector<MenuItem>& items, int columns, Color border) {
    const int n = static_cast<int>(items.size());
    const int cols = std::max(1, columns);
    const int rows = (n + cols - 1) / cols;
    std::size_t keyW = 0;
    for (const auto& it : items) keyW = std::max(keyW, displayWidth(it.key));
    const int colW = (kWidth - 4) / cols;

    std::vector<std::string> lines;
    for (int r = 0; r < rows; ++r) {
        std::string line;
        for (int c = 0; c < cols; ++c) {
            int idx = c * rows + r;
            if (idx >= n) { line += std::string(static_cast<std::size_t>(colW), ' '); continue; }
            std::string cell = paint(Color::Gold, padLeft(items[idx].key, keyW), true) + "  " + items[idx].label;
            line += padRight(cell, static_cast<std::size_t>(colW));
        }
        lines.push_back(line);
    }
    box(title, lines, border);
}

void table(const std::string& title, const std::vector<std::string>& headers,
           const std::vector<std::vector<std::string>>& rows, Color border) {
    const Glyphs& G = g();
    const std::size_t nc = headers.size();
    std::vector<std::size_t> w(nc);
    for (std::size_t c = 0; c < nc; ++c) {
        w[c] = displayWidth(headers[c]);
        for (const auto& r : rows) w[c] = std::max(w[c], displayWidth(r[c]));
    }
    auto fmt = [&](const std::vector<std::string>& r) {
        std::string s;
        for (std::size_t c = 0; c < nc; ++c) s += (c ? "  " : "") + padRight(r[c], w[c]);
        return s;
    };
    std::vector<std::string> lines;
    if (rows.empty()) {
        lines.push_back(paint(Color::Dim, "(nothing here yet)"));
    } else {
        std::size_t total = 2 * (nc - 1);
        for (std::size_t x : w) total += x;
        lines.push_back(paint(Color::Dim, fmt(headers), true));
        lines.push_back(paint(Color::Dim, repeat(G.h, static_cast<int>(total))));
        for (const auto& r : rows) lines.push_back(fmt(r));
    }
    box(title, lines, border);
}

std::string bar(double fraction, int width, Color c) {
    const Glyphs& G = g();
    fraction = std::max(0.0, std::min(1.0, fraction));
    int filled = static_cast<int>(fraction * width + 0.5);
    return paint(c, repeat(G.full, filled)) + paint(Color::Dim, repeat(G.empty, width - filled));
}

void ok(const std::string& msg) { std::cout << "  " << paint(Color::Good, g().check, true) << " " << msg << "\n"; }
void err(const std::string& msg) { std::cout << "  " << paint(Color::Bad, g().cross, true) << " " << msg << "\n"; }
void warn(const std::string& msg) { std::cout << "  " << paint(Color::Warn, "!", true) << " " << msg << "\n"; }
void note(const std::string& msg) { std::cout << "  " << paint(Color::Primary, g().dot) << " " << msg << "\n"; }
void hint(const std::string& msg) { std::cout << "  " << paint(Color::Dim, std::string(g().pointer) + " " + msg) << "\n"; }

std::string styledPrompt(const std::string& raw) {
    std::size_t i = 0;
    while (i < raw.size() && raw[i] == '\n') ++i;
    return raw.substr(0, i) + "  " + paint(Color::Gold, g().prompt, true) + " " + raw.substr(i);
}

}  // namespace ui
}  // namespace storylang
