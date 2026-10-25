#include "Validator.h"

#include <algorithm>
#include <iostream>
#include <map>

#include "InputHelper.h"
#include "PathFinder.h"

namespace storylang {

namespace {
std::string q(const std::string& s) { return "\"" + input::clip(s, 40) + "\""; }
}  // namespace

std::vector<Issue> Validator::run() const {
    std::vector<Issue> out;
    checkStructure(out);
    checkReferences(out);
    checkReachability(out);
    checkDeadEnds(out);
    checkCycles(out);
    checkInventory(out);
    std::stable_sort(out.begin(), out.end(), [](const Issue& a, const Issue& b) {
        return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    });
    return out;
}

int Validator::count(const std::vector<Issue>& issues, Severity s) {
    return static_cast<int>(std::count_if(issues.begin(), issues.end(),
                                          [&](const Issue& i) { return i.severity == s; }));
}

// 1. start scene exists, at least one scene, at least one ending
void Validator::checkStructure(std::vector<Issue>& out) const {
    if (story_.scenes().empty()) {
        out.push_back({Severity::Error, "The story has no scenes.", 0});
        return;
    }
    if (story_.startSceneId() == 0)
        out.push_back({Severity::Error, "No start scene has been selected.", 0});
    else if (!story_.hasScene(story_.startSceneId()))
        out.push_back({Severity::Error,
                       "The start scene (id " + std::to_string(story_.startSceneId()) +
                           ") does not exist.",
                       0});
    if (story_.endingIds().empty())
        out.push_back({Severity::Error, "The story has no ending scene.", 0});
}

// 2. dangling choice targets, unknown location/character/item references
void Validator::checkReferences(std::vector<Issue>& out) const {
    for (const auto& kv : story_.scenes()) {
        const Scene& s = kv.second;
        const std::string where = story_.sceneLabel(s.id);

        if (s.locationId != 0 && !story_.findEntity(EntityKind::Location, s.locationId))
            out.push_back({Severity::Error,
                           where + " uses unknown location id " + std::to_string(s.locationId) + ".",
                           s.id});
        for (int cid : s.characterIds)
            if (!story_.findEntity(EntityKind::Character, cid))
                out.push_back({Severity::Error,
                               where + " lists unknown character id " + std::to_string(cid) + ".",
                               s.id});

        for (std::size_t i = 0; i < s.choices.size(); ++i) {
            const Choice& c = s.choices[i];
            const std::string cl = where + " choice #" + std::to_string(i + 1) + " " + q(c.text);
            if (!story_.hasScene(c.targetSceneId))
                out.push_back({Severity::Error,
                               cl + " points to scene " + std::to_string(c.targetSceneId) +
                                   ", which does not exist.",
                               s.id});
            if (c.requiredItemId != 0 && !story_.findEntity(EntityKind::Item, c.requiredItemId))
                out.push_back({Severity::Error, cl + " requires an unknown item.", s.id});
            if (c.grantsItemId != 0 && !story_.findEntity(EntityKind::Item, c.grantsItemId))
                out.push_back({Severity::Error, cl + " grants an unknown item.", s.id});
        }
    }
}

// 3. BFS from the start: anything not visited is unreachable
void Validator::checkReachability(std::vector<Issue>& out) const {
    if (!story_.hasScene(story_.startSceneId())) return;
    std::set<int> reach = PathFinder::reachableScenes(story_);
    for (const auto& kv : story_.scenes()) {
        if (reach.count(kv.first)) continue;
        const Scene& s = kv.second;
        if (s.isEnding)
            out.push_back({Severity::Error,
                           "Ending " + story_.sceneLabel(s.id) + " cannot be reached from the start.",
                           s.id});
        else
            out.push_back({Severity::Warning,
                           "Scene " + story_.sceneLabel(s.id) + " cannot be reached from the start.",
                           s.id});
    }
}

// 4. dead ends (non-ending with no way out) and endings that have choices
void Validator::checkDeadEnds(std::vector<Issue>& out) const {
    for (const auto& kv : story_.scenes()) {
        const Scene& s = kv.second;
        if (!s.isEnding && s.choices.empty())
            out.push_back({Severity::Error,
                           "Scene " + story_.sceneLabel(s.id) +
                               " is a dead end: it has no choices and is not marked as an ending.",
                           s.id});
        if (s.isEnding && !s.choices.empty())
            out.push_back({Severity::Warning,
                           "Ending " + story_.sceneLabel(s.id) +
                               " has outgoing choices, but play stops at endings so they are never shown.",
                           s.id});
    }
}

// 5. DFS with colours: a back edge to a "grey" scene is a loop (legal, but reported)
namespace {
struct CycleFinder {
    const Story& story;
    std::map<int, int> color;  // 0 white, 1 grey, 2 black
    std::vector<std::pair<int, int>> backEdges;

