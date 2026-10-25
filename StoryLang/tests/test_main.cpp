// Dependency-free test runner:  make test
#include <algorithm>
#include <iostream>

#include "InputHelper.h"
#include "PathFinder.h"
#include "SampleStories.h"
#include "Story.h"
#include "StoryMap.h"
#include "StoryRepository.h"
#include "Validator.h"

using namespace storylang;

static int g_failed = 0, g_run = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_run;                                                                 \
        if (!(cond)) { ++g_failed; std::cout << "  FAIL line " << __LINE__ << ": " #cond "\n"; } \
    } while (0)

static bool hasMessage(const std::vector<Issue>& v, Severity s, const std::string& needle) {
    return std::any_of(v.begin(), v.end(), [&](const Issue& i) {
        return i.severity == s && i.message.find(needle) != std::string::npos;
    });
}

static void testCleanStoryIsValid() {
    std::cout << "clean demo story validates\n";
    Story s = makeDemoStory(false);
    auto issues = Validator(s).run();
    CHECK(Validator::count(issues, Severity::Error) == 0);
    CHECK(Validator::count(issues, Severity::Warning) == 0);
    CHECK(Validator::count(issues, Severity::Info) > 0);  // it has loops
    CHECK(s.scenes().size() == 20);
    CHECK(s.endingIds().size() == 5);
}

static void testPlantedFlawsAreFound() {
    std::cout << "planted flaws are detected\n";
    Story s = makeDemoStory(true);
    auto issues = Validator(s).run();
    CHECK(hasMessage(issues, Severity::Warning, "Forgotten Boathouse"));
    CHECK(hasMessage(issues, Severity::Error, "Into the Fog"));
    CHECK(hasMessage(issues, Severity::Error, "scene 99"));
    CHECK(Validator::count(issues, Severity::Error) == 2);
    CHECK(Validator::count(issues, Severity::Warning) == 1);
}

static void testMissingStartAndEnding() {
    std::cout << "missing start / empty story\n";
    Story empty("x");
    CHECK(hasMessage(Validator(empty).run(), Severity::Error, "no scenes"));
    Story s("y");
    int a = s.addScene("A", "a");
    (void)a;
    auto issues = Validator(s).run();
    CHECK(hasMessage(issues, Severity::Error, "No start scene"));
    CHECK(hasMessage(issues, Severity::Error, "no ending"));
}

static void testCascadeDelete() {
    std::cout << "deleting a scene\n";
    Story s = makeDemoStory(false);
    int target = 0;
    for (const auto& kv : s.scenes()) if (kv.second.title == "The Cliff Path") target = kv.first;
    CHECK(target != 0);
    int incoming = static_cast<int>(s.incomingChoices(target).size());
    CHECK(incoming > 0);
    CHECK(s.removeScene(target, true) == incoming);
    CHECK(s.incomingChoices(target).empty());
    CHECK(s.removeScene(target, true) == -1);

    Story t = makeDemoStory(false);
    t.removeScene(target, false);  // leave dangling
    CHECK(hasMessage(Validator(t).run(), Severity::Error, "does not exist"));
}

static void testIdsAreNeverReused() {
    std::cout << "scene ids are stable\n";
    Story s("z");
    int a = s.addScene("A", "a"), b = s.addScene("B", "b");
    s.removeScene(b, true);
    int c = s.addScene("C", "c");
    CHECK(c != b && c > b && a != c);
}

static void testShortestPath() {
    std::cout << "shortest path honours items\n";
    Story s = makeDemoStory(false);
    int truth = 0, leave = 0;
    for (const auto& kv : s.scenes()) {
        if (kv.second.title == "Ending: The Keeper's Truth") truth = kv.first;
        if (kv.second.title == "Ending: Unfinished Business") leave = kv.first;
    }
    auto p = PathFinder::shortestPath(s, leave);
    CHECK(p && p->size() == 2);  // dock -> leave
    auto q = PathFinder::shortestPath(s, truth);
    CHECK(q.has_value());
    if (q) {
        CHECK(q->front().sceneId == s.startSceneId());
        CHECK(q->back().sceneId == truth);
        CHECK(q->back().choiceIndex == -1);
        CHECK(q->size() >= 9);  // key + lantern detours are unavoidable
    }
}

static void testPathCounts() {
    std::cout << "path enumeration\n";
    Story s = makeDemoStory(false);
    PathCount pc = PathFinder::countPaths(s);
    CHECK(!pc.truncated);
    CHECK(pc.total > 0);
    for (int id : s.endingIds()) CHECK(pc.perEnding.count(id) && pc.perEnding[id] > 0);
}

static void testItemGateAnalysis() {
    std::cout << "unobtainable item gate\n";
    Story s("gate");
    int key = s.addEntity(EntityKind::Item, "Golden Key", "");
    int a = s.addScene("Start", "x");
    int b = s.addScene("Vault", "x");
    int e1 = s.addScene("Easy end", "x", true);
    int e2 = s.addScene("Vault end", "x", true);
    s.setStartSceneId(a);
    s.addChoice(a, {"Take the easy road", e1, 0, 0});
    s.addChoice(a, {"Open the vault", b, key, 0});  // nobody ever grants the key
    s.addChoice(b, {"Finish", e2, 0, 0});
    auto issues = Validator(s).run();
    CHECK(hasMessage(issues, Severity::Warning, "can never be unlocked"));
    CHECK(hasMessage(issues, Severity::Error, "Vault end"));
    CHECK(!PathFinder::shortestPath(s, e2).has_value());
}

