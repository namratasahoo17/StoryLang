#pragma once
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Entity.h"
#include "Scene.h"

namespace storylang {

// Story owns every scene and entity. All ids are stable integers (never vector
// indices) and are never reused after deletion, so a stale reference can never
// silently point at a different object.
class Story {
public:
    explicit Story(std::string name = "Untitled");

    const std::string& name() const { return name_; }
    void setName(std::string n) { name_ = std::move(n); }
    int startSceneId() const { return startSceneId_; }
    void setStartSceneId(int id) { startSceneId_ = id; }

    // ---- scenes -----------------------------------------------------------
    int addScene(const std::string& title, const std::string& text, bool isEnding = false);
    void putScene(const Scene& scene);  // keeps scene.id (used by the DB loader)
    // Returns -1 if the scene does not exist, otherwise the number of incoming
    // choices removed (only non-zero when cascade == true).
    int removeScene(int id, bool cascade);
    bool hasScene(int id) const { return scenes_.count(id) != 0; }
    Scene* findScene(int id);
    const Scene* findScene(int id) const;
    const std::map<int, Scene>& scenes() const { return scenes_; }

    // ---- choices ----------------------------------------------------------
    // The target is deliberately NOT checked here: authors may link to a scene
    // they have not written yet. The Validator reports unresolved links.
    bool addChoice(int sceneId, const Choice& choice);
    bool removeChoice(int sceneId, std::size_t index);

    // ---- characters / locations / items -----------------------------------
    int addEntity(EntityKind kind, const std::string& name, const std::string& description);
    void putEntity(EntityKind kind, const Entity& entity);
    // Removes the entity and every reference to it (scene links, choice gates).
    bool removeEntity(EntityKind kind, int id);
    const Entity* findEntity(EntityKind kind, int id) const;
    Entity* findEntity(EntityKind kind, int id);
    const std::map<int, Entity>& entities(EntityKind kind) const;
    std::string entityName(EntityKind kind, int id) const;  // "-" for id 0, "?" if unknown

    // ---- queries ----------------------------------------------------------
    std::vector<std::pair<int, std::size_t>> incomingChoices(int sceneId) const;
    std::size_t choiceCount() const;
    std::vector<int> endingIds() const;
    std::string sceneLabel(int id) const;  // "[3] Title" or "[3] <missing scene>"

private:
    std::map<int, Entity>& mapFor(EntityKind kind);
    const std::map<int, Entity>& mapFor(EntityKind kind) const;

    std::string name_;
    int startSceneId_ = 0;
    std::map<int, Scene> scenes_;
    std::map<int, Entity> characters_, locations_, items_;
    int nextSceneId_ = 1;
    int nextEntityId_[3] = {1, 1, 1};
};

}  // namespace storylang
