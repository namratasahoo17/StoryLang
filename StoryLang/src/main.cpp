// StoryLang - interactive narrative design & exploration system (console, C++17 + SQLite).
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "Editor.h"
#include "InputHelper.h"
#include "PathFinder.h"
#include "Player.h"
#include "SampleStories.h"
#include "Story.h"
#include "StoryMap.h"
#include "StoryRepository.h"
#include "Validator.h"

using namespace storylang;
using std::cout;
namespace in = storylang::input;

class App {
public:
    explicit App(const std::string& dbPath) : dbPath_(dbPath) {
        try {
            std::filesystem::path p(dbPath);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
            storage_ = std::make_unique<SqliteStoryRepository>(dbPath);
        } catch (const std::exception& e) {
            cout << "Warning: database unavailable (" << e.what()
                 << "). Save/load are disabled this session.\n";
        }
    }

    void run() {
        cout << "\n==============================================\n"
             << "   StoryLang - interactive narrative designer\n"
             << "==============================================\n";
        for (;;) {
            printMainMenu();
            int c = in::readInt("Select: ", 0, 10);
            try {
                switch (c) {
                    case 1: newStory(); break;
                    case 2: loadStory(); break;
                    case 3: if (requireStory()) { Editor(story_).run(); dirty_ = true; } break;
                    case 4: if (requireStory()) Validator::printReport(story_, Validator(story_).run()); break;
                    case 5: if (requireStory()) Player(story_).run(); break;
                    case 6: if (requireStory()) analysisMenu(); break;
                    case 7: if (requireStory()) saveStory(); break;
                    case 8: loadDemo(false); break;
                    case 9: loadDemo(true); break;
                    case 10: deleteSaved(); break;
                    case 0:
                        if (confirmLeave()) return;
                        break;
                }
            } catch (const InputClosed&) {
                throw;
            } catch (const std::exception& e) {
                cout << "\n  Error: " << e.what() << "\n";
            }
        }
    }

private:
    void printMainMenu() const {
        cout << "\n----------------------- MAIN MENU -----------------------\n";
        if (hasStory_)
            cout << " Current story: " << story_.name() << "  (" << story_.scenes().size()
                 << " scenes)" << (dirty_ ? "  *unsaved changes*" : "") << "\n";
        else
            cout << " No story loaded.\n";
        cout << "  1. Create new story            6. Story map & analysis\n"
             << "  2. Load story from database    7. Save story to database\n"
             << "  3. Edit story                  8. Load demo story (clean)\n"
             << "  4. Validate story              9. Load demo story (3 planted flaws)\n"
             << "  5. Play story                 10. Delete a saved story\n"
             << "  0. Exit\n";
    }

    bool requireStory() const {
        if (!hasStory_) cout << "\n  Load or create a story first (options 1, 2, 8 or 9).\n";
        return hasStory_;
    }

    bool confirmDiscard() {
        return !(hasStory_ && dirty_) ||
               in::confirm("You have unsaved changes that will be lost. Continue?");
    }

    bool confirmLeave() {
        if (hasStory_ && dirty_ && storage_) {
            if (in::confirm("Save '" + story_.name() + "' before exiting?")) saveStory();
        }
        cout << "\nGoodbye!\n";
        return true;
    }

    void setStory(Story s, const std::string& loadedName, bool dirty) {
        story_ = std::move(s);
        hasStory_ = true;
        dirty_ = dirty;
        loadedName_ = loadedName;
    }

    void newStory() {
        if (!confirmDiscard()) return;
        std::string name = in::readNonEmpty("Name of the new story: ");
        setStory(Story(name), "", true);
        cout << "  Created '" << name << "'. Use option 3 to add scenes.\n";
    }

    bool needStorage() const {
        if (!storage_) cout << "\n  Database is not available.\n";
        return storage_ != nullptr;
    }

    void loadStory() {
        if (!needStorage() || !confirmDiscard()) return;
        auto list = storage_->list();
        if (list.empty()) { cout << "\n  No saved stories in " << dbPath_ << " yet.\n"; return; }
        cout << "\nSaved stories:\n";
        for (std::size_t i = 0; i < list.size(); ++i)
            cout << "  " << (i + 1) << ". " << list[i].name << "  (" << list[i].sceneCount << " scenes)\n";
        auto v = in::readIntOrBlank("Load which one? (blank to cancel): ", 1, static_cast<int>(list.size()));
        if (!v) return;
        const std::string& name = list[static_cast<std::size_t>(*v - 1)].name;
        setStory(storage_->load(name), name, false);
        cout << "  Loaded '" << name << "'.\n";
    }

    void saveStory() {
        if (!needStorage()) return;
        const std::string& name = story_.name();
        if (storage_->exists(name) && name != loadedName_ &&
            !in::confirm("A saved story named '" + name + "' already exists. Overwrite it?"))
            return;
        storage_->save(story_);
        loadedName_ = name;
        dirty_ = false;
        cout << "  Saved '" << name << "' to " << dbPath_ << ".\n";
    }

