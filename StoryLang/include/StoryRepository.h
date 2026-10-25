#pragma once
#include <string>
#include <vector>

#include "Database.h"
#include "Story.h"

namespace storylang {

struct StoryInfo {
    std::string name;
    int sceneCount;
    int startSceneId;
};

// Storage abstraction: the UI depends on this interface, not on SQLite.
class IStoryStorage {
public:
    virtual ~IStoryStorage() = default;
    virtual void save(const Story& story) = 0;         // insert or overwrite by name
    virtual Story load(const std::string& name) = 0;   // throws std::runtime_error if missing
    virtual std::vector<StoryInfo> list() = 0;
    virtual bool remove(const std::string& name) = 0;
    virtual bool exists(const std::string& name) = 0;
};

class SqliteStoryRepository : public IStoryStorage {
public:
    explicit SqliteStoryRepository(const std::string& dbPath);

    void save(const Story& story) override;
    Story load(const std::string& name) override;
    std::vector<StoryInfo> list() override;
    bool remove(const std::string& name) override;
    bool exists(const std::string& name) override;

private:
    void createSchema();
    int findStoryId(const std::string& name);  // -1 if absent
    void clearChildren(int storyId);

    Database db_;
};

}  // namespace storylang
