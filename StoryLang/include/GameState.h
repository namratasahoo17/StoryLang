#pragma once
#include <set>
#include <vector>

#include "Scene.h"

namespace storylang {

struct Snapshot {
    int sceneId;
    std::set<int> inventory;
};

// Everything that changes while a story is being played.
class GameState {
public:
    void reset(int startScene) {
        current_ = startScene;
        inventory_.clear();
        visited_.clear();
        history_.clear();
        visited_.insert(startScene);
    }

    int current() const { return current_; }
    const std::set<int>& inventory() const { return inventory_; }
    const std::set<int>& visited() const { return visited_; }
    bool has(int itemId) const { return itemId == 0 || inventory_.count(itemId) != 0; }
    std::size_t steps() const { return history_.size(); }

    // Take a choice: remember where we were (for undo), collect any item, move on.
    void apply(const Choice& c) {
        history_.push_back({current_, inventory_});
        if (c.grantsItemId != 0) inventory_.insert(c.grantsItemId);
        current_ = c.targetSceneId;
        visited_.insert(current_);
    }

    bool undo() {
        if (history_.empty()) return false;
        current_ = history_.back().sceneId;
        inventory_ = history_.back().inventory;
        history_.pop_back();
        return true;
    }

private:
    int current_ = 0;
    std::set<int> inventory_;
    std::set<int> visited_;
    std::vector<Snapshot> history_;
};

}  // namespace storylang
