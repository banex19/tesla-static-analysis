/*
 * Copyright (c) 2012-2013 Jonathan Anderson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>

#include "AutomatonParser.h"
#include "CacheFile.h"
#include "Utils.h"

#include "DataStructures.h"
#include "Debug.h"
#include "PrepareAST.h"

#include "CompilationDatabase.h"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/PrettyStackTrace.h>

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;
using namespace llvm::sys::fs;
using namespace llvm::sys::path;
using namespace tesla;

enum OnFileNotInDatabase
{
    NOT_SPECIFIED = 0,
    ERROR = 1,
    SKIP = 2,
    COMPILE_WITH_DEFAULT = 3
};

cl::opt<std::string>
    SourceRoot(
        "s",
        cl::desc("<source root dir>"),
        cl::Required);

cl::opt<std::string> OutputDir(
    "o",
    cl::desc("<output directory>"),
    cl::Required);

cl::list<std::string> RecursiveIncludes(
    "I",
    cl::desc("Extra include directories (and all their subfolders recursively)"));

cl::opt<std::string> CompilationDatabaseFile(
    "database",
    cl::desc("A file containing a compilation database in the format \"<filename> -- <compilation options>\""));

cl::opt<OnFileNotInDatabase> NotInDatabasePolicy(
    "not-in-database",
    cl::desc("What to do if a file is not contained in the compilation database"),
    cl::values(
        clEnumVal(ERROR, "Error and exit"),
        clEnumVal(SKIP, "Skip file"),
        clEnumVal(COMPILE_WITH_DEFAULT, "Compile with default compilation options (default)")),
    cl::init(OnFileNotInDatabase::NOT_SPECIFIED));

cl::opt<bool> Verbose(
    "v",
    cl::desc("Enable verbosity"),
    cl::init(false));

cl::opt<bool> OverwriteCache(
    "x",
    cl::desc("Overwrite the current cache and build a new one from scratch"),
    cl::init(false));

cl::opt<bool> TouchAll(
    "t",
    cl::desc("Update all files relying on automata instrumentation (both updated and old automata)"),
    cl::init(false));

cl::opt<bool> ThinTeslaSpecific(
    "thin-tesla",
    cl::desc("Use ThinTESLA-specific representation"),
    cl::init(false));

cl::list<std::string> ExtraExtensions(
    "a",
    cl::desc("Extra extensions to consider"));

cl::list<std::string> ExtraExceptions(
    "e",
    cl::desc("Substrings that will cause a directory or file to be ignored"));

std::vector<std::string> exceptions = {"cmake"};
std::vector<std::string> extensions = {".cpp", ".c", ".cc"};

TeslaCompilationDatabase database;

void AddOrMerge(std::map<std::pair<std::string, std::string>, std::set<std::string>>& automatonFunctions, std::string filename,
                std::string id, std::set<std::string> newFunctions)
{
    std::pair<std::string, std::string> key = std::make_pair(filename, id);

    if (automatonFunctions.find(key) == automatonFunctions.end())
    {
        automatonFunctions[key] = newFunctions;
    }
    else
    {
        auto& oldSet = automatonFunctions[key];
        newFunctions.insert(oldSet.begin(), oldSet.end());
        automatonFunctions[key] = newFunctions;
    }
}

void TouchFile(TimestampedFile& file)
{
    timespec currentTime[2];
    clock_gettime(CLOCK_REALTIME, currentTime);
    currentTime[1] = currentTime[0];

    utimensat(AT_FDCWD, file.filename.c_str(), currentTime, 0);

    file.timestamp = (size_t)currentTime[0].tv_sec * (1e9) + currentTime[0].tv_nsec;
}

void TraverseSourcesRec(const std::string& sourceRoot, std::unordered_map<std::string, TimestampedFile>& cached, std::vector<TimestampedFile>& uncached)
{
    std::error_code err;
    directory_iterator it(sourceRoot, err);

    PanicIfError(err);

    while (it != directory_iterator())
    {
        PanicIfError(err);

        std::string path = it->path();
        std::string fullPath = GetRealPath(path);

        std::string lowercasePath = path;
        std::transform(lowercasePath.begin(), lowercasePath.end(), lowercasePath.begin(), ::tolower);

        bool acceptable = true;
        for (auto& exception : exceptions)
        {
            if (lowercasePath.find(exception) != std::string::npos)
            {
                acceptable = false;
                break;
            }
        }

        if (acceptable)
        {
            if (is_directory(path)) // A directory, go deeper.
            {
                TraverseSourcesRec(path, cached, uncached);
            }
            else // A file.
            {
                if (std::find(extensions.begin(), extensions.end(), extension(path)) != extensions.end()) // A source file we need to consider.
                {
                    auto status = it->status();
                    PanicIfError(status.getError());

                    size_t timestamp = status->getLastModificationTime().time_since_epoch().count();

                    if (cached.find(path) != cached.end() && cached[path].timestamp >= timestamp)
                    {
                        //    OutputVerbose("Found file " + path + " which is already cached and up-to-date", Verbose);
                        cached[path].upToDate = true;
                        cached[path].stillExisting = true;
                        cached[path].fullPath = fullPath;
                    }
                    else
                    {
                        if (cached.find(path) != cached.end())
                            cached[path].stillExisting = true;

                        uncached.push_back({path, fullPath, timestamp, false, true});
                        OutputVerbose("Found file " + path + " with timestamp " + std::to_string(timestamp) + " which is not cached and will be updated", Verbose);
                    }
                }
            }
        }

        it.increment(err);
    }
}

std::vector<TimestampedFile> TraverseSources(const std::string& sourceRoot, std::unordered_map<std::string, TimestampedFile>& cached)
{
    if (is_absolute(sourceRoot))
        OutputWarning("Source root " + sourceRoot + " is absolute - a relative path is suggested instead");

    std::vector<TimestampedFile> uncached;
    TraverseSourcesRec(sourceRoot, cached, uncached);
    return uncached;
}

void AnalyseFile(const std::string& filename, const std::string& fullPath, const std::vector<std::string>& compilationOptions, CollectedData& data)
{
    std::unique_ptr<TeslaActionFactory> Factory(new TeslaActionFactory(data, OutputDir + SanitizeFilename("TESLA_" + filename)));

    if (!database.IsEmpty() && database.IsFileInDatabase(fullPath))
    {
        OutputVerbose("File " + fullPath + " in compilation database", Verbose);

        auto fileCompilationOptions = database.GetCompilationOptions(fullPath);

        // Skip the "--", to avoid having two of them.
        auto compOptionsBegin = compilationOptions.begin();
        std::advance(compOptionsBegin, 1);

        fileCompilationOptions.insert(fileCompilationOptions.end(), compOptionsBegin, compilationOptions.end());

        std::vector<const char*> constCharCompilationOptions;
        for (auto& opt : fileCompilationOptions)
        {
            constCharCompilationOptions.push_back(opt.c_str());
        }

        int compOptionsSize = constCharCompilationOptions.size();
        assert(compOptionsSize == constCharCompilationOptions.size()); // Check for overflow.

        //  OutputAlways("Compilation options: " + StringFromVector(constCharCompilationOptions, " "));

        std::string errorMsg;
        std::unique_ptr<CompilationDatabase> Compilations(
            FixedCompilationDatabase::loadFromCommandLine(compOptionsSize, constCharCompilationOptions.data(), errorMsg));

        if (!Compilations)
            panic(
                "Error in compilation options");

        ClangTool Tool(*Compilations, std::vector<std::string>{fullPath});

        Tool.run(Factory.get());
    }
    else
    {
        if (!database.IsEmpty())
        {
            if (NotInDatabasePolicy == ERROR)
            {
                tesla::panic("File " + filename + " is not in compilation database");
            }
            else
                OutputWarning("File " + filename + " is not in compilation database");
        }

        if (NotInDatabasePolicy != SKIP)
        {
            std::vector<const char*> constCharCompilationOptions;
            for (auto& opt : compilationOptions)
            {
                constCharCompilationOptions.push_back(opt.c_str());
            }

            int compOptionsSize = constCharCompilationOptions.size();
            assert(compOptionsSize == constCharCompilationOptions.size()); // Check for overflow.

            std::string errorMsg;
            std::unique_ptr<CompilationDatabase> Compilations(
                FixedCompilationDatabase::loadFromCommandLine(compOptionsSize, constCharCompilationOptions.data(), errorMsg));

            ClangTool Tool(*Compilations, std::vector<std::string>{filename});

            Tool.run(Factory.get());
        }
    }
}

int main(int argc, const char** argv)
{
    llvm::PrettyStackTraceProgram X(argc, argv);

    // Parse tool options (ignoring compilation options passed to Clang).
    auto toolOptions = GetToolCommandLineOptions(argc, argv);
    std::vector<const char*> constCharOptions;
    for (auto& opt : toolOptions)
    {
        constCharOptions.push_back(opt.c_str());
    }

    cl::ParseCommandLineOptions(constCharOptions.size(), constCharOptions.data());

    // Add a preprocessor definition to indicate we're doing TESLA parsing.
    std::vector<std::string> compilationOptions = GetCompilationOptions(argc, argv);
    compilationOptions.push_back("-D");
    compilationOptions.push_back("__TESLA_ANALYSER__");

    // Add recursive include directories.
    std::vector<std::string> additionalIncludes;
    for (auto& additionalIncludeDir : RecursiveIncludes)
    {
        auto folders = GetAllRecursiveFolders(additionalIncludeDir);
        additionalIncludes.insert(additionalIncludes.end(), folders.begin(), folders.end());
    }

    // OutputAlways("Additional includes: " + StringFromVector(additionalIncludes, " - ") + "\n");

    for (auto& additionalInclude : additionalIncludes)
    {
        compilationOptions.push_back("-I");
        compilationOptions.push_back(additionalInclude.c_str());
    }

    if (CompilationDatabaseFile != "")
    {
        database = TeslaCompilationDatabase(CompilationDatabaseFile);

        if (NotInDatabasePolicy == NOT_SPECIFIED)
        {
            NotInDatabasePolicy = COMPILE_WITH_DEFAULT;
        }
    }
    else if (NotInDatabasePolicy != NOT_SPECIFIED)
    {
        tesla::panic("Compilation database not specified");
    }

    if (!is_directory(OutputDir))
    {
        panic("Output path is not a directory");
    }

    OutputDir += "/";

    std::string CacheFilename = OutputDir + SanitizeFilename("filecache");
    std::string AutomataCacheFilename = OutputDir + SanitizeFilename("automatacache");
    std::string AutomataFilename = OutputDir + "automata.tesla";

    for (auto& ext : ExtraExtensions)
        extensions.push_back(ext);

    OutputAlways("Considering files with the following extensions: " + StringFromVector(extensions));

    for (auto& exc : ExtraExceptions)
        exceptions.push_back(exc);

    OutputAlways("The following substrings will be ignored if encountered: " + StringFromVector(exceptions));

    FileCache cache{SourceRoot, CacheFilename};
    AutomataCache automataCache{SourceRoot, AutomataCacheFilename};
    ManifestFile result;

    auto cachedAutomata = automataCache.ReadAutomataCache(OverwriteCache);

    auto cachedFiles = cache.ReadCachedData(OverwriteCache);
    auto uncachedFiles = TraverseSources(SourceRoot, cachedFiles);

    std::unordered_map<std::string, TimestampedFile> alreadyUpToDate;
    std::unordered_map<std::string, TimestampedFile> removedFiles;
    for (auto cached : cachedFiles)
    {
        if (!cached.second.stillExisting)
        {
            removedFiles[cached.first] = cached.second;
        }
        else if (cached.second.upToDate)
        {
            alreadyUpToDate[cached.first] = cached.second;
        }
    }

    std::map<std::pair<std::string, std::string>, std::set<std::string>> automatonFunctions;
    std::unordered_map<std::string, bool> uncachedFilenames;

    for (auto& uncached : uncachedFiles)
    {
        auto filename = uncached.filename;
        auto fullPath = uncached.fullPath;

        uncachedFilenames[filename] = true;

        CollectedData data{result};

        AnalyseFile(filename, fullPath, compilationOptions, data);

        // Cache function names.
        uncached.functions.insert(data.definedFunctionNames.begin(), data.definedFunctionNames.end());

        // Cache automata and affected functions.
        for (auto& automaton : data.automatonDescriptions)
        {
            std::string id = AutomatonParser::GetStringIdentifier(automaton, filename);
            auto affectedFunctions = AutomatonParser::GetAffectedFunctions(automaton);

            AddOrMerge(automatonFunctions, filename, id, affectedFunctions);
        }

        for (auto& automaton : data.automatonUses)
        {
            std::string id = AutomatonParser::GetStringIdentifier(automaton, filename);
            auto affectedFunctions = AutomatonParser::GetAffectedFunctions(automaton);

            AddOrMerge(automatonFunctions, filename, id, affectedFunctions);
        }
    }

    // Make sure to remove from the cache those automata that have been either updated or removed.
    std::unordered_map<std::string, std::vector<AutomatonSummary>> automataStillExisting;
    std::unordered_map<std::string, std::vector<AutomatonSummary>> removedAutomata;
    for (auto& cached : cachedAutomata)
    {
        if (removedFiles.find(cached.first) == removedFiles.end() && uncachedFilenames.find(cached.first) == uncachedFilenames.end())
            automataStillExisting[cached.first] = cached.second;
        else
            removedAutomata[cached.first] = cached.second;
    }

    // These are all the automata that have been updated or removed.
    std::vector<AutomatonSummary> updatedAutomata;
    for (auto& a : automatonFunctions)
    {
        updatedAutomata.push_back(AutomatonSummary(a.first.first, a.first.second, a.second));
    }
    for (auto& a : removedAutomata)
    {
        updatedAutomata.insert(updatedAutomata.end(), a.second.begin(), a.second.end());
    }

    // Consider old automata as updated automata as well.
    if (TouchAll)
    {
        for (auto& a : automataStillExisting)
        {
            updatedAutomata.insert(updatedAutomata.end(), a.second.begin(), a.second.end());
        }
    }

    // Touch all files that contain either removed automata or new/updated automata.
    for (auto& a : updatedAutomata)
    {
        for (auto& fn : a.affectedFunctions)
        {
            for (auto& file : alreadyUpToDate)
            {
                if (file.second.functions.find(fn) != file.second.functions.end())
                {
                    OutputVerbose("Found cached file (" + file.first + ") affected by automaton " + a.id, Verbose);
                    TouchFile(file.second);
                    break;
                }
            }
        }
    }

    // Read all automata (that have not been updated or removed).
    for (auto& a : automataStillExisting)
    {
        CollectedData data{result};

        AnalyseFile(a.first, GetRealPath(a.first), compilationOptions, data);
    }

    // Output automata.
    size_t id = 0;
    for (auto& a : *result.mutable_root())
    {
        a.set_uniqueid(id);
        id++;
    }

    std::string protobufText;

    result.SerializeToString(&protobufText);

    std::ofstream file(AutomataFilename, std::ofstream::trunc);

    if (!file)
    {
        tesla::panic("Could not open automata output file " + AutomataFilename + " for writing");
    }

    file << protobufText;

    file.close();

    std::ofstream readableFile(AutomataFilename + ".human", std::ofstream::trunc);

    if (!readableFile)
    {
        tesla::panic("Could not open automata output file " + AutomataFilename + " for writing");
    }

    std::string readableText;
    google::protobuf::TextFormat::PrintToString(result, &readableText);

    readableFile << readableText;

    readableFile.close();

    // Output cache.
    automataCache.WriteAutomataCache(automataStillExisting, automatonFunctions);
    cache.WriteCacheData(alreadyUpToDate, uncachedFiles);

    OutputAlways("File cache data written to " + CacheFilename + ":\n\t" +
                 std::to_string(alreadyUpToDate.size()) + " files already in cache\n\t" +
                 std::to_string(uncachedFiles.size()) + " files updated\n\t" +
                 std::to_string(removedFiles.size()) + " files removed from the cache");

    return 0;
}
