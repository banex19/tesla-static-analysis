#include "Debug.h"
#include <iostream>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include "Utils.h"

using namespace llvm;
using namespace llvm::sys::fs;
using namespace llvm::sys::path;

void PanicIfError(std::error_code err)
{
    if (err)
        tesla::panic(err.message());
}

void OutputVerbose(const std::string& msg, bool verbose)
{
    if (verbose)
    {
        llvm::outs() << msg << "\n";
    }
}

void OutputWarning(const std::string& warning)
{
    llvm::errs() << "WARNING: " << warning << "\n";
}

void OutputAlways(const std::string& msg)
{
    llvm::outs() << msg << "\n";
}

std::string StringFromVector(const std::vector<const char*>& vec, const std::string& separator)
{
    std::vector<std::string> strVector;
    for (auto& s : vec)
    {
        strVector.push_back(s);
    }

    return StringFromVector(strVector, separator);
}

std::string StringFromVector(const std::vector<std::string>& vec, const std::string& separator)
{
    std::string str;

    for (size_t i = 0; i < vec.size(); ++i)
    {
        str += vec[i];

        if (i != vec.size() - 1)
            str += separator;
    }

    return str;
}

std::string StringFromSet(const std::set<std::string>& s, const std::string& separator)
{
    std::string str;

    size_t i = 0;

    for (auto& elem : s)
    {
        str += elem;

        if (i != s.size() - 1)
            str += separator;

        ++i;
    }

    return str;
}

std::string SanitizeFilename(const std::string& filename)
{
    std::string newFilename = filename;
    std::replace(newFilename.begin(), newFilename.end(), '.', '-');
    std::replace(newFilename.begin(), newFilename.end(), '/', '_');
    std::replace(newFilename.begin(), newFilename.end(), '\\', '_');

    return newFilename;
}

std::string GetFullPath(const std::string& OutputDir, const std::string& filename)
{
    return OutputDir + filename;
}

std::string GetRelativePath(const std::string& OutputDir, const std::string& filename)
{
    std::string rel = filename;
    return rel.erase(0, OutputDir.size());
}

void GetAllRecursiveFolders(const std::string& current, std::vector<std::string>& folders)
{
    folders.push_back(current);

    std::error_code err;
    directory_iterator it(current, err);

    PanicIfError(err);

    while (it != directory_iterator())
    {
        PanicIfError(err);

        std::string path = it->path();

        if (is_directory(path))
        {
            GetAllRecursiveFolders(path, folders);
        }

        it.increment(err);
    }
}

std::vector<std::string> GetAllRecursiveFolders(const std::string& base)
{
    std::vector<std::string> folders;

    if (is_directory(base))
    {
        GetAllRecursiveFolders(base, folders);
    }

    return folders;
}

std::vector<std::string> GetToolCommandLineOptions(int argc, const char** argv)
{
    std::vector<std::string> opts;

    for (size_t i = 0; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--")
            break;

        opts.push_back(argv[i]);
    }

    return opts;
}

std::vector<std::string> GetCompilationOptions(int argc, const char** argv)
{
    bool foundSeparator = false;
    std::vector<std::string> opts;

    for (size_t i = 0; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--")
        {
            foundSeparator = true;
        }

        if (!foundSeparator)
            continue;

        opts.push_back(argv[i]);
    }

    return opts;
}