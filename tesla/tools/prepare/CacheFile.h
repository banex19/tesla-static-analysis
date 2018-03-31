#pragma once

#include "DataStructures.h"
#include "Debug.h"
#include "Utils.h"
#include <string>

#include <unordered_map>

class FileCache
{
  public:
    FileCache(const std::string& BaseDir, const std::string& cacheFilePath)
        : BaseDir(BaseDir), path(cacheFilePath)
    {
    }

    std::unordered_map<std::string, TimestampedFile> ReadCachedData(bool ignoreExisting);

    void WriteCacheData(std::unordered_map<std::string, TimestampedFile>& cached, std::vector<TimestampedFile>& uncached);

  private:
    std::string path;
    std::string BaseDir;
};

class AutomataCache
{
  public:
    AutomataCache(const std::string& BaseDir, const std::string& path)
        : BaseDir(BaseDir), path(path)
    {
    }

    std::unordered_map<std::string, std::vector<AutomatonSummary>> ReadAutomataCache(bool ignoreExisting);

    void WriteAutomataCache(std::unordered_map<std::string, std::vector<AutomatonSummary>>& cached,
                            std::map<std::pair<std::string, std::string>, std::set<std::string>>& uncached);

  private:
    std::string path;
    std::string BaseDir;
};
