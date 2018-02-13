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

#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "PrepareAST.h"
#include "Debug.h"
#include "PrepareStructures.h"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/CommonOptionsParser.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

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

cl::opt<std::string> OutputFile(
    "o",
    cl::desc("<output file>"),
    cl::Required);

cl::opt<bool> Verbose(
    "v",
    cl::desc("Enable verbosity"),
    cl::init(false));

cl::list<std::string> ExtraExtensions(
    "e",
    cl::desc("Extra extensions to consider"));

std::vector<std::string> extensions = {".cpp", ".c", ".cc"};

void PanicIfError(std::error_code err)
{
    if (err)
        panic(err.message());
}

void OutputVerbose(const std::string &msg)
{
    if (Verbose)
    {
        llvm::errs() << msg << "\n";
    }
}

std::unordered_map<std::string, CachedFile> ReadCacheFile(std::string &filename)
{
    std::unordered_map<std::string, CachedFile> cachedFiles;

    if (filename == "")
    {
        panic("Output file not specified. Specify an output file with -o");
    }

    std::ifstream file(filename);

    if (!file)
    {
        llvm::errs() << "WARNING: Cache file " + filename + " not found - creating a new file\n";
        std::ofstream newFile(filename);

        if (!newFile)
            panic("Could not create file " + filename);

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
            panic("Cache file malformed - Delete the file \"" + filename + "\" and try again");
        }

        cachedFiles[sourceFile] = {sourceFile, timestamp};
    }

    file.close();

    return cachedFiles;
}

std::vector<std::string> TraverseSources(const std::string &sourceRoot, std::unordered_map<std::string, CachedFile> cached)
{
    std::vector<std::string> uncached;

    std::error_code err;
    directory_iterator it(sourceRoot, err);

    PanicIfError(err);

    while (it != directory_iterator())
    {
        PanicIfError(err);

        if (is_directory(it->path())) // Go deeper.
        {
            TraverseSources(it->path(), cached);
        }
        else // A file.
        {
            if (std::find(extensions.begin(), extensions.end(), extension(it->path())) != extensions.end()) // A source file we need to consider.
            {
                auto status = it->status();
                PanicIfError(status.getError());

                size_t timestamp = status->getLastModificationTime().time_since_epoch().count();

                if (cached.find(it->path()) != cached.end() && cached[it->path()].timestamp == timestamp)
                {
                     OutputVerbose("Found file " + it->path() + " which is already cached and up-to-date");                
                }
                else
                {
                    uncached.push_back(it->path();)
                }
            }
        }

        it.increment(err);
    }

    return uncached;
}

int main(int argc, const char **argv)
{
    llvm::PrettyStackTraceProgram X(argc, argv);

    // Add a preprocessor definition to indicate we're doing TESLA parsing.
    std::vector<const char *> args(argv, argv + argc);
    args.push_back("-D");
    args.push_back("__TESLA_PREPARE__");

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
            "Need compilation options, e.g. tesla-prepare -s ./src/ -o tesla.cache -- -I ../include");

    cl::ParseCommandLineOptions(argc, argv);

    for (auto &ext : ExtraExtensions)
        extensions.push_back(ext);

    auto cachedFiles = ReadCacheFile(OutputFile);
    TraverseSources(SourceRoot, cachedFiles);

    std::unique_ptr<TeslaActionFactory> Factory(new TeslaActionFactory(OutputFile));

    // ClangTool Tool(*Compilations, SourcePaths);

    // return Tool.run(Factory.get());

    return 0;
}
