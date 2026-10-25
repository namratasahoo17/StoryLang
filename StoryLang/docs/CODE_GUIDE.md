# StoryLang - Code Guide (every file, step by step)

Read this top to bottom once and you will be able to explain any part of the project in class.

---------------------------------------------------------------------------------------------------

## 0. The big picture in 60 seconds

**StoryLang treats a story as a directed graph.**

* A **scene** is a *node*.
* A **choice** is an *edge* (an arrow from one scene to another).
* An **ending** is a node where play stops.
* A choice can **need an item** or **give an item** (keys, lanterns...).

The program has three layers. Each layer only uses the layer(s) below it:

```
  +--------------------------------------------------------------+
  |  UI layer       main.cpp  Editor  Player  Ui  InputHelper    |   what the user sees
  +--------------------------------------------------------------+
  |  Logic layer    Validator  PathFinder  StoryMap              |   graph algorithms
  +--------------------------------------------------------------+
  |  Data layer     Story  Scene  Choice  Entity  |  Database    |   the model + SQLite
  |                                               |  StoryRepository
  +--------------------------------------------------------------+
```

**Suggested reading order:** `Scene.h` -> `Story.h/.cpp` -> `PathFinder` -> `Validator` -> `Player` ->
`Database` -> `StoryRepository` -> `Ui` -> `Editor` -> `main.cpp` -> `tests`.

### Words you must know (graph vocabulary)

| Word | Meaning here |
|---|---|
| **Node / vertex** | a scene |
| **Edge** | a choice (scene A -> scene B) |
| **Directed graph** | edges have a direction: A -> B does not mean B -> A |
| **Adjacency list** | each node stores the list of edges leaving it. `Scene::choices` *is* the adjacency list |
| **BFS** (breadth-first search) | visit the start, then all its neighbours, then *their* neighbours... using a **queue**. Finds everything reachable and the *shortest* paths |
| **DFS** (depth-first search) | go as deep as possible along one route, then **backtrack**. Used for loops and for counting routes |
| **Cycle / loop** | a route that returns to a scene already on the route |
| **State** | (current scene + items held). Two visits to the same scene with different items are different states |

---------------------------------------------------------------------------------------------------

## 1. The data layer

### `include/Entity.h`
Defines what a **character, location or item** looks like. They all have the same three fields, so there is
one struct and an enum to say which kind it is:

```cpp
enum class EntityKind { Character, Location, Item };
struct Entity { int id; std::string name; std::string description; };
```
`kindName()` turns the enum into text ("Character") for menus.
*Why one struct?* Less duplicated code: one add/edit/delete routine serves all three kinds.

### `include/Scene.h`
Two small structs, the heart of the model:

```cpp
struct Choice { text; targetSceneId; requiredItemId; grantsItemId; };   // an EDGE
struct Scene  { id; title; text; locationId; characterIds; isEnding; vector<Choice> choices; };  // a NODE
```
* `targetSceneId` is the **id** of the scene this choice leads to (never a pointer or vector index - see below).
* `requiredItemId == 0` means "no item needed"; `grantsItemId == 0` means "gives nothing". (Ids start at 1,
  so 0 is a safe "none".)
* `Scene::choices` is the adjacency list.

### `include/Story.h` + `src/model/Story.cpp`
`Story` **owns everything**: all scenes (in a `std::map<int, Scene>`), the three entity maps, the start scene
id and the story name.

**Key design decision - stable integer ids.** Counters `nextSceneId_` and `nextEntityId_[3]` only ever go
up. If you delete scene 7, the next new scene is still 21, never 7. So an old link can never silently start
pointing at a different scene.

Function by function:

