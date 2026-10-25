# 5-minute demo script

1. **Launch** `./build/storylang`. Mention: C++17, SQLite, no other dependencies.
2. **Option 9** - load "Harrow Island (demo - with flaws)" (22 scenes).
3. **Option 4 - Validate.** Point out the three planted problems:
   * ERROR   dead end: *Into the Fog*
   * ERROR   broken link: Lighthouse choice #4 -> scene 99
   * WARNING unreachable: *The Forgotten Boathouse*
   and the INFO loops ("loops are legal in stories, but the author should know").
4. **Option 6 > 1 - ASCII map.** Show `!! MISSING SCENE !!`, `(DEAD END)`, and the "Not reachable" list.
5. **Option 3 - Edit** and fix live:
   * Scenes > Delete scene > pick the Boathouse > option 1 (cascade).
   * Choices > Fog scene (id 22) > add choice "Turn back" -> scene 1.
   * Choices > Lighthouse (id 11) > delete choice #4.
6. **Option 4 again** - result PASS with 0 errors / 0 warnings.
7. **Option 6 > 2** - statistics: routes per ending. **> 3** - shortest route to *The Keeper's Truth* (12 choices,
   shows the key and lantern detours).
8. **Option 7 - Save**, then **0 - Exit**. Relaunch and **Option 2 - Load**: the fixed story is still there.
9. **Option 5 - Play**: try the locked choice at Mara ("needs Torn Letter"), use `i` and `b`, reach an ending.
10. **Optional:** Option 6 > 4 exports `.dot`; render with Graphviz for a picture of the graph.

Likely questions: *why BFS for reachability?* (visits each node once, O(V+E)); *why DFS colours for loops?* (a back
edge to a grey node = cycle); *why (scene, inventory) states?* (a gated scene may be unreachable even though a
plain graph says it is reachable); *why a transaction on save?* (all-or-nothing).
