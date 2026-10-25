#pragma once
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "Story.h"

namespace storylang {

struct PathStep {
    int sceneId;
    int choiceIndex;  // choice taken FROM this scene; -1 on the final step
};

// Result of exploring every (scene, inventory) state reachable from the start.
struct Exploration {
    std::set<int> scenes;                                // reachable with items considered
    std::set<std::pair<int, std::size_t>> choicesTaken;  // (scene, choice index) that can fire
    std::set<int> softLocks;                             // item gates can block ALL choices here
    bool truncated = false;                              // state cap hit
    std::size_t states = 0;
};

struct PathCount {
    std::map<int, long long> perEnding;
    long long total = 0;
    bool truncated = false;
};

class PathFinder {
public:
    static bool choiceAvailable(const Choice& c, const std::set<int>& inventory);

    // Plain graph BFS from the start scene, ignoring item gates. Endings are terminal.
    static std::set<int> reachableScenes(const Story& story);

    // BFS over (scene, inventory) states.
    static Exploration explore(const Story& story, std::size_t maxStates = 200000);

    // Shortest sequence of choices from the start to `targetScene`, honouring items.
    static std::optional<std::vector<PathStep>> shortestPath(const Story& story, int targetScene,
                                                             std::size_t maxStates = 200000);

    // DFS with backtracking: counts distinct routes to every ending. A route may not
    // repeat a (scene, inventory) state, so loops cannot make the count infinite.
    static PathCount countPaths(const Story& story, int maxDepth = 40,
                                long long maxSteps = 2000000);
};

}  // namespace storylang
