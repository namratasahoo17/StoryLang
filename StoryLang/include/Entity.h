#pragma once
#include <string>

namespace storylang {

// Characters, locations and items share the same shape (id, name, description),
// so they are modelled as one Entity type distinguished by EntityKind.
enum class EntityKind { Character = 0, Location = 1, Item = 2 };

inline const char* kindName(EntityKind k) {
    switch (k) {
        case EntityKind::Character: return "Character";
        case EntityKind::Location:  return "Location";
        case EntityKind::Item:      return "Item";
    }
    return "Entity";
}

struct Entity {
    int id = 0;
    std::string name;
    std::string description;
};

}  // namespace storylang
