#pragma once

#include "DataStructures.h"
#include "Debug.h"
#include <string>
#include "Utils.h"

#include <unordered_map>

class CacheFile
{
  public:
    CacheFile(const std::string& cacheFilePath)
        : path(cacheFilePath)
    {
        if (cacheFilePath == "")
        {
            tesla::panic("Cache filename not specified. Specify a cache file with -o");
        }
    }

    std::unordered_map<std::string, TimestampedFile> ReadCachedData(bool ignoreExisting);

    void WriteCacheData(std::unordered_map<std::string, TimestampedFile>& cached, std::vector<TimestampedFile>& uncached);

  private:
    std::string path;
};

class AutomataFile
{
  public:
    AutomataFile(const std::string& path)
        : path(path)
    {
    }

    std::unordered_map<std::string, std::vector<AutomatonSummary>> ReadAutomata(bool ignoreExisting);

    void WriteAutomata(std::unordered_map<std::string, std::vector<AutomatonSummary>>& cached, 
                 std::map<std::pair<std::string, std::string>, std::set<std::string>>& uncached);

  private:
    std::string path;
};