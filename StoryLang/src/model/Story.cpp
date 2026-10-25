#include "Story.h"

#include <algorithm>

namespace storylang {

Story::Story(std::string name) : name_(std::move(name)) {}

// ---------------------------------------------------------------- scenes
int Story::addScene(const std::string& title, const std::string& text, bool isEnding) {
    Scene s;
    s.id = nextSceneId_++;
    s.title = title;
    s.text = text;
    s.isEnding = isEnding;
    scenes_[s.id] = s;
    return s.id;
}

void Story::putScene(const Scene& scene) {
    scenes_[scene.id] = scene;
    nextSceneId_ = std::max(nextSceneId_, scene.id + 1);
}

int Story::removeScene(int id, bool cascade) {
    auto it = scenes_.find(id);
    if (it == scenes_.end()) return -1;
    scenes_.erase(it);
    if (startSceneId_ == id) startSceneId_ = 0;

    int removed = 0;
    if (cascade) {
        for (auto& kv : scenes_) {
            auto& ch = kv.second.choices;
            auto newEnd = std::remove_if(ch.begin(), ch.end(), [&](const Choice& c) {
                return c.targetSceneId == id;
            });
            removed += static_cast<int>(ch.end() - newEnd);
            ch.erase(newEnd, ch.end());
        }
    }
    return removed;
}

Scene* Story::findScene(int id) {
    auto it = scenes_.find(id);
    return it == scenes_.end() ? nullptr : &it->second;
}

const Scene* Story::findScene(int id) const {
    auto it = scenes_.find(id);
    return it == scenes_.end() ? nullptr : &it->second;
}

// --------------------------------------------------------------- choices
bool Story::addChoice(int sceneId, const Choice& choice) {
    Scene* s = findScene(sceneId);
    if (!s) return false;
    s->choices.push_back(choice);
    return true;
}

bool Story::removeChoice(int sceneId, std::size_t index) {
    Scene* s = findScene(sceneId);
    if (!s || index >= s->choices.size()) return false;
    s->choices.erase(s->choices.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

// -------------------------------------------------------------- entities
std::map<int, Entity>& Story::mapFor(EntityKind kind) {
    switch (kind) {
        case EntityKind::Character: return characters_;
        case EntityKind::Location:  return locations_;
        default:                    return items_;
    }
}

const std::map<int, Entity>& Story::mapFor(EntityKind kind) const {
    switch (kind) {
        case EntityKind::Character: return characters_;
        case EntityKind::Location:  return locations_;
        default:                    return items_;
    }
}

int Story::addEntity(EntityKind kind, const std::string& name, const std::string& description) {
    Entity e;
    e.id = nextEntityId_[static_cast<int>(kind)]++;
    e.name = name;
    e.description = description;
    mapFor(kind)[e.id] = e;
    return e.id;
}

void Story::putEntity(EntityKind kind, const Entity& entity) {
    mapFor(kind)[entity.id] = entity;
    int& next = nextEntityId_[static_cast<int>(kind)];
    next = std::max(next, entity.id + 1);
}

bool Story::removeEntity(EntityKind kind, int id) {
    if (mapFor(kind).erase(id) == 0) return false;
    for (auto& kv : scenes_) {
        Scene& s = kv.second;
        switch (kind) {
            case EntityKind::Character:
                s.characterIds.erase(
                    std::remove(s.characterIds.begin(), s.characterIds.end(), id),
                    s.characterIds.end());
                break;
            case EntityKind::Location:
                if (s.locationId == id) s.locationId = 0;
                break;
            case EntityKind::Item:
                for (Choice& c : s.choices) {
                    if (c.requiredItemId == id) c.requiredItemId = 0;
                    if (c.grantsItemId == id) c.grantsItemId = 0;
                }
                break;
        }
    }
    return true;
}

const Entity* Story::findEntity(EntityKind kind, int id) const {
    const auto& m = mapFor(kind);
    auto it = m.find(id);
    return it == m.end() ? nullptr : &it->second;
}

Entity* Story::findEntity(EntityKind kind, int id) {
    auto& m = mapFor(kind);
    auto it = m.find(id);
    return it == m.end() ? nullptr : &it->second;
}

const std::map<int, Entity>& Story::entities(EntityKind kind) const { return mapFor(kind); }

std::string Story::entityName(EntityKind kind, int id) const {
    if (id == 0) return "-";
    const Entity* e = findEntity(kind, id);
    return e ? e->name : "?";
}

// --------------------------------------------------------------- queries
std::vector<std::pair<int, std::size_t>> Story::incomingChoices(int sceneId) const {
    std::vector<std::pair<int, std::size_t>> result;
    for (const auto& kv : scenes_) {
        const auto& ch = kv.second.choices;
        for (std::size_t i = 0; i < ch.size(); ++i)
            if (ch[i].targetSceneId == sceneId) result.emplace_back(kv.first, i);
    }
    return result;
}

std::size_t Story::choiceCount() const {
    std::size_t n = 0;
    for (const auto& kv : scenes_) n += kv.second.choices.size();
    return n;
}

std::vector<int> Story::endingIds() const {
    std::vector<int> ids;
    for (const auto& kv : scenes_)
        if (kv.second.isEnding) ids.push_back(kv.first);
    return ids;
}

std::string Story::sceneLabel(int id) const {
    const Scene* s = findScene(id);
    return "[" + std::to_string(id) + "] " + (s ? s->title : std::string("<missing scene>"));
}

}  // namespace storylang