    void visit(int id) {
        color[id] = 1;
        const Scene* s = story.findScene(id);
        if (s && !s->isEnding) {
            for (const Choice& c : s->choices) {
                if (!story.hasScene(c.targetSceneId)) continue;
                int col = color[c.targetSceneId];
                if (col == 1) {
                    auto edge = std::make_pair(id, c.targetSceneId);
                    if (std::find(backEdges.begin(), backEdges.end(), edge) == backEdges.end())
                        backEdges.push_back(edge);
                }
                else if (col == 0)
                    visit(c.targetSceneId);
            }
        }
        color[id] = 2;
    }
};
}  // namespace

void Validator::checkCycles(std::vector<Issue>& out) const {
    if (!story_.hasScene(story_.startSceneId())) return;
    CycleFinder cf{story_, {}, {}};
    cf.visit(story_.startSceneId());
    for (const auto& e : cf.backEdges)
        out.push_back({Severity::Info,
                       "Loop: " + story_.sceneLabel(e.first) + " -> " + story_.sceneLabel(e.second) +
                           " lets the player revisit an earlier scene.",
                       e.first});
}

// 6. item gates: choices that can never unlock, scenes/endings hidden behind them, soft-locks
void Validator::checkInventory(std::vector<Issue>& out) const {
    bool anyGate = false;
    for (const auto& kv : story_.scenes())
        for (const Choice& c : kv.second.choices)
            if (c.isGated()) anyGate = true;
    if (!anyGate || !story_.hasScene(story_.startSceneId())) return;

    Exploration ex = PathFinder::explore(story_);
    std::set<int> structural = PathFinder::reachableScenes(story_);

    auto itemIsGrantedSomewhere = [&](int itemId) {
        for (const auto& kv : story_.scenes())
            for (const Choice& c : kv.second.choices)
                if (c.grantsItemId == itemId) return true;
        return false;
    };

    for (int id : structural) {
        const Scene* s = story_.findScene(id);
        if (!s || s->isEnding) continue;
        for (std::size_t i = 0; i < s->choices.size(); ++i) {
            const Choice& c = s->choices[i];
            if (!c.isGated() || !story_.hasScene(c.targetSceneId)) continue;
            if (ex.choicesTaken.count({id, i})) continue;
            std::string item = story_.entityName(EntityKind::Item, c.requiredItemId);
            std::string why = itemIsGrantedSomewhere(c.requiredItemId)
                                  ? "the item cannot be collected on any route that reaches this scene"
                                  : "no choice in the story ever grants it";
            out.push_back({Severity::Warning,
                           "Choice #" + std::to_string(i + 1) + " " + q(c.text) + " in " +
                               story_.sceneLabel(id) + " can never be unlocked: requires '" + item +
                               "' but " + why + ".",
                           id});
        }
    }

    for (int id : structural) {
        if (ex.scenes.count(id)) continue;
        const Scene* s = story_.findScene(id);
        if (!s) continue;
        if (s->isEnding)
            out.push_back({Severity::Error,
                           "Ending " + story_.sceneLabel(id) +
                               " is only reachable through item gates that can never be satisfied.",
                           id});
        else
            out.push_back({Severity::Warning,
                           "Scene " + story_.sceneLabel(id) +
                               " is only reachable through item gates that can never be satisfied.",
                           id});
    }

    for (int id : ex.softLocks)
        out.push_back({Severity::Warning,
                       "Possible soft-lock at " + story_.sceneLabel(id) +
                           ": a player can arrive here without the item any outgoing choice needs "
                           "(they can only use 'back').",
                       id});

    if (ex.truncated)
        out.push_back({Severity::Info,
                       "Item analysis stopped at the state limit; results may be incomplete.", 0});
}

void Validator::printReport(const Story& story, const std::vector<Issue>& issues) {
    std::cout << "\n===== VALIDATION REPORT: " << story.name() << " =====\n";
    std::cout << "Scenes: " << story.scenes().size() << "   Choices: " << story.choiceCount()
              << "   Endings: " << story.endingIds().size() << "\n\n";

    const struct { Severity s; const char* tag; } order[] = {
        {Severity::Error, "ERROR  "}, {Severity::Warning, "WARNING"}, {Severity::Info, "INFO   "}};
    for (const auto& o : order)
        for (const Issue& i : issues)
            if (i.severity == o.s) std::cout << " [" << o.tag << "] " << i.message << "\n";

    int e = count(issues, Severity::Error), w = count(issues, Severity::Warning),
        n = count(issues, Severity::Info);
    std::cout << "\nResult: " << (e == 0 ? "PASS" : "FAIL") << "  (" << e << " error(s), " << w
              << " warning(s), " << n << " info)\n";
    if (e == 0 && w == 0) std::cout << "No structural problems found.\n";
}

}  // namespace storylang
