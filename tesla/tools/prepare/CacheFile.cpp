
#include "CacheFile.h"
#include "DataStructures.h"
#include "Debug.h"
#include "Utils.h"

#include <fstream>
#include <unordered_map>

using namespace llvm;
using namespace llvm::sys::fs;
using namespace tesla;

std::unordered_map<std::string, TimestampedFile> FileCache::ReadCachedData(bool ignoreExisting)
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

        std::set<std::string> functions;

        std::string sourceFile;
        size_t timestamp;
        if (!(iss >> sourceFile >> timestamp))
        {
            tesla::panic("Cache file malformed - Delete the file \"" + path + "\" and try again");
        }

        std::string fn;
        while (iss >> fn)
        {
            functions.insert(fn);
        }

        sourceFile = GetFullPath(BaseDir, sourceFile);
        std::string absolutePath = GetRealPath(sourceFile);

        cachedFiles[sourceFile] = {sourceFile, absolutePath, timestamp, false, false, functions};
    }

    file.close();

    return cachedFiles;
}

void FileCache::WriteCacheData(std::unordered_map<std::string, TimestampedFile>& cached, std::vector<TimestampedFile>& uncached)
{
    std::ofstream file(path, std::ofstream::trunc);

    if (!file)
    {
        tesla::panic("Could not open cache output file " + path + " for writing");
    }

    for (auto& f : cached)
    {
        file << GetRelativePath(BaseDir, f.second.filename) << " " << f.second.timestamp << " " << StringFromSet(f.second.functions, " ") << " \n";
    }

    for (auto& f : uncached)
    {
        file << GetRelativePath(BaseDir, f.filename) << " " << f.timestamp << " " << StringFromSet(f.functions, " ") << " \n";
    }

    file.close();
}

std::unordered_map<std::string, std::vector<AutomatonSummary>> AutomataCache::ReadAutomataCache(bool ignoreExisting)
{
    std::unordered_map<std::string, std::vector<AutomatonSummary>> cached;

    std::ifstream file(path);

    if (!file || ignoreExisting)
    {
        if (!file && !ignoreExisting)
            OutputWarning("Automata cache file " + path + " not found - creating a new file");

        std::ofstream newFile(path, std::ofstream::trunc);

        if (!newFile)
            tesla::panic("Could not create automata cache file " + path);

        newFile.close();

        return cached;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::set<std::string> affectedFunctions;

        std::istringstream iss(line);

        std::string sourceFile;
        std::string id;
        if (!(iss >> sourceFile >> id))
        {
            tesla::panic("Automata cache file malformed - Delete the file \"" + path + "\" and try again");
        }

        std::string fn;
        while (iss >> fn)
        {
            affectedFunctions.insert(fn);
        }

        sourceFile = GetFullPath(BaseDir, sourceFile);

        cached[sourceFile].push_back(AutomatonSummary(sourceFile, id, affectedFunctions));
    }

    file.close();

    return cached;
}

void AutomataCache::WriteAutomataCache(std::unordered_map<std::string, std::vector<AutomatonSummary>>& cached,
                                       std::map<std::pair<std::string, std::string>, std::set<std::string>>& uncached)
{
    std::ofstream file(path, std::ofstream::trunc);

    if (!file)
    {
        tesla::panic("Could not open automata cache output file " + path + " for writing");
    }

    for (auto& f : cached)
    {
        for (auto& a : f.second)
        {
            file << GetRelativePath(BaseDir, a.filename) << " " << a.id << " " << StringFromSet(a.affectedFunctions, " ") << " \n";
        }
    }

    for (auto& f : uncached)
    {
        file << GetRelativePath(BaseDir, f.first.first) << " " << f.first.second << " " << StringFromSet(f.second, " ") << " \n";
    }

    file.close();
}