| Function | What it does, step by step |
|---|---|
| `addScene(title, text, isEnding)` | takes `nextSceneId_++` as the id, builds a `Scene`, stores it in the map, returns the id |
| `putScene(scene)` | stores a scene *with its own id* (used when loading from the database) and bumps the counter past that id |
| `removeScene(id, cascade)` | erases the scene; clears `startSceneId_` if it was the start. If `cascade` is true it also walks every other scene and removes choices that pointed at the deleted one (using `std::remove_if` + `erase`), counting them. Returns -1 if the id did not exist |
| `addChoice(sceneId, choice)` | appends to that scene's `choices`. **It does not check that the target exists** - on purpose, so authors can link to a scene they have not written yet. The Validator reports unresolved links |
| `removeChoice(sceneId, index)` | bounds-checked erase from the vector |
| `addEntity / putEntity / findEntity` | same ideas for characters, locations, items (a private helper `mapFor(kind)` picks the right map) |
| `removeEntity(kind, id)` | erases the entity **and cleans every reference**: removes the character from scenes' `characterIds`, resets `locationId` to 0, clears item requirements/grants on choices |
| `incomingChoices(sceneId)` | scans all scenes and returns `(scene, choiceIndex)` pairs that point at `sceneId` - used to warn before deleting |
| `endingIds()`, `choiceCount()`, `sceneLabel(id)` | small query helpers (`sceneLabel` gives `"[3] Title"` or `"[3] <missing scene>"`) |

*Why `std::map` and not `unordered_map`?* It keeps scenes sorted by id, so lists and files always come out in
a predictable order.

### `include/SampleStories.h` + `src/model/SampleStories.cpp`
`makeDemoStory(bool withFlaws)` builds "Harrow Island": 6 locations, 3 characters, 3 items, 20 scenes,
5 endings. It stores the id returned by each `addScene` in a variable (`dock`, `inn`, ...) and then wires the
choices with a tiny helper `link(from, text, to, requiredItem, grantedItem)`, so there are no magic numbers.

The text uses a **dialogue convention**: a line like `Mara Voss: Evening...` is spoken by that character
(the Player shows it in a name-tagged box). Any other line is narration.

With `withFlaws = true` it adds the three defects for the live demo:
1. *The Forgotten Boathouse* - a scene nothing links to (unreachable),
2. *Into the Fog* - reachable but has no choices and is not an ending (dead end),
3. a Lighthouse choice pointing to scene `99`, which does not exist (dangling link).

---------------------------------------------------------------------------------------------------

## 2. The logic layer (graph algorithms)

### `include/PathFinder.h` + `src/analysis/PathFinder.cpp`
Four static functions. All of them treat **endings as terminal** (play stops there, so their outgoing
choices are ignored).

**`choiceAvailable(choice, inventory)`** - true if the choice needs no item or the inventory contains it.

**`reachableScenes(story)`** - plain BFS:
1. Put the start scene in a queue and in a `seen` set.
2. Pop a scene; for each of its choices whose target exists and is not yet seen, mark it seen and push it.
3. Repeat until the queue is empty. `seen` = everything reachable.

*Complexity:* O(V + E), each scene and choice looked at once.

**`explore(story)`** - the clever one: **BFS over states**, not scenes. A state is `(scene, set of items)`.
1. Start with `(start scene, {})`.
2. For each state, for each choice that is *available with the current items*: copy the items, add the item the
   choice grants, and the new state is `(target, items)`.
3. A map `seen` makes sure each state is expanded once; a cap of 200,000 states keeps it fast.
4. It records: which scenes were reached, which `(scene, choice)` pairs *can* fire, and **soft-locks** (a state
   where at least one choice exists but *every* choice is blocked by a missing item).

*Why not plain BFS?* A plain BFS says "the Cellar is reachable" because an arrow leads there. But if the key
that opens it can never be picked up, the player can never get in. Only state-space search sees that.

**`shortestPath(story, target)`** - the same state BFS, but each node remembers its **parent** and the choice
that led to it. When a state whose scene is `target` is found, walk the parents back to the start and reverse.
BFS guarantees the first hit is the fewest choices.

**`countPaths(story)`** - **DFS with backtracking**:
1. Recurse into every available choice, carrying the current inventory.
2. Keep a set `onPath` of states on the current route; never enter a state already on it (so loops cannot
   cause infinite counting).
3. On reaching an ending, add 1 to that ending's counter.
4. After returning from a recursive call, **undo** the changes (remove the item just added, erase from
   `onPath`) - that undo is the "backtracking".
5. A depth limit (40) and step limit (2,000,000) stop huge stories from freezing; the UI reports "lower bound".

### `include/Validator.h` + `src/analysis/Validator.cpp`
Produces a list of `Issue { severity, message, sceneId }`. `run()` calls six checks, then sorts the list so
errors come first (`std::stable_sort`).

