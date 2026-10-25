#pragma once
#include <string>
#include <vector>

#include "Story.h"

namespace storylang {

enum class Severity { Info = 0, Warning = 1, Error = 2 };

struct Issue {
    Severity severity;
    std::string message;
    int sceneId;  // 0 if not tied to one scene
};

class Validator {
public:
    explicit Validator(const Story& story) : story_(story) {}

    // Runs every check; results are sorted most severe first.
    std::vector<Issue> run() const;

    static int count(const std::vector<Issue>& issues, Severity s);
    static void printReport(const Story& story, const std::vector<Issue>& issues);

private:
    void checkStructure(std::vector<Issue>& out) const;
    void checkReferences(std::vector<Issue>& out) const;
    void checkReachability(std::vector<Issue>& out) const;
    void checkDeadEnds(std::vector<Issue>& out) const;
    void checkCycles(std::vector<Issue>& out) const;
    void checkInventory(std::vector<Issue>& out) const;

    const Story& story_;
};

}  // namespace storylang
