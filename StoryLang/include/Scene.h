#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace storylang {

// A Choice is a directed edge: Scene --(text)--> targetSceneId.
// Id 0 means "none" for the optional item fields (ids start at 1).
struct Choice {
    std::string text;
    int targetSceneId = 0;
    int requiredItemId = 0;  // choice is locked until the player holds this item
    int grantsItemId = 0;    // taking this choice gives the player this item

    bool isGated() const { return requiredItemId != 0; }
};

// A Scene is a node in the story graph. `choices` is its adjacency list.
struct Scene {
    int id = 0;
    std::string title;
    std::string text;
    int locationId = 0;
    std::vector<int> characterIds;
    bool isEnding = false;
    std::vector<Choice> choices;

    bool hasCharacter(int characterId) const {
        return std::find(characterIds.begin(), characterIds.end(), characterId) !=
               characterIds.end();
    }
};

}  // namespace storylang