    void deleteSaved() {
        if (!needStorage()) return;
        auto list = storage_->list();
        if (list.empty()) { cout << "\n  No saved stories.\n"; return; }
        for (std::size_t i = 0; i < list.size(); ++i)
            cout << "  " << (i + 1) << ". " << list[i].name << "\n";
        auto v = in::readIntOrBlank("Delete which one? (blank to cancel): ", 1, static_cast<int>(list.size()));
        if (!v) return;
        const std::string name = list[static_cast<std::size_t>(*v - 1)].name;
        if (in::confirm("Permanently delete '" + name + "' from the database?")) {
            storage_->remove(name);
            if (name == loadedName_) loadedName_.clear();
            cout << "  Deleted.\n";
        }
    }

    void loadDemo(bool flawed) {
        if (!confirmDiscard()) return;
        Story demo = makeDemoStory(flawed);
        std::string name = demo.name();
        setStory(std::move(demo), "", true);
        cout << "  Loaded '" << name << "'.\n";
        if (flawed) cout << "  Tip: run option 4 (Validate) - it contains 3 planted flaws.\n";
    }

    // ------------------------------------------------------------ analysis
    void analysisMenu() {
        for (;;) {
            cout << "\n--- STORY MAP & ANALYSIS ---\n"
                 << " 1. ASCII story map\n 2. Statistics and path counts\n"
                 << " 3. Shortest path to an ending\n 4. Export Graphviz .dot file\n 0. Back\n";
            switch (in::readInt("Select: ", 0, 4)) {
                case 1: cout << "\n" << asciiMap(story_); break;
                case 2: statistics(); break;
                case 3: shortestPath(); break;
                case 4: exportDot(); break;
                default: return;
            }
        }
    }

    void statistics() const {
        auto endings = story_.endingIds();
        std::size_t nonEnding = story_.scenes().size() - endings.size();
        std::size_t gated = 0, edges = story_.choiceCount();
        for (const auto& kv : story_.scenes())
            for (const Choice& c : kv.second.choices)
                if (c.isGated()) ++gated;

        cout << "\n===== STATISTICS: " << story_.name() << " =====\n"
             << " Scenes:      " << story_.scenes().size() << " (" << endings.size() << " endings)\n"
             << " Choices:     " << edges << " (" << gated << " item-gated)\n"
             << " Characters:  " << story_.entities(EntityKind::Character).size()
             << "   Locations: " << story_.entities(EntityKind::Location).size()
             << "   Items: " << story_.entities(EntityKind::Item).size() << "\n";
        if (nonEnding > 0) {
            double avg = static_cast<double>(edges) / static_cast<double>(nonEnding);
            cout << " Avg. choices per non-ending scene: " << std::fixed;
            cout.precision(2);
            cout << avg << "\n";
            cout.unsetf(std::ios::floatfield);
        }

        PathCount pc = PathFinder::countPaths(story_);
        cout << "\n Distinct routes to each ending (no repeated scene+inventory state):\n";
        for (int id : endings) {
            auto it = pc.perEnding.find(id);
            long long n = it == pc.perEnding.end() ? 0 : it->second;
            cout << "   " << story_.sceneLabel(id) << ": " << n << (n == 0 ? "   <-- no route!" : "") << "\n";
        }
        cout << " Total complete routes: " << pc.total << "\n";
        if (pc.truncated) cout << " (search limit reached - counts are lower bounds)\n";
    }

    void shortestPath() const {
        auto endings = story_.endingIds();
        if (endings.empty()) { cout << "\n  This story has no endings.\n"; return; }
        cout << "\nEndings:\n";
        for (std::size_t i = 0; i < endings.size(); ++i)
            cout << "  " << (i + 1) << ". " << story_.sceneLabel(endings[i]) << "\n";
        auto v = in::readIntOrBlank("Which ending? (blank to cancel): ", 1, static_cast<int>(endings.size()));
        if (!v) return;
        int target = endings[static_cast<std::size_t>(*v - 1)];

        auto path = PathFinder::shortestPath(story_, target);
        if (!path) {
            cout << "\n  No route from the start reaches that ending (taking item gates into account).\n";
            return;
        }
        cout << "\nShortest route (" << path->size() - 1 << " choices):\n";
        for (const PathStep& st : *path) {
            cout << "  " << story_.sceneLabel(st.sceneId) << "\n";
            if (st.choiceIndex >= 0) {
                const Scene* sc = story_.findScene(st.sceneId);
                cout << "      choose: \"" << sc->choices[static_cast<std::size_t>(st.choiceIndex)].text << "\"\n";
            }
        }
    }

    void exportDot() const {
        std::string base;
        for (char ch : story_.name())
            base += std::isalnum(static_cast<unsigned char>(ch)) ? ch : '_';
        std::string def = "data/" + base + ".dot";
        std::string path = in::trim(in::readLine("Output file [" + def + "]: "));
        if (path.empty()) path = def;
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        std::ofstream f(path);
        if (!f) { cout << "  Could not write " << path << ".\n"; return; }
        f << toDot(story_);
        cout << "  Wrote " << path << ".\n  Render it with:  dot -Tpng \"" << path << "\" -o story.png\n";
    }

    std::string dbPath_;
    std::unique_ptr<IStoryStorage> storage_;
    Story story_;
    bool hasStory_ = false;
    bool dirty_ = false;
    std::string loadedName_;
};

int main(int argc, char** argv) {
    std::string dbPath = argc > 1 ? argv[1] : "data/storylang.db";
    try {
        App app(dbPath);
        app.run();
    } catch (const InputClosed&) {
        cout << "\n\n(input closed - exiting)\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