| Check | How it works |
|---|---|
| `checkStructure` | any scenes? start scene set and existing? at least one ending? |
| `checkReferences` | for every scene/choice: does the target scene exist? do the location, characters and items it mentions exist? |
| `checkReachability` | BFS from `PathFinder::reachableScenes`; every scene not in the set is unreachable (an *ending* that is unreachable is an Error, an ordinary scene a Warning) |
| `checkDeadEnds` | non-ending scene with zero choices = Error; ending that has choices = Warning (they are never shown) |
| `checkCycles` | **DFS with three colours**: white = not visited, grey = on the current route, black = finished. Meeting a *grey* scene again means a back edge = a loop. Reported as Info only, because loops are legal in stories (e.g. walking back to the inn). Duplicate edges are listed once |
| `checkInventory` | runs `PathFinder::explore` (only if the story has any gated choice). Reports choices that can never unlock, scenes/endings reachable only through impossible gates, and soft-locks |

`printReport()` draws the result: a header box, a coloured badge per issue (ERROR / WARN / NOTE; notes capped
at 3 so loops do not drown the real problems) and a PASS/FAIL box. **PASS means zero errors.**

### `include/StoryMap.h` + `src/analysis/StoryMap.cpp`
* `asciiMap(story)` - prints the story as a tree starting at the start scene. A recursive function
  `printChildren` prints each choice with tree glyphs; a `shown` set stops it re-expanding scenes already
  printed (it writes "shown above" instead), so loops cannot recurse forever. Tags: `(END)`, `(DEAD END)`,
  `!! MISSING SCENE !!`, `{needs X}`, `{gives X}`. Scenes never printed are listed under "Not reachable".
* `toDot(story)` - produces **Graphviz** source (`digraph { s1 -> s2 [label="..."] }`). Endings are double
  circles, the start is green, dead ends red, item-gated edges dashed, missing targets get a red dashed node.
  `esc()` escapes quotes/newlines so any text is safe.

---------------------------------------------------------------------------------------------------

## 3. The play engine

### `include/GameState.h` (header only)
Everything that changes while playing: current scene, inventory (`std::set<int>`), visited scenes, and a
**history stack** of `Snapshot { sceneId, inventory }`.
* `apply(choice)` - push a snapshot of "where I was + items I had", add the granted item, move to the target.
* `undo()` - pop the snapshot and restore **both** position and items. That is why `b` (back) can even
  "un-pick-up" a key.

### `include/Player.h` + `src/engine/Player.cpp`
Plays the story in a **visual-novel style** (inspired by Ren'Py, with no images):

1. **Intro card** with the controls.
2. **Per scene** (`showScene`): text is split into *beats* (one per line). `parseBeats` checks whether a line
   starts with a character's name followed by `:`. If so it is dialogue (name-tag box, border colour per
   character, text in “curly quotes”); otherwise narration.
3. The **backdrop** (`ui::backdrop`) is a coloured band showing the location name in spaced capitals and the
   scene title. Each location id picks one of 8 deep colours; endings are gold.
4. On a **first visit** in a real terminal: for each beat the screen is cleared, the stage and one beat are
   drawn (typed out letter by letter), and Enter ("» continue") advances. On a **revisit** or when output is
   piped, all beats appear at once - no waiting.
5. **Choices** (`showChoices`) are numbered buttons. Locked ones are greyed with `[locked: needs Brass Key]`.
   A dim status line shows the step count and items.
6. **Input loop**: a number picks a choice (checked: valid number, not locked, target exists); `b` undo;
   `i` inventory box; `t` typewriter on/off; `q` quit (with confirmation).
7. Picking up an item stores a message in `notice_` that appears on the next scene (otherwise the screen
   clear would erase it instantly).
8. **Endings** show a gold "THE END" box with steps, scenes explored and items collected as progress bars.

---------------------------------------------------------------------------------------------------

## 4. Storage (SQLite from C++)

### `include/Database.h` + `src/storage/Database.cpp`
Three small classes that wrap SQLite's C API using **RAII** (resource acquired in the constructor, released
automatically in the destructor - no manual cleanup, no leaks even when exceptions happen):

* **`Database`** - constructor `sqlite3_open`s the file and turns on foreign keys; destructor `sqlite3_close`s.
  `exec(sql)` runs statements that return nothing; failures throw `std::runtime_error` with SQLite's message.
