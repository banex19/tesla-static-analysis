#pragma once

#include "DataStructures.h"
#include "Debug.h"
#include "Utils.h"
#include <string>

#include <unordered_map>

class FileCache
{
  public:
    FileCache(const std::string& cacheFilePath)
        : path(cacheFilePath)
    {
    }

    std::unordered_map<std::string, TimestampedFile> ReadCachedData(bool ignoreExisting);

    void WriteCacheData(std::unordered_map<std::string, TimestampedFile>& cached, std::vector<TimestampedFile>& uncached);

  private:
    std::string path;
};

class AutomataCache
{
  public:
    AutomataCache(const std::string& path)
        : path(path)
    {
    }

    std::unordered_map<std::string, std::vector<AutomatonSummary>> ReadAutomata(bool ignoreExisting);

    void WriteAutomata(std::unordered_map<std::string, std::vector<AutomatonSummary>>& cached,
                       std::map<std::pair<std::string, std::string>, std::set<std::string>>& uncached);

  private:
    std::string path;
};
