#pragma once
// Ui.h - the "look" of StoryLang: colours, boxes, menus, tables, bars, banner.
// Everything that draws to the console goes through this module so the program
// has one consistent style and one place to change it.
#include <cstddef>
#include <string>
#include <vector>

namespace storylang {
namespace ui {

constexpr int kWidth = 78;  // every box / rule is exactly this many columns wide

enum class Color { Default, Primary, Accent, Pink, Gold, Good, Warn, Bad, Dim };

// Box-drawing characters: a Unicode set and a plain-ASCII fallback.
struct Glyphs {
    const char *tl, *tr, *bl, *br, *h, *v;                    // box corners / lines
    const char *dot, *prompt, *check, *cross, *star;          // markers
    const char *full, *empty;                                 // progress-bar cells
    const char *treeMid, *treeEnd, *treePipe;                 // story-map tree
    const char *pointer, *arrow, *sep, *lq, *rq;              // misc
};

// ---- setup -----------------------------------------------------------------
// Decides colour/Unicode/interactive mode from the terminal (isatty, NO_COLOR,
// TERM). forceColor = always colour; forcePlain = never colour, ASCII only.
void init(bool forceColor, bool forcePlain);
bool colorOn();
bool unicodeOn();
bool interactive();  // stdin AND stdout are a real terminal
const Glyphs& g();
void setClassic(bool on);  // use the legacy-Windows-console-safe glyph set

// ---- text helpers ----------------------------------------------------------
std::string paint(Color c, const std::string& s, bool bold = false);
std::string badge(const std::string& label, Color background);  // " ERROR " on a coloured block
std::size_t displayWidth(const std::string& s);                 // ignores ANSI, counts UTF-8 chars
std::string padRight(const std::string& s, std::size_t width);
std::string padLeft(const std::string& s, std::size_t width);
std::string center(const std::string& s, std::size_t width);
std::string repeat(const std::string& s, int n);
std::string clip(const std::string& s, std::size_t maxWidth);   // "long te..." (UTF-8 safe)
std::vector<std::string> wrapLines(const std::string& text, std::size_t width,
                                   const std::string& indent = "");

// ---- widgets ---------------------------------------------------------------
struct MenuItem {
    std::string key;
    std::string label;
};

void clearScreen();  // only when interactive + colour; never when output is piped
void banner();  // title-screen logo
// A coloured "stage" band (Ren'Py-style backdrop): place name + scene title.
// bgColor is an xterm-256 colour number; ending = gold band with dark text.
void backdrop(const std::string& place, const std::string& subtitle, int bgColor, bool ending = false);
void rule(const std::string& title = "", Color c = Color::Dim);
// typeMs > 0 types the text out character by character (only on a real terminal).
void box(const std::string& title, const std::vector<std::string>& lines,
         Color border = Color::Primary, const std::string& rightTitle = "", int typeMs = 0);
void menu(const std::string& title, const std::vector<MenuItem>& items, int columns = 1,
          Color border = Color::Primary);
void table(const std::string& title, const std::vector<std::string>& headers,
           const std::vector<std::vector<std::string>>& rows, Color border = Color::Primary);
std::string bar(double fraction, int width, Color c = Color::Gold);

// one-line status messages
void ok(const std::string& msg);
void err(const std::string& msg);
void warn(const std::string& msg);
void note(const std::string& msg);
void hint(const std::string& msg);

std::string styledPrompt(const std::string& raw);  // used by InputHelper

}  // namespace ui
}  // namespace storylang
