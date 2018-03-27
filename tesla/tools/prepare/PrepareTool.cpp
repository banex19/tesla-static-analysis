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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "AutomatonParser.h"
#include "CacheFile.h"
#include "Utils.h"

#include "DataStructures.h"
#include "Debug.h"
#include "PrepareAST.h"

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

cl::opt<std::string> SourceRoot(
    "s",
    cl::desc("<source root dir>"),
    cl::Required);

cl::opt<std::string> OutputDir(
    "o",
    cl::desc("<output directory>"),
    cl::Required);

cl::opt<bool> Verbose(
    "v",
    cl::desc("Enable verbosity"),
    cl::init(false));

cl::opt<bool> OverwriteCache(
    "x",
    cl::desc("Overwrite the current cache and build a new one from scratch"),
    cl::init(false));

cl::list<std::string> ExtraExtensions(
    "a",
    cl::desc("Extra extensions to consider"));

cl::list<std::string> ExtraExceptions(
    "e",
    cl::desc("Substrings that will cause a directory or file to be ignored"));

std::vector<std::string> exceptions = {"cmake"};
std::vector<std::string> extensions = {".cpp", ".c", ".cc"};

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

void TraverseSourcesRec(const std::string& sourceRoot, std::unordered_map<std::string, TimestampedFile>& cached, std::vector<TimestampedFile>& uncached)
{
    std::error_code err;
    directory_iterator it(sourceRoot, err);

    PanicIfError(err);

    while (it != directory_iterator())
    {
        PanicIfError(err);

        std::string path = it->path();
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

                    if (cached.find(path) != cached.end() && cached[path].timestamp == timestamp)
                    {
                        OutputVerbose("Found file " + path + " which is already cached and up-to-date", Verbose);
                        cached[path].upToDate = true;
                        cached[path].stillExisting = true;
                    }
                    else
                    {
                        if (cached.find(path) != cached.end())
                            cached[path].stillExisting = true;
                            
                        uncached.push_back({path, timestamp, false, true});
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

int main(int argc, const char** argv)
{
    llvm::PrettyStackTraceProgram X(argc, argv);

    // Add a preprocessor definition to indicate we're doing TESLA parsing.
    std::vector<const char*> args(argv, argv + argc);
    args.push_back("-D");
    args.push_back("__TESLA_ANALYSER__");

    // Change argc and argv to refer to the vector's memory.
    // The CompilationDatabase will modify these, so we shouldn't pass in
    // args.data() directly.
    argc = (int)args.size();
    assert(((size_t)argc) == args.size()); // check for overflow

    argv = args.data();

    std::string errorMsg;
    std::unique_ptr<CompilationDatabase> Compilations(
        FixedCompilationDatabase::loadFromCommandLine(argc, argv, errorMsg));

    if (!Compilations)
        panic(
            "Need compilation options, e.g. tesla-prepare -s ./src/ -o teslacache -- -I ../include");

    cl::ParseCommandLineOptions(argc, argv);

    if (!is_directory(OutputDir))
    {
        panic("Output path is not a directory");
    }

    OutputDir += "/";

    std::string OutputFilename = OutputDir + SanitizeFilename("filecache");
    std::string AutomataFilename = OutputDir + SanitizeFilename("automatacache");

    for (auto& ext : ExtraExtensions)
        extensions.push_back(ext);

    OutputAlways("Considering files with the following extensions: " + StringFromVector(extensions));

    for (auto& exc : ExtraExceptions)
        exceptions.push_back(exc);

    OutputAlways("The following substrings will be ignored if encountered: " + StringFromVector(exceptions));

    CacheFile cache{OutputFilename};
    AutomataFile automataCache{AutomataFilename};

    auto cachedAutomata = automataCache.ReadAutomata(OverwriteCache);

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

        uncachedFilenames[filename] = true;

        CollectedData data;

        std::unique_ptr<TeslaActionFactory> Factory(new TeslaActionFactory(data, OutputDir + SanitizeFilename("TESLA_" + filename)));

        ClangTool Tool(*Compilations, std::vector<std::string>{filename});

        Tool.run(Factory.get());

        // Cache function names.
        std::ofstream file{OutputDir + SanitizeFilename("DEFS_" + filename), std::ofstream::trunc};

        if (!file)
            panic("Could not create cache file for " + filename);

        for (auto& def : data.definedFunctionNames)
        {
            file << def << "\n";
        }

        file.close();

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

    // Make sure to remove automata that have been either updated or removed.
    std::unordered_map<std::string, std::vector<AutomatonSummary>> automataStillExisting;
    for (auto& cached : cachedAutomata)
    {
        if (removedFiles.find(cached.first) == removedFiles.end() && uncachedFilenames.find(cached.first) == uncachedFilenames.end())
            automataStillExisting[cached.first] = cached.second;
    }

    automataCache.WriteAutomata(automataStillExisting, automatonFunctions);
    cache.WriteCacheData(alreadyUpToDate, uncachedFiles);

    OutputAlways("Cache data written to " + OutputFilename + ":\n\t" +
                 std::to_string(alreadyUpToDate.size()) + " files already in cache\n\t" +
                 std::to_string(uncachedFiles.size()) + " files updated\n\t" +
                 std::to_string(removedFiles.size()) + " files removed from the cache");

    return 0;
}
