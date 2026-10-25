#pragma once
#include "Story.h"

namespace storylang {

// "Harrow Island": a 20-scene mystery with 5 endings and 3 items.
// withFlaws = true plants three deliberate defects for the validator demo:
//   1) an unreachable scene, 2) a non-ending dead end, 3) a link to a missing scene.
Story makeDemoStory(bool withFlaws);

}  // namespace storylang
