#pragma once
#include <string>
#include <vector>

struct TimestampedFile
{
    std::string filename;
    size_t timestamp;

    bool upToDate = false;
};

struct CollectedData
{
    std::vector<std::string> definedFunctionNames;
};