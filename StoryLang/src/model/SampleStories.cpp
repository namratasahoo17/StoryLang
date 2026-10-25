#include "SampleStories.h"

#include <vector>

namespace storylang {

Story makeDemoStory(bool withFlaws) {
    Story s(withFlaws ? "Harrow Island (demo - with flaws)" : "Harrow Island (demo)");
    const EntityKind CH = EntityKind::Character, LO = EntityKind::Location, IT = EntityKind::Item;

    // ---- locations, characters, items -------------------------------------
    int locDock = s.addEntity(LO, "Ferry Dock", "A weathered pier slick with sea spray.");
    int locInn = s.addEntity(LO, "Harrow Inn", "The only warm light on the island.");
    int locCliff = s.addEntity(LO, "Cliff Path", "A narrow track above the crashing sea.");
    int locChapel = s.addEntity(LO, "Village Chapel", "Salt-stained stone and cold candles.");
    int locLight = s.addEntity(LO, "Lighthouse", "Its lamp has been dark for three nights.");
    int locCellar = s.addEntity(LO, "Lighthouse Cellar", "Damp, silent and older than the tower.");

    int chMara = s.addEntity(CH, "Mara Voss", "The innkeeper. Friendly, but watches the door.");
    int chElias = s.addEntity(CH, "Father Elias", "The island priest, softly spoken.");
    int chTomas = s.addEntity(CH, "Old Tomas", "The missing lighthouse keeper.");

    int itKey = s.addEntity(IT, "Brass Key", "Heavy, green with age. Stamped with a tiny lighthouse.");
    int itLetter = s.addEntity(IT, "Torn Letter", "Half a letter, signed with the letter 'M'.");
    int itLantern = s.addEntity(IT, "Lantern", "A storm lantern with oil to spare.");

    auto scene = [&](const std::string& title, const std::string& text, int loc,
                     std::vector<int> chars, bool ending) {
        int id = s.addScene(title, text, ending);
        Scene* sc = s.findScene(id);
        sc->locationId = loc;
        sc->characterIds = chars;
        return id;
    };
    auto link = [&](int from, const std::string& text, int to, int req = 0, int grant = 0) {
        s.addChoice(from, Choice{text, to, req, grant});
    };

    // ---- scenes ---------------------------------------------------------------
    int dock = scene("Arrival at Harrow Island",
        "The last ferry of the day groans against the pier. You are a retired detective with "
        "one letter in your pocket: Old Tomas, keeper of the island lighthouse, has not been "
        "seen for three nights, and the lamp has gone dark.\n"
        "Fog is rolling in. The ferryman checks his watch and tells you he leaves in ten minutes.",
        locDock, {}, false);
    int inn = scene("The Harrow Inn",
        "Warm light, a crackling fire and a room that goes quiet when you enter. Mara Voss, the "
        "innkeeper, polishes a glass without looking at it.",
        locInn, {chMara}, false);
    int mara = scene("Mara's Story",
        "Mara lowers her voice. 'Tomas was afraid of something. He left a key with me for safe "
        "keeping, but I never liked holding it.' She keeps glancing at the cellar door.",
        locInn, {chMara}, false);
    int maraSecret = scene("Mara's Secret",
        "You lay the torn letter on the bar. Mara goes white. 'Those initials... they are mine. "
        "I wrote that to Tomas years ago. If anyone finds the rest of it, they will think I "
        "had a hand in his disappearance.'",
        locInn, {chMara}, false);
    int night = scene("A Restless Night",
        "You take the room above the bar. Near midnight, heavy footsteps cross the yard and "
        "someone fumbles at the gate. The wind howls; the footsteps stop beneath your window.",
        locInn, {}, false);
    int cliff = scene("The Cliff Path",
        "The path hugs the cliff edge. Below, the sea is a black roar. Something glints beside "
        "the railing, and far ahead the dead lighthouse waits.",
        locCliff, {}, false);
    int lantern = scene("An Abandoned Lantern",
        "A storm lantern lies by the rail, still half full of oil. Someone dropped it in a "
        "hurry, and not long ago.",
        locCliff, {}, false);
    int chapel = scene("The Village Chapel",
        "Candles gutter in the draught. Father Elias folds his hands. 'Tomas confessed to me "
        "last week. I cannot repeat it, but... the confessional is rarely locked.'",
        locChapel, {chElias}, false);
    int elias = scene("Father Elias Speaks",
        "'Tomas feared the cellar beneath his own tower,' the priest murmurs. 'He said you "
        "would need a light, and a key, and a reason to trust the wrong person.'",
        locChapel, {chElias}, false);
    int confessional = scene("The Confessional",
        "Behind the wooden grille, wedged into the frame, you find half of a letter. The rest "
        "has been torn away. The signature ends in a looping 'M'.",
        locChapel, {}, false);
    int lighthouse = scene("The Lighthouse",
        "The tower looms overhead. The door hangs open. Inside, a spiral staircase climbs into "
        "darkness and a heavy cellar door is padlocked beside it.",
        locLight, {}, false);
    int lamp = scene("The Lamp Room",
        "The great lamp is cold. On the logbook table lies an entry from three nights ago: "
        "'M. came again. I will not give it to her. If anything happens, look below.'",
        locLight, {}, false);
    int cellar = scene("The Cellar Door",
        "The key turns with a groan. Beyond the door, stone steps drop into a low tunnel that "
        "swallows the light of the stairwell.",
        locCellar, {}, false);
    int tunnel = scene("The Smugglers' Tunnel",
        "By lantern light you follow the tunnel until it opens into a small chamber. Old Tomas "
        "sits bound against the wall, alive, blinking at the flame.",
        locCellar, {chTomas}, false);
    int tomas = scene("Old Tomas",
        "'Mara has been running contraband through my tower for years,' Tomas croaks. 'I "
        "threatened to talk. She locked me down here and waited for the fog.' He looks at you. "
        "'What happens now is your choice.'",
        locCellar, {chTomas}, false);

    int endLeave = scene("Ending: Unfinished Business",
        "You decide the island can keep its secrets. The ferry carries you away through the "
        "fog. In the morning, the lighthouse lamp is still dark, and you never learn why.",
        locDock, {}, true);
    int endSleep = scene("Ending: The Quiet Morning",
        "You pull the blanket over your head. By morning the footsteps are gone, the island is "
        "silent, and a note has been pushed under your door: 'Leave on the next ferry.' You do.",
        locInn, {}, true);
    int endAccuse = scene("Ending: A Hasty Accusation",
        "You accuse Mara in front of the whole inn. Without proof, the room turns on you. By "
        "dawn you are on the ferry again and the truth stays buried below the lighthouse.",
        locInn, {chMara}, true);
    int endTruth = scene("Ending: The Keeper's Truth",
        "You and Tomas walk into the harbour at dawn. Constable Reed takes his statement and "
        "Mara's smuggling ring is dismantled. The lamp is lit again that night.",
        locDock, {chTomas}, true);
    int endHush = scene("Ending: A Secret Kept",
        "Tomas begs for silence, and you give it. The lamp is relit, Mara keeps her inn, and "
        "the island stays quiet. You carry the truth home alone.",
        locLight, {chTomas}, true);

    // ---- choices (the graph edges) ----------------------------------------
    link(dock, "Follow the lamplit road to the inn", inn);
    link(dock, "Climb the cliff path toward the dark lighthouse", cliff);
    link(dock, "Re-board the ferry and leave", endLeave);

    link(inn, "Ask Mara about the missing keeper", mara);
    link(inn, "Take a room for the night", night);
    link(inn, "Walk to the village chapel", chapel);
    link(inn, "Head back to the dock", dock);

    link(mara, "Accept the brass key she slips you", inn, 0, itKey);
    link(mara, "Show her the torn letter", maraSecret, itLetter);
    link(mara, "Change the subject", inn);

    link(maraSecret, "Ask her to take you to the lighthouse", lighthouse);
    link(maraSecret, "Accuse her in front of the whole inn", endAccuse);

    link(night, "Follow the footsteps into the night", cliff);
    link(night, "Pull the blanket over your head", endSleep);

    link(cliff, "Continue to the lighthouse", lighthouse);
    link(cliff, "Inspect the glint by the railing", lantern);
    link(cliff, "Return to the ferry dock", dock);

    link(lantern, "Pocket the lantern and rejoin the path", cliff, 0, itLantern);

    link(chapel, "Search the confessional", confessional);
    link(chapel, "Speak with Father Elias", elias);
    link(chapel, "Leave the chapel", inn);

    link(elias, "Thank him and step back", chapel);
    link(confessional, "Take the torn letter", chapel, 0, itLetter);

    link(lighthouse, "Climb the spiral stairs", lamp);
    link(lighthouse, "Unlock the cellar door", cellar, itKey);
    link(lighthouse, "Walk back down the cliff path", cliff);

    link(lamp, "Go back down", lighthouse);

    link(cellar, "Follow the tunnel into the dark", tunnel, itLantern);
    link(cellar, "Climb back up", lighthouse);

    link(tunnel, "Untie Old Tomas", tomas);

    link(tomas, "Bring him to Constable Reed", endTruth);
    link(tomas, "Promise to keep his secret", endHush);

    s.setStartSceneId(dock);

    // ---- three deliberate flaws for the validator demo ---------------------
    if (withFlaws) {
        // 1. unreachable scene (nothing links to it)
        int boathouse = scene("The Forgotten Boathouse",
            "A rotting boathouse nobody has visited in years. You cannot remember how you got here.",
            locDock, {}, false);
        link(boathouse, "Walk back to the dock", dock);

        // 2. non-ending dead end (reachable, but no choices)
        int fog = scene("Into the Fog",
            "You wander into the fog. The pier vanishes behind you. There is only grey.",
            locDock, {}, false);
        link(dock, "Wander into the fog", fog);

        // 3. a choice pointing at a scene that does not exist
        link(lighthouse, "Listen at the cellar window", 99);
    }
    return s;
}

}  // namespace storylang
