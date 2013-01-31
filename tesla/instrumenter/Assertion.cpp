/*! @file Assertion.cpp  Code for instrumenting TESLA assertion sites. */
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

#include "Assertion.h"
#include "Automaton.h"
#include "Instrumentation.h"
#include "Manifest.h"
#include "Names.h"

#include "tesla.pb.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

using std::set;
using std::string;


namespace tesla {

char TeslaAssertionSiteInstrumenter::ID = 0;

TeslaAssertionSiteInstrumenter::~TeslaAssertionSiteInstrumenter() {
  ::google::protobuf::ShutdownProtobufLibrary();
}

bool TeslaAssertionSiteInstrumenter::runOnModule(Module &M) {
  // If this module doesn't declare any assertions, just carry on.
  AssertFn = M.getFunction(ASSERTION_FN_NAME);
  if (!AssertFn)
    return false;

  // Find all calls to TESLA assertion pseudo-function (before we modify IR).
  set<CallInst*> AssertCalls;
  for (auto I = AssertFn->use_begin(); I != AssertFn->use_end(); ++I)
    AssertCalls.insert(cast<CallInst>(*I));


  // If we do use the TESLA assertion function, we need to match it up with
  // the information parsed by the TESLA analyser (stored in the Manifest).
  OwningPtr<Manifest> Manifest(Manifest::load(llvm::errs()));
  if (!Manifest)
    report_fatal_error("unable to load TESLA manifest");

  return ConvertAssertions(AssertCalls, *Manifest, M);;
}


bool TeslaAssertionSiteInstrumenter::ConvertAssertions(
    set<CallInst*>& AssertCalls, Manifest& Manifest, Module& M) {

  // Things we'll need later, common to all assertions.
  Type *RegisterT = RegisterType(M);

  // Convert each assertion into appropriate instrumentation.
  for (auto *Assert : AssertCalls) {
    // Prepare to insert new code in place of the assertion.
    IRBuilder<> Builder(Assert);

    Location Loc;
    ParseAssertionLocation(&Loc, Assert);

    OwningPtr<const Automaton> A(Manifest.FindAutomaton(Loc));
    if (!A) {
      // TODO: remove once DFA::Convert(NFA) works
      llvm::errs()
        << "WARNING: no (realisable) automaton for '"
        << ShortName(Loc)
        << "'!\n";

      Assert->removeFromParent();
      delete Assert;
      continue;

      report_fatal_error(
        "no automaton '" + ShortName(Loc) + "' in TESLA manifest");
    }

    // Record named values that might be passed to instrumentation, such as
    // function parameters and StoreInst results in the current BasicBlock.
    std::map<string,Value*> ValuesInScope;
    BasicBlock *Block = Assert->getParent();
    Function *Fn = Block->getParent();

    for (llvm::Argument& A : Fn->getArgumentList())
      ValuesInScope[A.getName()] = &A;

    for (Instruction& I : *Block)
      if (StoreInst *Store = dyn_cast<StoreInst>(&I)) {
        Value *V = Store->getPointerOperand();
        if (V->hasName())
          ValuesInScope[V->getName()] = V;
      }

    // Find the arguments to the relevant 'now' instrumentation function.
    const Assertion& AssertInfo = A->getAssertion();
    size_t ArgCount = AssertInfo.argument_size();
    std::vector<Value*> Args(ArgCount, NULL);

    for (const Argument& Arg : AssertInfo.argument()) {
      Value *V = ValuesInScope[Arg.name()];
      if (V == NULL)
        report_fatal_error("failed to find assertion arg '" + Arg.name() + "'");

      Args[Arg.index()] = Cast(V, Arg.name(), RegisterT, Builder);
    }

    Builder.CreateCall(InstrumentationFn(*A, M), Args);

    // Delete the call to the assertion pseudo-function.
    Assert->removeFromParent();
    delete Assert;
  }

  AssertFn->removeFromParent();
  delete AssertFn;

  return true;
}


void TeslaAssertionSiteInstrumenter::ParseAssertionLocation(
  Location *Loc, CallInst *Call) {

  assert(Call->getCalledFunction()->getName() == ASSERTION_FN_NAME);

  if (Call->getNumArgOperands() < 3)
    report_fatal_error("TESLA assertion must have at least 3 arguments");

  // The filename should be a global variable.
  GlobalVariable *NameVar =
    dyn_cast<GlobalVariable>(Call->getOperand(0)->stripPointerCasts());

  ConstantDataArray *A;
  if (!NameVar ||
      !(A = dyn_cast_or_null<ConstantDataArray>(NameVar->getInitializer()))) {
    Call->dump();
    report_fatal_error("unable to parse filename from TESLA assertion");
  }

  *Loc->mutable_filename() = A->getAsString();


  // The line and counter values should be constant integers.
  ConstantInt *Line = dyn_cast<ConstantInt>(Call->getOperand(1));
  if (!Line) {
    Call->getOperand(1)->dump();
    report_fatal_error("assertion line must be a constant int");
  }

  Loc->set_line(Line->getLimitedValue(INT_MAX));

  ConstantInt *Count = dyn_cast<ConstantInt>(Call->getOperand(2));
  if (!Count) {
    Call->getOperand(2)->dump();
    report_fatal_error("assertion count must be a constant int");
  }

  Loc->set_counter(Count->getLimitedValue(INT_MAX));
}


Function* TeslaAssertionSiteInstrumenter::InstrumentationFn(
  const Automaton& A, Module& M) {

  const Assertion& AssertInfo = A.getAssertion();
  const size_t ArgCount = AssertInfo.argument_size();

  Type *Void = Type::getVoidTy(M.getContext());
  Type *RegisterT = RegisterType(M);

  // NOW events only take arguments of type register_t.
  std::vector<Type*> ArgTypes(ArgCount, RegisterT);

  FunctionType *FnType = FunctionType::get(Void, ArgTypes, false);
  string Name = (ASSERTION_REACHED + "_" + Twine(A.ID())).str();

  Function *InstrFn = dyn_cast<Function>(M.getOrInsertFunction(Name, FnType));
  assert(InstrFn != NULL && "instrumentation function not a Function!");

  return InstrFn;
}

}