* **`Statement`** - owns a prepared `sqlite3_stmt*`. `bind(index, value)` fills the `?` placeholders (indexes
  start at 1), `step()` runs it (true = a row is ready), `columnInt/columnText` read results, `reset()` re-uses
  it. It is **move-only** (copying a raw handle would double-free it).
* **`Transaction`** - `BEGIN` in the constructor; `commit()` makes it permanent; if the object is destroyed
  without commit (an exception happened) the destructor issues `ROLLBACK`. All-or-nothing.

### `include/StoryRepository.h` + `src/storage/StoryRepository.cpp`
* **`IStoryStorage`** is an **abstract interface** (`save`, `load`, `list`, `remove`, `exists`, all
  `= 0`). The UI only knows this interface.
* **`SqliteStoryRepository`** implements it. If you wanted files or a server instead, you would write another
  class and the UI would not change (**polymorphism / dependency inversion**).

Schema (5 tables):
```
stories(id, name UNIQUE, start_scene_id)
scenes(story_id, scene_id, title, body, location_id, is_ending)
choices(story_id, scene_id, position, text, target_scene_id, required_item_id, grants_item_id)
entities(story_id, kind, entity_id, name, description)        -- kind 0/1/2 = character/location/item
scene_characters(story_id, scene_id, character_id)
```
`target_scene_id` deliberately has **no** foreign key so a story with a dangling link can still be saved.

**`save()`** step by step: open a `Transaction` -> look up the story id by name -> if it exists, delete its
child rows and update the start id; otherwise insert a new `stories` row and take `lastInsertId()` ->
prepare insert statements once -> loop over scenes (insert scene, its choices with their `position`, its
characters) -> insert all entities -> `commit()`. If anything throws, the old copy is untouched.

**`load()`**: find the story row -> `SELECT` scenes (`putScene`) -> `SELECT` choices ordered by
`scene_id, position` (so order is preserved) -> scene_characters -> entities (`putEntity`) -> return the
rebuilt `Story`. **All user text is passed through `bind`, never concatenated into SQL**, so quotes or
`DROP TABLE` in a title are harmless (a test checks exactly that).

---------------------------------------------------------------------------------------------------

## 5. The UI layer

### `include/Ui.h` + `src/ui/Ui.cpp` - the "look" of the program
Everything that draws goes through here, so there is one style and one place to change it.

* **Modes** (`init`): colour is on only if stdout is a real terminal, `NO_COLOR` is unset and `TERM` is not
  `dumb`; `--plain` forces ASCII/no colour, `--color` forces colour. `interactive()` = both stdin and stdout
  are terminals - this is what allows screen clearing and click-to-continue, and it keeps piped/scripted use
  (and the tests) clean.
* **Glyph sets**: modern Unicode (`╭─╮│`), a *classic* set that only uses characters the old Windows console
  font has, and plain ASCII (`+-|`). `g()` returns the right one.
* **Colours**: `paint(Color, text, bold)` wraps text in ANSI escape codes (xterm-256 colours) - and returns
  the text unchanged when colour is off.
* **`displayWidth`** counts *visible* characters: it skips ANSI escape sequences and counts UTF-8 characters
  once. All padding uses it - that is why boxes stay perfectly aligned even with colours and accents.
* **Widgets**: `box` (rounded frame with a title and optional right-hand title), `menu` (key + label in
  columns), `table` (auto-sized columns with a header), `bar` (progress bar), `rule`, `banner` (title-screen
  logo on a navy band), `backdrop` (the coloured scene band), and one-line messages `ok / err / warn / note /
  hint`.
* `typeOut` prints a line one character at a time (escape sequences are printed instantly) using a small
  portable `sleepMs`.
* `wrapLines`, `clip`, `padLeft/Right`, `center`: text helpers, all UTF-8 aware.

### `include/InputHelper.h` + `src/ui/InputHelper.cpp` - safe input
* Reads **whole lines** with `std::getline` and parses with `strtol`, so typing `abc` can never put `cin`
  into a broken state (the classic C++ input bug).
* `readInt(prompt, min, max)` repeats until valid; `readIntOrBlank` allows empty (= cancel/keep);
  `confirm` asks y/n; `readMultiline` reads until a line containing just `.`.
* If input ends (Ctrl-D or end of a piped file), it throws `InputClosed`, which `main` catches - no infinite
  loops.
* `waitEnter` / `pause` show the dim "press Enter" prompts.

