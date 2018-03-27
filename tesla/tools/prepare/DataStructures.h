#pragma once
#include <set>
#include <string>
#include <vector>

#include <tesla.pb.h>

struct TimestampedFile
{
    std::string filename;
    size_t timestamp;

    bool upToDate = false;
    bool stillExisting = false;
};

struct AutomatonSummary
{
    AutomatonSummary()
    {
    }

    AutomatonSummary(std::string& filename, std::string& id, std::set<std::string>& affectedFunctions)
        : filename(filename), id(id), affectedFunctions(affectedFunctions)
    {
    }

    std::string filename;
    std::string id;
    std::set<std::string> affectedFunctions;
};

struct CollectedData
{
    std::vector<std::string> definedFunctionNames;

    std::vector<tesla::AutomatonDescription> automatonDescriptions;
    std::vector<tesla::Usage> automatonUses;
};