static void testSoftLock() {
    std::cout << "soft-lock detection\n";
    Story s("soft");
    int key = s.addEntity(EntityKind::Item, "Key", "");
    int a = s.addScene("Start", "x");
    int hall = s.addScene("Hall", "x");
    int door = s.addScene("Door", "x");
    int end = s.addScene("End", "x", true);
    s.setStartSceneId(a);
    s.addChoice(a, {"Grab key", hall, 0, key});
    s.addChoice(a, {"Skip key", hall, 0, 0});
    s.addChoice(hall, {"Go to the door", door, 0, 0});
    s.addChoice(door, {"Unlock", end, key, 0});
    CHECK(hasMessage(Validator(s).run(), Severity::Warning, "soft-lock"));
}

static void testMapsAndDot() {
    std::cout << "ascii map and dot\n";
    Story s = makeDemoStory(true);
    std::string m = asciiMap(s);
    CHECK(m.find("MISSING SCENE") != std::string::npos);
    CHECK(m.find("DEAD END") != std::string::npos);
    CHECK(m.find("Not reachable from the start") != std::string::npos);
    std::string d = toDot(s);
    CHECK(d.find("digraph") == 0);
    CHECK(d.find("missing99") != std::string::npos);
}

static void testRepositoryRoundTrip() {
    std::cout << "SQLite round trip\n";
    SqliteStoryRepository repo(":memory:");
    Story original = makeDemoStory(true);  // flaws survive a save too
    CHECK(!repo.exists(original.name()));
    repo.save(original);
    CHECK(repo.exists(original.name()));

    Story copy = repo.load(original.name());
    CHECK(copy.scenes().size() == original.scenes().size());
    CHECK(copy.choiceCount() == original.choiceCount());
    CHECK(copy.startSceneId() == original.startSceneId());
    CHECK(copy.entities(EntityKind::Item).size() == 3);
    CHECK(copy.entities(EntityKind::Character).size() == 3);
    for (const auto& kv : original.scenes()) {
        const Scene* c = copy.findScene(kv.first);
        CHECK(c != nullptr);
        if (!c) continue;
        CHECK(c->title == kv.second.title);
        CHECK(c->text == kv.second.text);
        CHECK(c->isEnding == kv.second.isEnding);
        CHECK(c->characterIds == kv.second.characterIds);
        CHECK(c->choices.size() == kv.second.choices.size());
        for (std::size_t i = 0; i < c->choices.size(); ++i) {
            CHECK(c->choices[i].text == kv.second.choices[i].text);
            CHECK(c->choices[i].targetSceneId == kv.second.choices[i].targetSceneId);
            CHECK(c->choices[i].requiredItemId == kv.second.choices[i].requiredItemId);
            CHECK(c->choices[i].grantsItemId == kv.second.choices[i].grantsItemId);
        }
    }

    // overwrite keeps exactly one copy and picks up changes
    original.addScene("Extra", "more");
    repo.save(original);
    CHECK(repo.list().size() == 1);
    CHECK(repo.load(original.name()).scenes().size() == original.scenes().size());

    // quotes / unicode / SQL metacharacters are stored verbatim
    Story tricky("Bob's \"story\"; DROP TABLE stories;--");
    int id = tricky.addScene("It's", "line1\nline2 \u00e9\u00e8 '--");
    tricky.setStartSceneId(id);
    repo.save(tricky);
    CHECK(repo.load(tricky.name()).findScene(id)->text == "line1\nline2 \u00e9\u00e8 '--");
    CHECK(repo.list().size() == 2);

    CHECK(repo.remove(original.name()));
    CHECK(!repo.remove(original.name()));
    CHECK(repo.list().size() == 1);
    bool threw = false;
    try { repo.load("nope"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

static void testInputHelpers() {
    std::cout << "input helpers\n";
    long v = 0;
    CHECK(input::parseInt("42", v) && v == 42);
    CHECK(input::parseInt("-7", v) && v == -7);
    CHECK(!input::parseInt("", v));
    CHECK(!input::parseInt("12abc", v));
    CHECK(!input::parseInt("abc", v));
    CHECK(!input::parseInt("1.5", v));
    CHECK(input::trim("  hi \t\n") == "hi");
    CHECK(input::wrapText("aaa bbb ccc", 7) == "aaa bbb\nccc");
    CHECK(input::clip("abcdefghij", 6) == "abc...");
}

int main() {
    testCleanStoryIsValid();
    testPlantedFlawsAreFound();
    testMissingStartAndEnding();
    testCascadeDelete();
    testIdsAreNeverReused();
    testShortestPath();
    testPathCounts();
    testItemGateAnalysis();
    testSoftLock();
    testMapsAndDot();
    testRepositoryRoundTrip();
    testInputHelpers();
    std::cout << "\n" << (g_run - g_failed) << "/" << g_run << " checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