### `include/Editor.h` + `src/ui/Editor.cpp` - the story builder
A class holding a reference to the `Story` it edits. Menus: Scenes, Choices, Characters, Locations, Items,
Set start, Rename. Highlights:
* `addScene` asks for title, multi-line text, ending yes/no; the first scene created becomes the start scene.
* `addChoice` warns (but allows) a target that does not exist yet.
* `deleteScene` shows how many choices point at the scene and offers **cascade delete** (safe) or **leave
  dangling** (useful for demonstrating the validator).
* `askEntity` shows a table of items/characters/locations and returns `optional<int>`
  (blank = keep, 0 = clear, otherwise the id).
* Deleting an entity first counts and reports how many places use it.

### `src/main.cpp` - the application
* **`main()`** parses `--plain / --color / --classic / --help`, picks the database path (default:
  `data/storylang.db` **next to the program** - ideal for a pendrive), calls `ui::init`, and runs `App`,
  catching `InputClosed` and other exceptions. On Windows it pauses at the end so a double-clicked window
  does not vanish.
* **`class App`** holds the current `Story`, a `dirty_` flag ("unsaved changes"), the storage interface and
  the name it was loaded under.
  * `run()` loops: draw main menu -> read a number -> `switch`. Exceptions inside an action are caught and
    shown, so one error never kills the program.
  * `printMainMenu()` draws the banner, the *Current story* panel (name, saved/unsaved, counts and a **live
    health check** that runs the validator) and the menu.
  * `saveStory/loadStory/deleteSaved` talk to `IStoryStorage` only.
  * `analysisMenu`: `statistics()` (overview box + bar chart of routes per ending), `shortestPath()` (a
    timeline box), `exportDot()`.

---------------------------------------------------------------------------------------------------

## 6. Tests and build files

### `tests/test_main.cpp`
A tiny test runner with a `CHECK(cond)` macro (no external library). It verifies: the clean demo has no
errors/warnings; the flawed demo has exactly 2 errors + 1 warning and names each flaw; empty/missing-start
stories; cascade delete and dangling links; ids never reused; shortest paths; route counts; unobtainable item
gates; soft-locks; map and DOT output; the **SQLite round trip** (everything saved is loaded back identical,
overwrite keeps one copy, quotes/unicode/`DROP TABLE` survive, delete works, loading a missing name throws);
input parsing; and that every box/menu/table line is exactly 78 columns in all glyph modes.
Run with `make test` - currently **346 checks**.

### `Makefile`
`make` builds `build/storylang`; `make test` builds and runs the tests; `make run`; `make clean`;
`make windows` cross-compiles `StoryLang.exe` (needs mingw-w64). If `third_party/sqlite3/sqlite3.c` exists it
is compiled in, otherwise `-lsqlite3` is used.

### `CMakeLists.txt`
Same build for CMake users (static library `storylang_core`, executable `storylang`, test executable).

### `build_windows.bat`
One-click Windows build -> `StoryLang.exe` (see section 9).

### `third_party/sqlite3/`
Where `sqlite3.c` / `sqlite3.h` (the SQLite "amalgamation") go for a self-contained build.

---------------------------------------------------------------------------------------------------

## 7. Two complete walk-throughs

### A. You type `4` (Validate) on the flawed demo
1. `App::run` reads `4` -> `Validator(story_).run()`.
2. `checkStructure`: story has scenes, start set, endings exist - no issues.
3. `checkReferences`: loops over all 22 scenes; the Lighthouse choice #4 targets scene 99 and
   `hasScene(99)` is false -> **Error: points to scene 99, which does not exist**.
4. `checkReachability`: BFS from the dock never reaches the Boathouse -> **Warning**.
5. `checkDeadEnds`: *Into the Fog* has zero choices and `isEnding` is false -> **Error**.
6. `checkCycles`: DFS colours find loops (inn <-> dock, ...) -> a few **Notes**.
7. `checkInventory`: three gated choices exist, so `explore` runs; every gate can be satisfied -> nothing.
8. Issues are sorted errors-first and `printReport` draws badges and the red FAIL box (2 errors, 1 warning).

