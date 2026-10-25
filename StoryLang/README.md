# StoryLang

A console-based **interactive narrative design and exploration system** in C++17 with SQLite persistence.
A story is a **directed graph**: every scene is a node, every choice is an edge. StoryLang lets you build that
graph through menus, play it, **validate** it with graph algorithms, map it, and save it to a database.

## Quick start

```bash
# Linux / macOS (needs g++ or clang++, make, and SQLite - see below)
make            # builds build/storylang
make test       # builds and runs the test suite
./build/storylang            # database defaults to data/storylang.db
./build/storylang my.db      # or choose your own database file
```

Try it in 60 seconds: start the app, choose **9** (demo with 3 planted flaws), then **4** (Validate), fix the flaws
in **3** (Edit), validate again, then **5** (Play).

### SQLite - pick ONE
| Option | What to do |
|---|---|
| **System library** (default) | Debian/Ubuntu `sudo apt install libsqlite3-dev` - Fedora `sudo dnf install sqlite-devel` - macOS: already included - Windows (MSYS2): `pacman -S mingw-w64-x86_64-sqlite3` |
| **Bundled** (nothing to install) | Put `sqlite3.c` and `sqlite3.h` from the [SQLite amalgamation](https://www.sqlite.org/download.html) into `third_party/sqlite3/`. The Makefile/CMake pick it up automatically, so the project stays pure C++ + one C file. |

CMake alternative: `cmake -S . -B build && cmake --build build` (binaries in `build/bin/`, run tests with `ctest --test-dir build`).
On Windows with Visual Studio, use CMake and the bundled amalgamation.

## Main menu

```
 1. Create new story            6. Story map & analysis
 2. Load story from database    7. Save story to database
 3. Edit story                  8. Load demo story (clean)
 4. Validate story              9. Load demo story (3 planted flaws)
 5. Play story                 10. Delete a saved story
 0. Exit
```

* **Edit** - scenes, choices, characters, locations, items, start scene. Choices may point at scenes you haven't
  written yet; the validator reminds you. Deleting a scene asks whether to also remove the choices that lead to it.
* **Play** - `number` choose, `b` go back (restores items too), `i` inventory, `q` quit. Locked choices are shown with
  what they need.
* **Analysis** - ASCII story map, statistics + number of routes to each ending, shortest path to any ending,
  Graphviz `.dot` export (`dot -Tpng file.dot -o story.png`).

## What the validator checks

| Check | Severity | Algorithm |
|---|---|---|
| No scenes / no start scene / no ending | Error | direct |
| Choice points to a missing scene | Error | scan of adjacency lists |
| Unknown location / character / item reference | Error | lookup |
| Non-ending scene with no choices (dead end) | Error | scan |
| Ending unreachable from start | Error | BFS |
| Scene unreachable from start | Warning | BFS (endings are terminal) |
| Ending that has choices (never shown) | Warning | scan |
| Item-gated choice that can never unlock; scenes/endings hidden behind such gates | Warning / Error | BFS over **(scene, inventory)** states |
| Possible soft-lock (arrive without the item every exit needs) | Warning | same state-space BFS |
| Loops (revisitable scenes) | Info | DFS with white/grey/black colouring |

Loops are *legal* in interactive fiction, so they are only reported as information.

## Project layout

```
StoryLang/
├── include/                 headers (one per class)
├── src/
│   ├── model/               Story.cpp (graph + CRUD), SampleStories.cpp
│   ├── engine/              Player.cpp (play loop); GameState.h
│   ├── analysis/            Validator.cpp, PathFinder.cpp (BFS/DFS), StoryMap.cpp (ASCII + DOT)
│   ├── storage/             Database.cpp (RAII sqlite3 wrapper), StoryRepository.cpp
│   ├── ui/                  InputHelper.cpp (validated input), Editor.cpp
│   └── main.cpp             App: main menu, analysis menu
├── tests/test_main.cpp      dependency-free tests (338 checks)
├── third_party/sqlite3/     optional bundled SQLite
├── docs/                    ARCHITECTURE.md, DEMO_SCRIPT.md
├── Makefile  CMakeLists.txt
└── data/                    database and exports go here
```

## Design notes (talking points for a viva)

* **OOP** - `Story` encapsulates all mutation and keeps referential cleanup (`removeEntity` clears references).
  `IStoryStorage` is an abstract interface; `SqliteStoryRepository` is the concrete implementation, so the UI never
  touches SQLite directly (polymorphism / dependency inversion).
* **RAII** - `Database` owns `sqlite3*`, `Statement` owns `sqlite3_stmt*` (move-only), `Transaction` rolls back
  unless committed. No manual `close`/`finalize` anywhere outside the wrappers.
* **STL** - `std::map` (ordered, stable ids), `std::set` (inventories, visited), `std::queue` (BFS), `std::optional`,
  `std::vector`, `std::unique_ptr`.
* **Stable integer ids**, never reused after deletion, so a stale link can never silently point at a new scene.
* **Input safety** - all input is read with `getline` and parsed with `strtol` (no `cin` fail states); end-of-input
  throws `InputClosed` instead of looping forever.
* **Database safety** - prepared statements with bound parameters (no string-built SQL with user data) and one
  transaction per save, so a failed save never corrupts the previous copy. Verified with quotes, unicode and
  `DROP TABLE` text in a story name.
* **Item-aware analysis** - a plain graph BFS would call a gated scene "reachable" even if its key can never be
  found. Exploring `(scene, inventory)` states fixes that. Route counting forbids repeating a state, so loops
  can't make the count infinite.

### Database schema
```sql
stories(id, name UNIQUE, start_scene_id)
scenes(story_id, scene_id, title, body, location_id, is_ending)            -- PK (story_id, scene_id)
choices(story_id, scene_id, position, text, target_scene_id,
        required_item_id, grants_item_id)                                  -- no FK on target: dangling links can be saved
entities(story_id, kind, entity_id, name, description)                     -- kind: 0 character, 1 location, 2 item
scene_characters(story_id, scene_id, character_id)
```

## Known limitations
* Story and scene text is plain ASCII/UTF-8 text; no rich formatting.
* Analysis state limits (200,000 states; 2,000,000 DFS steps) keep huge stories responsive; the UI tells you when a
  result is a lower bound.
* Item *removal* (consumables) is not modelled - items are only gained.
