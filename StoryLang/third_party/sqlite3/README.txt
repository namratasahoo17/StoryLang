OPTIONAL: bundle SQLite so nobody has to install it.

1. Download the "amalgamation" zip from https://www.sqlite.org/download.html
   (file name like sqlite-amalgamation-3450100.zip).
2. Copy  sqlite3.c  and  sqlite3.h  into THIS folder.
3. Build as usual (make / cmake). The build detects sqlite3.c automatically and
   compiles it in; no -lsqlite3 and no system package needed.

If this folder has no sqlite3.c, the build links the system SQLite instead
(Ubuntu/Debian: sudo apt install libsqlite3-dev | Fedora: sqlite-devel |
macOS: preinstalled, or brew install sqlite | Windows/MSYS2: pacman -S mingw-w64-x86_64-sqlite3).
