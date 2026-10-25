#include "StoryRepository.h"

namespace storylang {

SqliteStoryRepository::SqliteStoryRepository(const std::string& dbPath) : db_(dbPath) {
    createSchema();
}

void SqliteStoryRepository::createSchema() {
    db_.exec(R"SQL(
CREATE TABLE IF NOT EXISTS stories (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT    NOT NULL UNIQUE,
    start_scene_id INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS scenes (
    story_id    INTEGER NOT NULL,
    scene_id    INTEGER NOT NULL,
    title       TEXT    NOT NULL,
    body        TEXT    NOT NULL,
    location_id INTEGER NOT NULL DEFAULT 0,
    is_ending   INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (story_id, scene_id),
    FOREIGN KEY (story_id) REFERENCES stories(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS choices (
    story_id         INTEGER NOT NULL,
    scene_id         INTEGER NOT NULL,
    position         INTEGER NOT NULL,
    text             TEXT    NOT NULL,
    target_scene_id  INTEGER NOT NULL,
    required_item_id INTEGER NOT NULL DEFAULT 0,
    grants_item_id   INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (story_id, scene_id, position),
    FOREIGN KEY (story_id, scene_id) REFERENCES scenes(story_id, scene_id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS entities (
    story_id    INTEGER NOT NULL,
    kind        INTEGER NOT NULL,          -- 0 character, 1 location, 2 item
    entity_id   INTEGER NOT NULL,
    name        TEXT    NOT NULL,
    description TEXT    NOT NULL DEFAULT '',
    PRIMARY KEY (story_id, kind, entity_id),
    FOREIGN KEY (story_id) REFERENCES stories(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS scene_characters (
    story_id     INTEGER NOT NULL,
    scene_id     INTEGER NOT NULL,
    character_id INTEGER NOT NULL,
    PRIMARY KEY (story_id, scene_id, character_id),
    FOREIGN KEY (story_id, scene_id) REFERENCES scenes(story_id, scene_id) ON DELETE CASCADE
);
)SQL");
}

int SqliteStoryRepository::findStoryId(const std::string& name) {
    Statement st = db_.prepare("SELECT id FROM stories WHERE name = ?");
    st.bind(1, name);
    return st.step() ? st.columnInt(0) : -1;
}

bool SqliteStoryRepository::exists(const std::string& name) { return findStoryId(name) >= 0; }

void SqliteStoryRepository::clearChildren(int storyId) {
    // Children first; explicit so correctness never depends on the foreign_keys pragma.
    for (const char* table : {"scene_characters", "choices", "scenes", "entities"}) {
        Statement st = db_.prepare(std::string("DELETE FROM ") + table + " WHERE story_id = ?");
        st.bind(1, storyId);
        st.step();
    }
}

void SqliteStoryRepository::save(const Story& story) {
    Transaction tx(db_);  // all-or-nothing: a failed save never corrupts the old copy

    int storyId = findStoryId(story.name());
    if (storyId >= 0) {
        clearChildren(storyId);  // delete-and-reinsert is simpler than diffing
        Statement up = db_.prepare("UPDATE stories SET start_scene_id = ? WHERE id = ?");
        up.bind(1, story.startSceneId());
        up.bind(2, storyId);
        up.step();
    } else {
        Statement ins = db_.prepare("INSERT INTO stories(name, start_scene_id) VALUES(?, ?)");
        ins.bind(1, story.name());
        ins.bind(2, story.startSceneId());
        ins.step();
        storyId = static_cast<int>(db_.lastInsertId());
    }

    Statement insScene = db_.prepare(
        "INSERT INTO scenes(story_id, scene_id, title, body, location_id, is_ending) "
        "VALUES(?,?,?,?,?,?)");
    Statement insChoice = db_.prepare(
        "INSERT INTO choices(story_id, scene_id, position, text, target_scene_id, "
        "required_item_id, grants_item_id) VALUES(?,?,?,?,?,?,?)");
    Statement insSc = db_.prepare(
        "INSERT INTO scene_characters(story_id, scene_id, character_id) VALUES(?,?,?)");

    for (const auto& kv : story.scenes()) {
        const Scene& s = kv.second;
        insScene.reset();
        insScene.bind(1, storyId);
        insScene.bind(2, s.id);
        insScene.bind(3, s.title);
        insScene.bind(4, s.text);
        insScene.bind(5, s.locationId);
        insScene.bind(6, s.isEnding ? 1 : 0);
        insScene.step();

        for (std::size_t i = 0; i < s.choices.size(); ++i) {
            const Choice& c = s.choices[i];
            insChoice.reset();
            insChoice.bind(1, storyId);
            insChoice.bind(2, s.id);
            insChoice.bind(3, static_cast<int>(i));
            insChoice.bind(4, c.text);
            insChoice.bind(5, c.targetSceneId);
            insChoice.bind(6, c.requiredItemId);
            insChoice.bind(7, c.grantsItemId);
            insChoice.step();
        }
        for (int cid : s.characterIds) {
            insSc.reset();
            insSc.bind(1, storyId);
            insSc.bind(2, s.id);
            insSc.bind(3, cid);
            insSc.step();
        }
    }

    Statement insEnt = db_.prepare(
        "INSERT INTO entities(story_id, kind, entity_id, name, description) VALUES(?,?,?,?,?)");
    for (EntityKind k : {EntityKind::Character, EntityKind::Location, EntityKind::Item}) {
        for (const auto& kv : story.entities(k)) {
            insEnt.reset();
            insEnt.bind(1, storyId);
            insEnt.bind(2, static_cast<int>(k));
            insEnt.bind(3, kv.second.id);
            insEnt.bind(4, kv.second.name);
            insEnt.bind(5, kv.second.description);
            insEnt.step();
        }
    }
    tx.commit();
}

Story SqliteStoryRepository::load(const std::string& name) {
    int storyId = -1, start = 0;
    {
        Statement st = db_.prepare("SELECT id, start_scene_id FROM stories WHERE name = ?");
        st.bind(1, name);
        if (st.step()) {
            storyId = st.columnInt(0);
            start = st.columnInt(1);
        }
    }
    if (storyId < 0) throw std::runtime_error("No saved story named '" + name + "'.");

    Story story(name);
    story.setStartSceneId(start);

    {
        Statement st = db_.prepare(
            "SELECT scene_id, title, body, location_id, is_ending FROM scenes "
            "WHERE story_id = ? ORDER BY scene_id");
        st.bind(1, storyId);
        while (st.step()) {
            Scene s;
            s.id = st.columnInt(0);
            s.title = st.columnText(1);
            s.text = st.columnText(2);
            s.locationId = st.columnInt(3);
            s.isEnding = st.columnInt(4) != 0;
            story.putScene(s);
        }
    }
    {
        Statement st = db_.prepare(
            "SELECT scene_id, text, target_scene_id, required_item_id, grants_item_id "
            "FROM choices WHERE story_id = ? ORDER BY scene_id, position");
        st.bind(1, storyId);
        while (st.step()) {
            Choice c;
            c.text = st.columnText(1);
            c.targetSceneId = st.columnInt(2);
            c.requiredItemId = st.columnInt(3);
            c.grantsItemId = st.columnInt(4);
            story.addChoice(st.columnInt(0), c);
        }
    }
    {
        Statement st = db_.prepare(
            "SELECT scene_id, character_id FROM scene_characters WHERE story_id = ? "
            "ORDER BY scene_id, character_id");
        st.bind(1, storyId);
        while (st.step())
            if (Scene* s = story.findScene(st.columnInt(0))) s->characterIds.push_back(st.columnInt(1));
    }
    {
        Statement st = db_.prepare(
            "SELECT kind, entity_id, name, description FROM entities WHERE story_id = ? "
            "ORDER BY kind, entity_id");
        st.bind(1, storyId);
        while (st.step()) {
            Entity e;
            e.id = st.columnInt(1);
            e.name = st.columnText(2);
            e.description = st.columnText(3);
            story.putEntity(static_cast<EntityKind>(st.columnInt(0)), e);
        }
    }
    return story;
}

std::vector<StoryInfo> SqliteStoryRepository::list() {
    std::vector<StoryInfo> out;
    Statement st = db_.prepare(
        "SELECT s.name, s.start_scene_id, "
        "(SELECT COUNT(*) FROM scenes WHERE story_id = s.id) FROM stories s ORDER BY s.name");
    while (st.step()) out.push_back({st.columnText(0), st.columnInt(2), st.columnInt(1)});
    return out;
}

bool SqliteStoryRepository::remove(const std::string& name) {
    int id = findStoryId(name);
    if (id < 0) return false;
    Transaction tx(db_);
    clearChildren(id);
    Statement st = db_.prepare("DELETE FROM stories WHERE id = ?");
    st.bind(1, id);
    st.step();
    tx.commit();
    return true;
}

}  // namespace storylang
