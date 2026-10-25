#pragma once
#include <string>

#include "GameState.h"
#include "Story.h"

namespace storylang {

// Runs an interactive play-through of a Story on the console.
class Player {
public:
    explicit Player(const Story& story) : story_(story) {}
    void run();

private:
    void showScene(const Scene& scene) const;
    void showInventory() const;
    void showEndSummary() const;
    std::string lockReason(const Choice& c) const;

    const Story& story_;
    GameState state_;
};

}  // namespace storylang