### B. You choose option 2 on a scene while playing
1. `Player::run` reads `2` -> `parseInt` -> within `1..n`.
2. It checks `state_.has(requiredItemId)` (locked?) and `story_.hasScene(target)` (broken link?).
3. `fresh` = target not yet in `visited()` (decides whether text animates).
4. `state_.apply(choice)`: snapshot pushed, item added if the choice grants one, current scene = target.
5. Loop top: `findScene` -> `showScene` (backdrop + beats) -> if it is an ending, `showEnding` and return;
   otherwise `showChoices` and wait for input again.

---------------------------------------------------------------------------------------------------

## 8. Presenting it in class

### 2-minute pitch (say this)
> "Interactive stories are really graphs: scenes are nodes and choices are edges. As a story grows, you lose
> track of that graph - links break, scenes become unreachable, players get stuck. Normal writing tools only
> store text. StoryLang models the story as a graph, so I can **validate it with algorithms**, **map it**,
> **play it**, and **save it in a database** - all in pure C++."

### 5-minute demo
1. Launch -> title screen. Press `9` (demo with 3 planted flaws).
2. Press `4`. Point at ERROR / ERROR / WARN and say which algorithm found each.
3. Press `6` -> `1`: the map shows `MISSING SCENE`, `DEAD END`, and the unreachable list.
4. Press `3` and fix the flaws (delete the Boathouse with cascade, add a "Turn back" choice to the fog, delete
   the broken lighthouse choice). Press `4` again: **PASS**.
5. `6` -> `2`: routes per ending bar chart. `6` -> `3`: shortest path to *The Keeper's Truth*.
6. `7` save, `0` exit, run again, `2` load - the fixed story is still there (database).
7. `5` Play: click through the dialogue, show a locked option, `i`, `b`, reach an ending.

### Questions you may be asked - and answers
1. **Why a graph?** Branching = nodes + directed edges; this unlocks standard algorithms.
2. **Why BFS for reachability?** It visits each scene once, O(V+E), and a queue is simple.
3. **How do you detect loops?** DFS with 3 colours; reaching a grey (in-progress) node = back edge.
4. **Why not just a plain graph check for items?** Reachability depends on what you carry, so the search runs
   over (scene, inventory) states.
5. **Why integer ids instead of pointers/indices?** Stable across deletes and trivially stored in a database;
   never reused, so stale links cannot silently change meaning.
6. **What is RAII?** Resources are owned by objects; destructors release them. `Database`, `Statement` and
   `Transaction` do this for SQLite.
7. **What is polymorphism here?** `IStoryStorage` (abstract) with `SqliteStoryRepository` (concrete).
8. **How do you prevent SQL injection?** Prepared statements with bound parameters only.
9. **What happens if saving fails halfway?** The transaction rolls back; the old save is intact.
10. **What are the limits?** Items can only be gained (no consumables); analysis has state caps for huge
    stories; text is plain text.
11. **How could you extend it?** Variables/flags (e.g. trust points), consumable items, import/export files,
    a GUI front-end reusing the same model and analysis.

---------------------------------------------------------------------------------------------------

## 9. Making the `.exe` and putting it on the pendrive

You build the exe **once** on any Windows PC (it needs internet the first time to fetch SQLite):

1. Download **w64devkit** (a portable C++ compiler, no installer):
   <https://github.com/skeeto/w64devkit/releases> -> get the `.zip` (or `.7z.exe`) -> extract anywhere.
2. Double-click `w64devkit.exe` inside it (a terminal opens). Type
   `cd /d D:\path\to\StoryLang` (or drag the folder in) and run `build_windows.bat`.
   *(It downloads SQLite automatically; if that fails it tells you the 4 manual steps.)*
3. When it prints **SUCCESS**, you will have **`StoryLang.exe`** in the folder. It is fully standalone: no DLLs,
   no installer, no admin rights.
4. Copy `StoryLang.exe` to the pendrive. Double-click it. Saved stories go into a `data` folder next to it.

**Tips for the presentation PC**
* Windows Terminal or the normal black console both work. If a symbol looks wrong, run `StoryLang.exe --classic`
  or `StoryLang.exe --plain`.
* If Windows SmartScreen says "unknown publisher": *More info -> Run anyway*. (Unsigned programs always show it.)
* Some school PCs block running programs from USB. Bring a backup: the whole project folder + this guide, or
  your laptop.
* Maximise the console window and use a large font for the projector. The UI is 78 columns wide.
* Before presenting, run it once on the target PC type of machine (or at least on yours from the pendrive).
