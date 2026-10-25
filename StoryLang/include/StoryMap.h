#pragma once
#include <string>

#include "Story.h"

namespace storylang {

// Indented ASCII tree of the story, starting at the start scene.
std::string asciiMap(const Story& story);

// Graphviz source: render with `dot -Tpng story.dot -o story.png`.
std::string toDot(const Story& story);

}  // namespace storylang
