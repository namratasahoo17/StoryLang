#include "Editor.h"

#include <algorithm>
#include <climits>
#include <iostream>

#include "InputHelper.h"

using std::cout;

namespace storylang {

namespace in = input;

namespace {
const int kMaxId = 1000000;
const EntityKind kAllKinds[] = {EntityKind::Character, EntityKind::Location, EntityKind::Item};
}  // namespace

// ================================================================ top menu
void Editor::run() {
    for (;;) {
        cout << "\n--- EDIT STORY: " << story_.name() << " ---\n"
             << " 1. Scenes (list / add / edit / delete)\n"
             << " 2. Choices (links between scenes)\n"
             << " 3. Characters\n"
             << " 4. Locations\n"
             << " 5. Items\n"
             << " 6. Set start scene\n"
             << " 7. Rename story\n"
             << " 0. Back to main menu\n";
        switch (in::readInt("Select: ", 0, 7)) {
            case 1: sceneMenu(); break;
            case 2: choiceMenu(); break;
            case 3: entityMenu(EntityKind::Character); break;
            case 4: entityMenu(EntityKind::Location); break;
            case 5: entityMenu(EntityKind::Item); break;
            case 6: setStart(); break;
            case 7: story_.setName(in::readNonEmpty("New story name: ")); break;
            default: return;
        }
    }
}

// ================================================================= scenes
void Editor::listScenes() const {
    if (story_.scenes().empty()) {
        cout << "  (no scenes yet)\n";
        return;
    }
    for (const auto& kv : story_.scenes()) {
        const Scene& s = kv.second;
        cout << "  [" << s.id << "] " << s.title;
        if (s.id == story_.startSceneId()) cout << "  <START>";
        if (s.isEnding) cout << "  <ENDING>";
        cout << "  - " << s.choices.size() << " choice(s)";
        if (s.locationId) cout << " @ " << story_.entityName(EntityKind::Location, s.locationId);
        cout << "\n";
    }
}

int Editor::pickScene(const std::string& prompt) const {
    if (story_.scenes().empty()) {
        cout << "  No scenes yet - add one first.\n";
        return 0;
    }
    for (;;) {
        auto v = in::readIntOrBlank(prompt + " (blank to cancel): ", 1, kMaxId);
        if (!v) return 0;
        if (story_.hasScene(*v)) return *v;
        cout << "  There is no scene with id " << *v << ".\n";
    }
}

std::string Editor::readBody() const {
    for (;;) {
        std::string t = in::readMultiline("Scene text");
        if (!in::trim(t).empty()) return t;
        cout << "  Scene text cannot be empty.\n";
    }
}

void Editor::viewScene(int id) const {
    const Scene* s = story_.findScene(id);
    if (!s) return;
    cout << "\n  [" << s->id << "] " << s->title << (s->isEnding ? "  <ENDING>" : "") << "\n";
    cout << "  Location:   " << story_.entityName(EntityKind::Location, s->locationId) << "\n";
    cout << "  Characters: ";
    if (s->characterIds.empty()) cout << "-";
    for (std::size_t i = 0; i < s->characterIds.size(); ++i)
        cout << (i ? ", " : "") << story_.entityName(EntityKind::Character, s->characterIds[i]);
    cout << "\n" << in::wrapText(s->text, 70, "  | ") << "\n";
    printChoices(*s);
}

void Editor::printChoices(const Scene& scene) const {
    if (scene.choices.empty()) {
        cout << "  (no choices)\n";
        return;
    }
    for (std::size_t i = 0; i < scene.choices.size(); ++i) {
        const Choice& c = scene.choices[i];
        cout << "  " << (i + 1) << ") \"" << c.text << "\" -> " << story_.sceneLabel(c.targetSceneId);
        if (c.requiredItemId) cout << "  {needs " << story_.entityName(EntityKind::Item, c.requiredItemId) << "}";
        if (c.grantsItemId) cout << "  {gives " << story_.entityName(EntityKind::Item, c.grantsItemId) << "}";
        cout << "\n";
    }
}

void Editor::sceneMenu() {
    for (;;) {
        cout << "\n--- SCENES ---\n 1. List scenes\n 2. View a scene\n 3. Add scene\n"
             << " 4. Edit scene\n 5. Delete scene\n 0. Back\n";
        switch (in::readInt("Select: ", 0, 5)) {
            case 1: listScenes(); break;
            case 2: {
                listScenes();
                if (int id = pickScene("Scene id")) viewScene(id);
                break;
            }
            case 3: addScene(); break;
            case 4: {
                listScenes();
                if (int id = pickScene("Scene id to edit")) editScene(id);
                break;
            }
            case 5: deleteScene(); break;
            default: return;
        }
    }
}

void Editor::addScene() {
    std::string title = in::readNonEmpty("Scene title: ");
    std::string text = readBody();
    bool ending = in::confirm("Is this an ENDING scene?");
    int id = story_.addScene(title, text, ending);
    cout << "  Created scene [" << id << "].\n";
    if (story_.startSceneId() == 0) {
        story_.setStartSceneId(id);
        cout << "  (It is now the start scene.)\n";
    }
    if (!story_.entities(EntityKind::Location).empty() && in::confirm("Assign a location now?")) {
        if (auto v = askEntity(EntityKind::Location, "Location id (blank = skip, 0 = none): "))
            story_.findScene(id)->locationId = *v;
    }
    if (!ending && in::confirm("Add choices to this scene now?")) choicesLoop(id);
}

void Editor::editScene(int id) {
    for (;;) {
        Scene* s = story_.findScene(id);
        if (!s) return;
        cout << "\n--- EDIT SCENE [" << id << "] " << s->title << " ---\n"
             << " 1. Title\n 2. Text\n 3. Location\n 4. Add character\n 5. Remove character\n"
             << " 6. Toggle ending (currently " << (s->isEnding ? "ENDING" : "normal") << ")\n"
             << " 7. Edit this scene's choices\n 0. Back\n";
        switch (in::readInt("Select: ", 0, 7)) {
            case 1: s->title = in::readNonEmpty("New title: "); break;
            case 2: s->text = readBody(); break;
            case 3:
                if (story_.entities(EntityKind::Location).empty())
                    cout << "  No locations defined yet (create some from the edit menu).\n";
                else if (auto v = askEntity(EntityKind::Location, "Location id (blank = keep, 0 = none): "))
                    s->locationId = *v;
                break;
            case 4:
                if (story_.entities(EntityKind::Character).empty())
                    cout << "  No characters defined yet (create some from the edit menu).\n";
                else if (auto v = askEntity(EntityKind::Character, "Character id to add (blank = cancel): ")) {
                    if (*v == 0) break;
                    if (s->hasCharacter(*v)) cout << "  Already in this scene.\n";
                    else s->characterIds.push_back(*v);
                }
                break;
            case 5: {
                if (s->characterIds.empty()) { cout << "  No characters in this scene.\n"; break; }
                for (int cid : s->characterIds)
                    cout << "  [" << cid << "] " << story_.entityName(EntityKind::Character, cid) << "\n";
                auto v = in::readIntOrBlank("Character id to remove (blank = cancel): ", 1, kMaxId);
                if (v) {
                    auto& ids = s->characterIds;
                    auto it = std::find(ids.begin(), ids.end(), *v);
                    if (it == ids.end()) cout << "  That character is not in this scene.\n";
                    else ids.erase(it);
                }
                break;
            }
            case 6:
                s->isEnding = !s->isEnding;
                if (s->isEnding && !s->choices.empty())
                    cout << "  Note: endings stop play, so this scene's choices will never be shown.\n";
                break;
            case 7: choicesLoop(id); break;
            default: return;
        }
    }
}

void Editor::deleteScene() {
    listScenes();
    int id = pickScene("Scene id to delete");
    if (!id) return;
    auto incoming = story_.incomingChoices(id);
    cout << "  " << story_.sceneLabel(id) << " has " << incoming.size() << " incoming choice(s)";
    if (id == story_.startSceneId()) cout << " and is the START scene";
    cout << ".\n"
         << " 1. Delete scene AND remove the choices that lead to it (safe)\n"
         << " 2. Delete scene only, leave dangling links (the validator will flag them)\n"
         << " 3. Cancel\n";
    int c = in::readInt("Select: ", 1, 3);
    if (c == 3) return;
    int removed = story_.removeScene(id, c == 1);
    cout << "  Scene deleted." << (c == 1 ? " Removed " + std::to_string(removed) + " incoming choice(s)." : "")
         << "\n";
    if (story_.startSceneId() == 0 && !story_.scenes().empty())
        cout << "  The start scene was cleared - pick a new one from the edit menu.\n";
}

// ================================================================ choices
void Editor::choiceMenu() {
    listScenes();
    if (int id = pickScene("Scene whose choices to edit")) choicesLoop(id);
}

void Editor::choicesLoop(int sceneId) {
    for (;;) {
        const Scene* s = story_.findScene(sceneId);
        if (!s) return;
        cout << "\n--- CHOICES OF " << story_.sceneLabel(sceneId) << " ---\n";
        printChoices(*s);
        cout << " 1. Add choice\n 2. Edit choice\n 3. Delete choice\n 0. Done\n";
        switch (in::readInt("Select: ", 0, 3)) {
            case 1: addChoice(sceneId); break;
            case 2: editChoice(sceneId); break;
            case 3: deleteChoice(sceneId); break;
            default: return;
        }
    }
}

void Editor::addChoice(int sceneId) {
    Choice c;
    c.text = in::readNonEmpty("Choice text (what the player sees): ");
    listScenes();
    c.targetSceneId = in::readInt("Target scene id: ", 1, kMaxId);
    if (!story_.hasScene(c.targetSceneId)) {
        cout << "  Warning: scene " << c.targetSceneId << " does not exist (yet). The validator will\n"
             << "  report this link as an error until you create that scene.\n";
        if (!in::confirm("Keep the link anyway?")) return;
    }
    if (!story_.entities(EntityKind::Item).empty()) {
        if (auto v = askEntity(EntityKind::Item, "Item REQUIRED to take this choice (blank = none): "))
            c.requiredItemId = *v;
        if (auto v = askEntity(EntityKind::Item, "Item GRANTED by this choice (blank = none): "))
            c.grantsItemId = *v;
    }
    story_.addChoice(sceneId, c);
    cout << "  Choice added.\n";
}

void Editor::editChoice(int sceneId) {
    Scene* s = story_.findScene(sceneId);
    if (!s || s->choices.empty()) { cout << "  Nothing to edit.\n"; return; }
    int n = static_cast<int>(s->choices.size());
    int idx = in::readInt("Choice number to edit (1-" + std::to_string(n) + "): ", 1, n) - 1;
    Choice& c = s->choices[static_cast<std::size_t>(idx)];

    std::string t = in::trim(in::readLine("New text (blank = keep): "));
    if (!t.empty()) c.text = t;
    if (auto v = in::readIntOrBlank("New target scene id (blank = keep): ", 1, kMaxId)) {
        c.targetSceneId = *v;
        if (!story_.hasScene(*v)) cout << "  Warning: scene " << *v << " does not exist yet.\n";
    }
    if (!story_.entities(EntityKind::Item).empty()) {
        if (auto v = askEntity(EntityKind::Item, "Required item (blank = keep, 0 = none): "))
            c.requiredItemId = *v;
        if (auto v = askEntity(EntityKind::Item, "Granted item (blank = keep, 0 = none): "))
            c.grantsItemId = *v;
    }
    cout << "  Choice updated.\n";
}

void Editor::deleteChoice(int sceneId) {
    Scene* s = story_.findScene(sceneId);
    if (!s || s->choices.empty()) { cout << "  Nothing to delete.\n"; return; }
    int n = static_cast<int>(s->choices.size());
    int idx = in::readInt("Choice number to delete (1-" + std::to_string(n) + "): ", 1, n) - 1;
    if (in::confirm("Delete this choice?")) {
        story_.removeChoice(sceneId, static_cast<std::size_t>(idx));
        cout << "  Deleted.\n";
    }
}

// ================================================================ entities
void Editor::listEntities(EntityKind kind) const {
    const auto& m = story_.entities(kind);
    if (m.empty()) { cout << "  (no " << kindName(kind) << "s yet)\n"; return; }
    for (const auto& kv : m) {
        cout << "  [" << kv.first << "] " << kv.second.name;
        if (!kv.second.description.empty()) cout << " - " << in::clip(kv.second.description, 60);
        cout << "\n";
    }
}

std::optional<int> Editor::askEntity(EntityKind kind, const std::string& prompt) const {
    if (story_.entities(kind).empty()) return std::nullopt;
    listEntities(kind);
    for (;;) {
        auto v = in::readIntOrBlank(prompt, 0, kMaxId);
        if (!v) return std::nullopt;
        if (*v == 0 || story_.findEntity(kind, *v)) return v;
        cout << "  Unknown " << kindName(kind) << " id.\n";
    }
}

int Editor::countReferences(EntityKind kind, int id) const {
    int n = 0;
    for (const auto& kv : story_.scenes()) {
        const Scene& s = kv.second;
        if (kind == EntityKind::Character && s.hasCharacter(id)) ++n;
        if (kind == EntityKind::Location && s.locationId == id) ++n;
        if (kind == EntityKind::Item)
            for (const Choice& c : s.choices)
                if (c.requiredItemId == id || c.grantsItemId == id) ++n;
    }
    return n;
}

void Editor::entityMenu(EntityKind kind) {
    const std::string k = kindName(kind);
    for (;;) {
        cout << "\n--- " << k << "S ---\n 1. List\n 2. Add\n 3. Edit\n 4. Delete\n 0. Back\n";
        switch (in::readInt("Select: ", 0, 4)) {
            case 1: listEntities(kind); break;
            case 2: {
                std::string name = in::readNonEmpty(k + " name: ");
                std::string desc = in::trim(in::readLine("Description (optional): "));
                int id = story_.addEntity(kind, name, desc);
                cout << "  Added [" << id << "] " << name << ".\n";
                break;
            }
            case 3: {
                listEntities(kind);
                auto v = in::readIntOrBlank("Id to edit (blank = cancel): ", 1, kMaxId);
                if (!v) break;
                Entity* e = story_.findEntity(kind, *v);
                if (!e) { cout << "  Unknown id.\n"; break; }
                std::string name = in::trim(in::readLine("New name (blank = keep): "));
                std::string desc = in::trim(in::readLine("New description (blank = keep): "));
                if (!name.empty()) e->name = name;
                if (!desc.empty()) e->description = desc;
                cout << "  Updated.\n";
                break;
            }
            case 4: {
                listEntities(kind);
                auto v = in::readIntOrBlank("Id to delete (blank = cancel): ", 1, kMaxId);
                if (!v) break;
                if (!story_.findEntity(kind, *v)) { cout << "  Unknown id.\n"; break; }
                int refs = countReferences(kind, *v);
                if (refs) cout << "  It is used " << refs << " time(s); those references will be cleared.\n";
                if (in::confirm("Delete " + story_.entityName(kind, *v) + "?")) {
                    story_.removeEntity(kind, *v);
                    cout << "  Deleted.\n";
                }
                break;
            }
            default: return;
        }
    }
}

// ==================================================================== misc
void Editor::setStart() {
    listScenes();
    if (int id = pickScene("New start scene id")) {
        story_.setStartSceneId(id);
        cout << "  Start scene is now " << story_.sceneLabel(id) << ".\n";
    }
}

}  // namespace storylang
