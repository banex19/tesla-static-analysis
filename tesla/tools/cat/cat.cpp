/** @file  graph.cpp    Tool for graphing TESLA manifests. */
/*
 * Copyright (c) 2013 Jonathan Anderson
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

#include "Automaton.h"
#include "Debug.h"
#include "Manifest.h"

#include "tesla.pb.h"

#include <google/protobuf/text_format.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace tesla;

using std::string;

cl::list<string> InputFiles(cl::desc("<input files>"),
                            cl::Positional, cl::OneOrMore);

cl::opt<string> OutputFile("o", cl::desc("<output file>"), cl::init("-"));

int main(int argc, char* argv[])
{
    cl::ParseCommandLineOptions(argc, argv);

    auto& err = llvm::errs();

    ManifestFile Result;
    std::map<Identifier, const AutomatonDescription*> Automata;
    std::map<Identifier, const Usage*> Usages;

    size_t id = 0;

    for (auto& Filename : InputFiles)
    {
        std::unique_ptr<Manifest> Manifest(Manifest::load(llvm::errs(),
                                                          Automaton::Unlinked,
                                                          Filename));
        if (!Manifest)
        {
            err << "Unable to read manifest '" << Filename << "'\n";
            return 1;
        }

        for (auto i : Manifest->AllAutomata())
        {
            auto Existing = Automata.find(i.first);
            if (Existing == Automata.end())
            {
                Automata[i.first] = &(*Result.add_automaton() = *i.second);
            }
            else if (*Existing->second != *i.second)
            {
                // If we already have this automaton, assert that both are
                // exactly the same.
                panic("Attempting to cat two files containing automaton '" + ShortName(Existing->first) + "', but these automata are not exactly the same.");
            }
        }

        for (auto i : Manifest->RootAutomata())
        {
            auto Existing = Usages.find(i->identifier());
            if (Existing == Usages.end())
            {
                ((Usage*)i)->set_uniqueid(id);
                *Result.add_root() = *i;
                 ((Usage*)i)->set_uniqueid(0);
                Usages[i->identifier()] = &(*i);
                id++;
            }
            else if (*Existing->second != *i)
            {
                panic("Attempting to cat two files containing root '" + ShortName(i->identifier()) + "', but these roots are not exactly the same.");
            }
        }
    }

    string ProtobufText;
    google::protobuf::TextFormat::PrintToString(Result, &ProtobufText);

    bool UseFile = (OutputFile != "-");
    std::unique_ptr<raw_fd_ostream> outfile;

    if (UseFile)
    {
        std::error_code OutErrorInfo;
        outfile.reset(new raw_fd_ostream(OutputFile.c_str(), OutErrorInfo, llvm::sys::fs::F_RW));
    }
    raw_ostream& out = UseFile ? *outfile : llvm::outs();
    out << ProtobufText;

    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}
