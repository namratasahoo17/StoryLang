#include "Player.h"

#include <iostream>

#include "InputHelper.h"
#include "PathFinder.h"

using std::cout;

namespace storylang {

namespace in = input;

std::string Player::lockReason(const Choice& c) const {
    return "requires " + story_.entityName(EntityKind::Item, c.requiredItemId);
}

void Player::showScene(const Scene& scene) const {
    cout << "\n" << std::string(62, '=') << "\n";
    cout << "  " << scene.title << "\n";

    std::string meta;
    if (scene.locationId != 0)
        meta += "@ " + story_.entityName(EntityKind::Location, scene.locationId);
    if (!scene.characterIds.empty()) {
        if (!meta.empty()) meta += "  |  ";
        meta += "Present: ";
        for (std::size_t i = 0; i < scene.characterIds.size(); ++i) {
            if (i) meta += ", ";
            meta += story_.entityName(EntityKind::Character, scene.characterIds[i]);
        }
    }
    if (!meta.empty()) cout << "  " << meta << "\n";
    cout << std::string(62, '-') << "\n";
    cout << in::wrapText(scene.text, 60, "  ") << "\n";
}

void Player::showInventory() const {
    if (state_.inventory().empty()) {
        cout << "  Your inventory is empty.\n";
        return;
    }
    cout << "  Inventory:\n";
    for (int id : state_.inventory()) {
        const Entity* e = story_.findEntity(EntityKind::Item, id);
        cout << "   - " << (e ? e->name : "?");
        if (e && !e->description.empty()) cout << ": " << e->description;
        cout << "\n";
    }
}

void Player::showEndSummary() const {
    cout << "\n" << std::string(62, '*') << "\n";
    cout << "  THE END\n";
    cout << std::string(62, '*') << "\n";
    cout << "  Steps taken:     " << state_.steps() << "\n";
    cout << "  Scenes visited:  " << state_.visited().size() << " of " << story_.scenes().size()
         << "\n";
    cout << "  Items collected: " << state_.inventory().size() << "\n";
}

void Player::run() {
    if (!story_.hasScene(story_.startSceneId())) {
        cout << "\n  Cannot play: the story has no valid start scene. Run the validator.\n";
        return;
    }
    state_.reset(story_.startSceneId());
    cout << "\n######## " << story_.name() << " ########\n";
    cout << "Commands: choice number | b = go back | i = inventory | q = quit\n";

    for (;;) {
        const Scene* scene = story_.findScene(state_.current());
        if (!scene) return;  // cannot happen: moves to missing scenes are refused below

        showScene(*scene);
        if (scene->isEnding) {
            showEndSummary();
            return;
        }

        const int n = static_cast<int>(scene->choices.size());
        if (n == 0) {
            cout << "\n  [Dead end: this scene offers no choices. Use b to go back or q to quit.]\n";
        } else {
            cout << "\n";
            for (int i = 0; i < n; ++i) {
                const Choice& c = scene->choices[static_cast<std::size_t>(i)];
                cout << "  " << (i + 1) << ") " << c.text;
                if (!state_.has(c.requiredItemId)) cout << "   [LOCKED - " << lockReason(c) << "]";
                cout << "\n";
            }
        }

        for (;;) {  // input loop for this scene
            std::string cmd = in::toLower(in::trim(in::readLine("\n> ")));
            if (cmd == "q") {
                if (in::confirm("Quit this playthrough?")) {
                    cout << "  You walked away from the story.\n";
                    return;
                }
                continue;
            }
            if (cmd == "b") {
                if (state_.undo()) break;
                cout << "  Nothing to go back to.\n";
                continue;
            }
            if (cmd == "i") {
                showInventory();
                continue;
            }
            long v = 0;
            if (!in::parseInt(cmd, v) || v < 1 || v > n) {
                cout << "  Enter a choice number"
                     << (n > 0 ? " (1-" + std::to_string(n) + ")" : std::string())
                     << ", or b / i / q.\n";
                continue;
            }
            const Choice& c = scene->choices[static_cast<std::size_t>(v - 1)];
            if (!state_.has(c.requiredItemId)) {
                cout << "  That option is locked - it " << lockReason(c) << ".\n";
                continue;
            }
            if (!story_.hasScene(c.targetSceneId)) {
                cout << "  That path leads nowhere (broken link to scene " << c.targetSceneId
                     << "). Tell the author - the validator will find it.\n";
                continue;
            }
            bool newItem = c.grantsItemId != 0 && !state_.has(c.grantsItemId);
            state_.apply(c);
            if (newItem)
                cout << "\n  ** You obtained: " << story_.entityName(EntityKind::Item, c.grantsItemId)
                     << " **\n";
            break;
        }
    }
}

}  // namespace storylang
