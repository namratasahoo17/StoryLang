#include "StoryMap.h"

#include <set>
#include <sstream>

#include "InputHelper.h"

namespace storylang {

namespace {

void printChildren(const Story& s, int id, const std::string& prefix, std::set<int>& shown,
                   std::ostringstream& out) {
    const Scene* sc = s.findScene(id);
    if (!sc) return;
    const std::size_t n = sc->choices.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Choice& ch = sc->choices[i];
        const bool last = (i + 1 == n);
        out << prefix << (last ? "`-- " : "|-- ") << "\"" << input::clip(ch.text, 34) << "\"";
        if (ch.requiredItemId) out << " {needs " << s.entityName(EntityKind::Item, ch.requiredItemId) << "}";
        if (ch.grantsItemId) out << " {gives " << s.entityName(EntityKind::Item, ch.grantsItemId) << "}";
        out << " -> ";

        const Scene* t = s.findScene(ch.targetSceneId);
        if (!t) {
            out << "[" << ch.targetSceneId << "] !! MISSING SCENE !!\n";
            continue;
        }
        out << "[" << t->id << "] " << t->title;
        if (t->isEnding) out << "  (END)";
        if (!t->isEnding && t->choices.empty()) out << "  (DEAD END)";
        if (shown.count(t->id)) {
            out << "  (shown above)\n";
            continue;
        }
        out << "\n";
        shown.insert(t->id);
        if (!t->isEnding) printChildren(s, t->id, prefix + (last ? "    " : "|   "), shown, out);
    }
}

std::string esc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"' || c == '\\') { r += '\\'; r += c; }
        else if (c == '\n') r += "\\n";
        else r += c;
    }
    return r;
}

}  // namespace

std::string asciiMap(const Story& story) {
    std::ostringstream out;
    if (!story.hasScene(story.startSceneId())) {
        out << "(no valid start scene - nothing to map)\n";
        return out.str();
    }
    std::set<int> shown;
    const Scene* start = story.findScene(story.startSceneId());
    out << "[" << start->id << "] " << start->title << "  (START)\n";
    shown.insert(start->id);
    if (!start->isEnding) printChildren(story, start->id, "", shown, out);

    bool header = false;
    for (const auto& kv : story.scenes()) {
        if (shown.count(kv.first)) continue;
        if (!header) {
            out << "\nNot reachable from the start:\n";
            header = true;
        }
        out << "  [" << kv.first << "] " << kv.second.title << (kv.second.isEnding ? "  (END)" : "") << "\n";
    }
    return out.str();
}

std::string toDot(const Story& story) {
    std::ostringstream out;
    out << "digraph \"" << esc(story.name()) << "\" {\n"
        << "  rankdir=TB;\n"
        << "  node [fontname=\"Helvetica\", fontsize=10];\n"
        << "  edge [fontname=\"Helvetica\", fontsize=9];\n";

    std::set<int> missing;
    for (const auto& kv : story.scenes()) {
        const Scene& s = kv.second;
        out << "  s" << s.id << " [label=\"" << s.id << ": " << esc(s.title) << "\", shape="
            << (s.isEnding ? "doublecircle" : "box");
        if (s.id == story.startSceneId())
            out << ", style=filled, fillcolor=\"#b7e4c7\"";
        else if (s.isEnding)
            out << ", style=filled, fillcolor=\"#ffd6a5\"";
        else if (s.choices.empty())
            out << ", style=filled, fillcolor=\"#ffadad\"";
        out << "];\n";
    }
    for (const auto& kv : story.scenes()) {
        const Scene& s = kv.second;
        for (const Choice& c : s.choices) {
            std::string label = esc(input::clip(c.text, 28));
            if (c.requiredItemId) label += "\\n[needs " + esc(story.entityName(EntityKind::Item, c.requiredItemId)) + "]";
            if (c.grantsItemId) label += "\\n[+" + esc(story.entityName(EntityKind::Item, c.grantsItemId)) + "]";
            if (!story.hasScene(c.targetSceneId)) missing.insert(c.targetSceneId);
            out << "  s" << s.id << " -> " << (story.hasScene(c.targetSceneId) ? "s" : "missing")
                << c.targetSceneId << " [label=\"" << label << "\"";
            if (c.requiredItemId) out << ", style=dashed";
            if (!story.hasScene(c.targetSceneId)) out << ", color=red";
            out << "];\n";
        }
    }
    for (int id : missing)
        out << "  missing" << id << " [label=\"MISSING " << id
            << "\", shape=box, style=\"dashed,filled\", fillcolor=\"#ffadad\"];\n";
    out << "}\n";
    return out.str();
}

}  // namespace storylang
