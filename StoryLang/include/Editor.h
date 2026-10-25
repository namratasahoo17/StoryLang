#pragma once
#include <optional>
#include <string>

#include "Story.h"

namespace storylang {

// Menu-driven editor for scenes, choices, characters, locations and items.
class Editor {
public:
    explicit Editor(Story& story) : story_(story) {}
    void run();

private:
    // scenes
    void sceneMenu();
    void listScenes() const;
    void viewScene(int id) const;
    void addScene();
    void editScene(int id);
    void deleteScene();
    int pickScene(const std::string& prompt) const;  // 0 = cancelled
    // choices
    void choiceMenu();
    void choicesLoop(int sceneId);
    void printChoices(const Scene& scene) const;
    void addChoice(int sceneId);
    void editChoice(int sceneId);
    void deleteChoice(int sceneId);
    // entities
    void entityMenu(EntityKind kind);
    void listEntities(EntityKind kind) const;
    std::optional<int> askEntity(EntityKind kind, const std::string& prompt) const;
    int countReferences(EntityKind kind, int id) const;
    // misc
    void setStart();
    std::string readBody() const;

    Story& story_;
};

}  // namespace storylang
