#include "PathFinder.h"

#include <algorithm>
#include <queue>

namespace storylang {

bool PathFinder::choiceAvailable(const Choice& c, const std::set<int>& inventory) {
    return c.requiredItemId == 0 || inventory.count(c.requiredItemId) != 0;
}

// ------------------------------------------------------ plain graph BFS
std::set<int> PathFinder::reachableScenes(const Story& story) {
    std::set<int> seen;
    if (!story.hasScene(story.startSceneId())) return seen;
    std::queue<int> q;
    q.push(story.startSceneId());
    seen.insert(story.startSceneId());
    while (!q.empty()) {
        int id = q.front();
        q.pop();
        const Scene* s = story.findScene(id);
        if (!s || s->isEnding) continue;  // endings are terminal
        for (const Choice& c : s->choices) {
            if (story.hasScene(c.targetSceneId) && seen.insert(c.targetSceneId).second)
                q.push(c.targetSceneId);
        }
    }
    return seen;
}

// ------------------------------------------- (scene, inventory) state BFS
namespace {

struct Node {
    int scene;
    std::set<int> inv;
    int parent;
    int viaChoice;
};
using Key = std::pair<int, std::set<int>>;

struct BfsResult {
    Exploration ex;
    std::vector<Node> nodes;
    int foundIndex = -1;
};

BfsResult bfs(const Story& story, std::size_t maxStates, int target) {
    BfsResult r;
    const Scene* start = story.findScene(story.startSceneId());
    if (!start) return r;

    std::map<Key, std::size_t> seen;
    r.nodes.push_back({start->id, {}, -1, -1});
    seen[{start->id, {}}] = 0;

    for (std::size_t i = 0; i < r.nodes.size(); ++i) {
        Node cur = r.nodes[i];  // copy: the vector may reallocate below
        r.ex.scenes.insert(cur.scene);
        if (target >= 0 && cur.scene == target) {
            r.foundIndex = static_cast<int>(i);
            break;
        }
        const Scene* sc = story.findScene(cur.scene);
        if (!sc || sc->isEnding) continue;

        bool anyAvailable = false, blockedByItem = false;
        for (std::size_t c = 0; c < sc->choices.size(); ++c) {
            const Choice& ch = sc->choices[c];
            if (!story.hasScene(ch.targetSceneId)) continue;
            if (!PathFinder::choiceAvailable(ch, cur.inv)) {
                blockedByItem = true;
                continue;
            }
            anyAvailable = true;
            r.ex.choicesTaken.insert({sc->id, c});

            std::set<int> inv = cur.inv;
            if (ch.grantsItemId != 0) inv.insert(ch.grantsItemId);
            Key k{ch.targetSceneId, inv};
            if (seen.count(k)) continue;
            if (r.nodes.size() >= maxStates) {
                r.ex.truncated = true;
                continue;
            }
            seen[k] = r.nodes.size();
            r.nodes.push_back({ch.targetSceneId, inv, static_cast<int>(i), static_cast<int>(c)});
        }
        if (!anyAvailable && blockedByItem) r.ex.softLocks.insert(sc->id);
    }
    r.ex.states = r.nodes.size();
    return r;
}

}  // namespace

Exploration PathFinder::explore(const Story& story, std::size_t maxStates) {
    return bfs(story, maxStates, -1).ex;
}

std::optional<std::vector<PathStep>> PathFinder::shortestPath(const Story& story, int targetScene,
                                                              std::size_t maxStates) {
    BfsResult r = bfs(story, maxStates, targetScene);
    if (r.foundIndex < 0) return std::nullopt;

    std::vector<PathStep> steps;
    int childChoice = -1;
    for (int idx = r.foundIndex; idx != -1; idx = r.nodes[static_cast<std::size_t>(idx)].parent) {
        const Node& n = r.nodes[static_cast<std::size_t>(idx)];
        steps.push_back({n.scene, childChoice});
        childChoice = n.viaChoice;  // the choice that led INTO n is taken from its parent
    }
    std::reverse(steps.begin(), steps.end());
    return steps;
}

// ------------------------------------------------ DFS path enumeration
namespace {

struct CountCtx {
    const Story& story;
    PathCount& out;
    int maxDepth;
    long long maxSteps;
    long long steps = 0;
    std::set<Key> onPath;
    std::set<int> inv;
};

void dfs(CountCtx& c, int sceneId, int depth) {
    const Scene* sc = c.story.findScene(sceneId);
    if (!sc) return;
    if (sc->isEnding) {
        c.out.perEnding[sceneId]++;
        c.out.total++;
        return;
    }
    if (depth >= c.maxDepth) {
        c.out.truncated = true;
        return;
    }
    for (const Choice& ch : sc->choices) {
        if (++c.steps > c.maxSteps) {
            c.out.truncated = true;
            return;
        }
        if (!c.story.hasScene(ch.targetSceneId)) continue;
        if (!PathFinder::choiceAvailable(ch, c.inv)) continue;

        bool added = false;
        if (ch.grantsItemId != 0 && !c.inv.count(ch.grantsItemId)) {
            c.inv.insert(ch.grantsItemId);
            added = true;
        }
        Key k{ch.targetSceneId, c.inv};
        if (!c.onPath.count(k)) {
            c.onPath.insert(k);
            dfs(c, ch.targetSceneId, depth + 1);
            c.onPath.erase(k);
        }
        if (added) c.inv.erase(ch.grantsItemId);
    }
}

}  // namespace

PathCount PathFinder::countPaths(const Story& story, int maxDepth, long long maxSteps) {
    PathCount result;
    if (!story.hasScene(story.startSceneId())) return result;
    CountCtx ctx{story, result, maxDepth, maxSteps, 0, {}, {}};
    ctx.onPath.insert({story.startSceneId(), {}});
    dfs(ctx, story.startSceneId(), 0);
    return result;
}

}  // namespace storylang
