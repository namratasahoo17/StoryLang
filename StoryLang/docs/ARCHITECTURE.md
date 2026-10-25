# Architecture

## Class diagram
```mermaid
classDiagram
    class Story {
      -map~int,Scene~ scenes_
      -map~int,Entity~ characters_, locations_, items_
      -int startSceneId_
      +addScene() int
      +removeScene(id, cascade) int
      +addChoice(sceneId, Choice) bool
      +addEntity(kind, name, desc) int
      +removeEntity(kind, id) bool
      +incomingChoices(id)
    }
    class Scene { +id +title +text +locationId +characterIds +isEnding +choices }
    class Choice { +text +targetSceneId +requiredItemId +grantsItemId }
    class Entity { +id +name +description }
    class GameState { +current() +inventory() +apply(Choice) +undo() }
    class Player { +run() }
    class Validator { +run() vector~Issue~ }
    class PathFinder { <<static>> reachableScenes() explore() shortestPath() countPaths() }
    class Editor { +run() }
    class IStoryStorage { <<interface>> save() load() list() remove() exists() }
    class SqliteStoryRepository
    class Database
    class Statement
    class Transaction

    Story "1" *-- "many" Scene
    Scene "1" *-- "many" Choice
    Story "1" *-- "many" Entity
    Player --> GameState
    Player --> Story
    Validator --> Story
    Validator --> PathFinder
    Editor --> Story
    IStoryStorage <|-- SqliteStoryRepository
    SqliteStoryRepository *-- Database
    Database ..> Statement : creates
    Database <.. Transaction
```

## Demo story graph ("Harrow Island", clean version)
Dashed = item-gated. Endings in double circles.
```mermaid
flowchart TD
    dock([Arrival]) --> inn[Harrow Inn]
    dock --> cliff[Cliff Path]
    dock --> E1(((Unfinished Business)))
    inn --> mara[Mara's Story]
    inn --> night[Restless Night]
    inn --> chapel[Chapel]
    inn --> dock
    mara -- "gives Brass Key" --> inn
    mara -. "needs Torn Letter" .-> secret[Mara's Secret]
    secret --> light[Lighthouse]
    secret --> E3(((Hasty Accusation)))
    night --> cliff
    night --> E2(((Quiet Morning)))
    cliff --> light
    cliff --> lantern[Lantern] -- "gives Lantern" --> cliff
    chapel --> conf[Confessional] -- "gives Torn Letter" --> chapel
    chapel --> elias[Father Elias] --> chapel
    light --> lamp[Lamp Room] --> light
    light -. "needs Brass Key" .-> cellar[Cellar]
    cellar -. "needs Lantern" .-> tunnel[Tunnel] --> tomas[Old Tomas]
    tomas --> E4(((Keeper's Truth)))
    tomas --> E5(((Secret Kept)))
```

## Data flow
`main.cpp (App)` -> `Editor` / `Player` / `Validator` / `PathFinder` operate on one in-memory `Story`;
`IStoryStorage` converts that `Story` to/from SQLite rows inside a transaction.
