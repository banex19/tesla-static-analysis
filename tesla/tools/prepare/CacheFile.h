#pragma once

#include <string>
#include "Debug.h"
#include "DataStructures.h"

#include <unordered_map>

class CacheFile
{
  public:
    CacheFile(const std::string &cacheFilePath)
        : path(cacheFilePath)
    {
        if (cacheFilePath == "")
        {
            tesla::panic("Cache filename not specified. Specify a cache file with -o");
        }
    }

    std::unordered_map<std::string, TimestampedFile> ReadCachedData(bool ignoreExisting);

    void WriteCacheData(std::unordered_map<std::string, TimestampedFile> &cached, std::vector<TimestampedFile> &uncached);

  private:
    std::string path;
};