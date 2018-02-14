
#include "CacheFile.h"
#include "Debug.h"
#include "DataStructures.h"
#include "Utils.h"

#include <unordered_map>
#include <fstream>

using namespace llvm;
using namespace llvm::sys::fs;
using namespace tesla;

std::unordered_map<std::string, TimestampedFile> CacheFile::ReadCachedData(bool ignoreExisting)
{
    std::unordered_map<std::string, TimestampedFile> cachedFiles;

    std::ifstream file(path);

    if (!file || ignoreExisting)
    {
        if (!file && !ignoreExisting)
            OutputWarning("Cache file " + path + " not found - creating a new file");

        std::ofstream newFile(path, std::ofstream::trunc);

        if (!newFile)
            tesla::panic("Could not create cache file " + path);

        newFile.close();

        return cachedFiles;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);

        std::string sourceFile;
        size_t timestamp;
        if (!(iss >> sourceFile >> timestamp))
        {
            tesla::panic("Cache file malformed - Delete the file \"" + path + "\" and try again");
        }

        cachedFiles[sourceFile] = {sourceFile, timestamp};
    }

    file.close();

    return cachedFiles;
}

void CacheFile::WriteCacheData(std::unordered_map<std::string, TimestampedFile> &cached, std::vector<TimestampedFile> &uncached)
{
    std::ofstream file(path, std::ofstream::trunc);

    if (!file)
    {
        tesla::panic("Could not open output file " + path);
    }

    for (auto &f : cached)
    {
        file << f.second.filename << " " << f.second.timestamp << "\n";
    }

    for (auto &f : uncached)
    {
        file << f.filename << " " << f.timestamp << "\n";
    }

    file.close();
}