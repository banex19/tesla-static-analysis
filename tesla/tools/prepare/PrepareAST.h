/** @file AST.h    Declaration of @ref tesla::TeslaAction, @ref tesla::TeslaConsumer. */
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

#ifndef AST_H
#define AST_H

#include "DataStructures.h"
#include "PrepareVisitor.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/StringRef.h>

namespace clang
{
class ASTContext;
class CompilerInstance;
} // namespace clang

namespace tesla
{

class TeslaConsumer : public clang::ASTConsumer
{
  public:
    TeslaConsumer(CollectedData& data, std::string InFilename, std::string OutFilename);
    virtual void HandleTranslationUnit(clang::ASTContext& Context);

  private:
    CollectedData& data;
    std::string InFile;
    std::string OutFile;
};

class TeslaAction : public clang::ASTFrontendAction
{
  public:
    TeslaAction(CollectedData& data, std::string OutFilename) : data(data), OutFile(OutFilename) {}

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance& Compiler, llvm::StringRef InFile);

  private:
    CollectedData& data;
    std::string OutFile;
};

class TeslaActionFactory : public clang::tooling::FrontendActionFactory
{
  public:
    TeslaActionFactory(CollectedData& data, std::string OutFilename) : data(data), OutFile(OutFilename) {}
    ~TeslaActionFactory() {}

    clang::FrontendAction* create();

  private:
    CollectedData& data;
    std::string OutFile;
};

} // namespace tesla

#endif // AST_H
