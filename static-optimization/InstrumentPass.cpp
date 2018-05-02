#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

/*
X (ASSERTION) Y

X SURELY_BEFORE ASSERTION => If X and ASSERTION happen, X surely happened before
X MUST_HAPPEN => X surely happens
X MUST_HAPPEN_BEFORE ASSERTION => If ASSERTION happens, X surely happened before
*/

cl::opt<std::string> firstFunctionName("fn1", cl::Required);
cl::opt<std::string> secondFunctionName("fn2", cl::Required);

std::string StringFromVector(const std::vector<std::string>& vec,
                             const std::string& separator = ", ")
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

std::string StringFromSet(const std::set<std::string>& s, const std::string& separator = ", ")
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

namespace
{
struct InstrumentPass : public ModulePass
{
    static char ID;
    InstrumentPass() : ModulePass(ID) {}

    virtual void getAnalysisUsage(AnalysisUsage& AU) const override
    {
        AU.setPreservesAll();
        AU.addRequired<DominatorTreeWrapperPass>();
        AU.addRequired<LoopInfoWrapperPass>();
    }

    std::vector<std::vector<std::string>> GetPathsTo(Module& M, CallGraph& graph,
                                                     const std::string& source,
                                                     const std::string& dest,
                                                     bool& out_hasRecursion)
    {
        out_hasRecursion = false;
        std::vector<std::vector<std::string>> paths;
        std::set<std::string> recursivePoints;

        if (source == dest)
            paths.push_back({source});
        else
            GetPathsToAux(M, graph[M.getFunction(source)], dest, {source}, paths, {}, recursivePoints);

        for (auto& path : paths)
        {
            if (out_hasRecursion)
                break;

            for (auto& function : path)
            {
                if (recursivePoints.find(function) != recursivePoints.end())
                {
                    out_hasRecursion = true;
                    break;
                }
            }
        }

        return paths;
    }

    void GetPathsToAux(Module& M, CallGraphNode* node, const std::string& dest,
                       std::vector<std::string> curr,
                       std::vector<std::vector<std::string>>& paths,
                       std::unordered_map<std::string, bool> alreadyChecked, std::set<std::string>& recursivePoints)
    {

        if (node == nullptr || node->getFunction() == nullptr)
            return;

        alreadyChecked[node->getFunction()->getName()] = true;

        for (auto f : *node)
        {
            CallGraphNode& next = *(f.second);

            if (next.getFunction() == nullptr)
                continue;

            if (alreadyChecked[next.getFunction()->getName()])
            {
                recursivePoints.insert(next.getFunction()->getName());
                continue;
            }

            std::vector<std::string> newCurr = curr;

            newCurr.push_back(next.getFunction()->getName());

            if (next.getFunction()->getName() == dest)
                paths.push_back(newCurr);

            GetPathsToAux(M, &next, dest, newCurr, paths, alreadyChecked, recursivePoints);
        }
    }

    void ReportFunctionProperties(Module& M, CallGraph& graph, const std::string& bound, const std::string& function)
    {
        llvm::errs() << "---------------------" << function << "---------------------\n";
        bool definitelyHappens = DefinitelyHappens(M, graph, bound, function);
        if (definitelyHappens)
            llvm::errs() << "[RESULT] Function " << function << " is definitely called\n";
        bool calledOnce = AtMostCalledOnce(M, graph, bound, function);
        if (calledOnce)
            llvm::errs() << "[RESULT] Function " << function << " is at most called once\n";
        bool neverCalled = DefinitelyNeverCalled(M, graph, bound, function);
        if (neverCalled)
            llvm::errs() << "[RESULT] Function " << function << " is definitely never called\n";

        if (!definitelyHappens && !calledOnce)
        {
            llvm::errs() << "[RESULT] Function " << function << " has no special property\n";
        }

        llvm::errs() << "\n\n";
    }

    virtual bool runOnModule(Module& M)
    {
        CallGraph graph{M};

        // graph.print(llvm::errs());

        if (M.getFunction(firstFunctionName) == nullptr || M.getFunction(secondFunctionName) == nullptr)
        {
            llvm::errs() << "Invalid functions\n";
            return false;
        }

        const std::string& before = firstFunctionName;
        const std::string& after = secondFunctionName;
        const std::string& assertion = "tesla_assert";
        const std::string& bound = "main";

        bool hasRecursion = false;

        auto pathsToBefore = GetPathsTo(M, graph, bound, before, hasRecursion);
        for (auto& path : pathsToBefore)
        {
            llvm::errs() << "Path to " << before << ": " << StringFromVector(path, " --> ") << "\n";
        }
        llvm::errs() << "There is recursion: " << hasRecursion << "\n";

        auto pathsToAfter = GetPathsTo(M, graph, bound, after, hasRecursion);
        for (auto& path : pathsToAfter)
        {
            llvm::errs() << "Path to " << after << ": " << StringFromVector(path, " --> ") << "\n";
        }
        llvm::errs() << "There is recursion: " << hasRecursion << "\n";

        auto pathsToAssertion = GetPathsTo(M, graph, bound, assertion, hasRecursion);
        llvm::errs() << "There are " << pathsToAssertion.size()
                     << " ways to reach the assertion\n";
        for (auto& path : pathsToAssertion)
        {
            llvm::errs() << "Path to assertion: " << StringFromVector(path, " --> ") << "\n";
        }

        ReportFunctionProperties(M, graph, bound, before);
        ReportFunctionProperties(M, graph, bound, assertion);
        ReportFunctionProperties(M, graph, bound, after);

        /* for (auto& path : pathsToBefore)
        {
            for (size_t i = path.size() - 1; i > 0; --i)
            {

                Function* to = M.getFunction(path[i]);
                Function* from = M.getFunction(path[i - 1]);

                if (!from->isDeclaration())
                {
                    auto& tree =
                        getAnalysis<DominatorTreeWrapperPass>(*from).getDomTree();

                    llvm::errs() << "Function " << path[i - 1] << "\n";

                    for (auto& BB : *from)
                    {
                        for (auto& I : BB)
                        {
                            CallInst* callInst = dyn_cast<CallInst>(&I);
                            if (callInst &&
                                callInst->getCalledFunction()->getName() == path[i])
                            {
                                bool dominatesExits = DominatesAllExits(*from, tree, BB);
                                llvm::errs() << "Dominates exit blocks: " << dominatesExits;

                                if (!dominatesExits)
                                {
                                    BasicBlock* pred = GetFirstParentCondition(BB);
                                    if (pred && DominatesAllExits(*from, tree, *pred))
                                    {
                                        llvm::errs() << " (but a parent does)\n";
                                        llvm::errs() << *pred;
                                        llvm::errs() << "\nCondition depends on function args: "
                                                     << (TerminatesWithConditionalBranch(*pred) &&
                                                         DoesConditionDependOnArgs(*from, *pred))
                                                     << "\n";
                                    }
                                }

                                llvm::errs() << "\n";
                            }
                        }
                    }
                }
            }
        } */

        return false;
    }

    BasicBlock* GetFirstParentCondition(BasicBlock& block)
    {
        BasicBlock* pred = block.getSinglePredecessor();
        while (pred)
        {

            if (TerminatesWithConditionalBranch(*pred))
                return pred;

            pred = pred->getSinglePredecessor();
        }
        return nullptr;
    }

    std::vector<CallInst*> GetAllCallsToFunction(Function* parent, const std::string& functionName)
    {
        std::vector<CallInst*> calls;
        for (auto& B : *parent)
        {
            for (auto& I : B)
            {
                CallInst* call = dyn_cast<CallInst>(&I);
                if (call != nullptr && call->getCalledFunction()->getName() == functionName)
                {
                    calls.push_back(call);
                }
            }
        }

        return calls;
    }

    bool DefinitelyHappens(Module& M, CallGraph& graph, const std::string& bound, const std::string& functionName)
    {
        bool hasRecursion = false;
        auto paths = GetPathsTo(M, graph, bound, functionName, hasRecursion);

        for (auto& path : paths)
        {
            bool pathIsGood = true;
            for (int i = path.size() - 2; i >= 0; --i)
            {
                Function* parentFunction = M.getFunction(path[i]);
                DominatorTree& tree = getAnalysis<DominatorTreeWrapperPass>(*parentFunction).getDomTree();

                auto calls = GetAllCallsToFunction(M.getFunction(path[i]), path[i + 1]);

                bool definitelyCalled = false;

                for (auto& call : calls)
                {
                    BasicBlock* entryBlock = &parentFunction->getEntryBlock();
                    BasicBlock* callBlock = call->getParent();

                    if (tree.dominates(entryBlock, callBlock))
                    {
                        definitelyCalled = true;
                        break;
                    }
                }

                if (!definitelyCalled) // Drop this path.
                {
                    pathIsGood = false;
                    break;
                }
            }

            if (pathIsGood)
                return true;
        }

        return false;
    }

    bool DefinitelyCalledAfter(Module& M, CallGraph& graph, const std::string& bound, const std::string& before, const std::string& after)
    {
        bool hasRecursion = false;
        auto pathsToBefore = GetPathsTo(M, graph, bound, before, hasRecursion);
        auto pathsToAfter = GetPathsTo(M, graph, bound, after, hasRecursion);

        for (auto& pathToAfter : pathsToAfter)
        {
            for (auto& pathToBefore : pathsToBefore)
            {
                size_t i = 0;
                for (; i < pathToBefore.size() && i < pathToAfter.size(); ++i)
                {
                    if (pathToBefore[i] != pathToAfter[i])
                        break;
                }

                assert(i > 0);
                assert(pathToBefore[i] != pathToAfter[i]);

                llvm::errs() << "Checking that in " << pathToBefore[i - 1] << ", " << pathToAfter[i] << " is always after " << pathToBefore[i] << "\n";

                bool defAfter = CallDefinitelyAfter(*M.getFunction(pathToBefore[i - 1]),
                                                    pathToAfter[i], pathToBefore[i]);

                llvm::errs() << "Calls to " << after << " definitely after " << pathToBefore[pathToBefore.size() - 1] << " (via " << pathToBefore[i - 1] << ") : "
                             << (defAfter ? "yes" : "no")
                             << "\n";

                if (!defAfter)
                    return false;
            }
        }

        return true;
    }

    bool AtMostCalledOnce(Module& M, CallGraph& graph, const std::string& bound, const std::string& functionName)
    {
        bool hasRecursion = false;
        auto pathsToFunc = GetPathsTo(M, graph, bound, functionName, hasRecursion);

        if (hasRecursion)
            return false;

        if (pathsToFunc.size() > 1)
            return false;

        for (auto& path : pathsToFunc)
        {
            for (int i = (int)path.size() - 2; i >= 0; --i)
            {
                Function* parentFunction = M.getFunction(path[i]);
                LoopInfo& loopInfo = getAnalysis<LoopInfoWrapperPass>(*parentFunction).getLoopInfo();

                auto calls = GetAllCallsToFunction(M.getFunction(path[i]), path[i + 1]);

                assert(calls.size() > 0);

                if (calls.size() > 1)
                    return false;

                for (auto& call : calls)
                {
                    BasicBlock* parentBlock = call->getParent();
                    if (loopInfo.getLoopFor(parentBlock) != nullptr)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    bool DefinitelyNeverCalled(Module& M, CallGraph& graph, const std::string& bound, const std::string& functionName)
    {
        bool hasRecursion = false;
        auto pathsToFunc = GetPathsTo(M, graph, bound, functionName, hasRecursion);

        return pathsToFunc.size() == 0;
    }

    bool
    DoesConditionDependOnArgs(Function& F, BasicBlock& block)
    {
        assert(TerminatesWithConditionalBranch(block));

        BranchInst* branch = dyn_cast<BranchInst>(block.getTerminator());

        auto condInst = dyn_cast<Instruction>(branch->getCondition());

        if (condInst)
        {
            for (Use& U : condInst->operands())
            {
                bool isArg = false;
                for (auto& arg : F.args())
                {
                    if (U.get() == &arg)
                    {
                        isArg = true;
                        break;
                    }
                }

                if (!isArg)
                    return false;
            }
        }
        else
        {
            return false;
        }
        return true;
    }

    bool TerminatesWithConditionalBranch(BasicBlock& block)
    {
        BranchInst* branch = dyn_cast<BranchInst>(block.getTerminator());
        return (branch != nullptr && !branch->isUnconditional());
    }

    bool BlockDefinitelyAfter(BasicBlock* start, BasicBlock* before)
    {
        for (auto succ : successors(start))
        {
            if (succ == before)
                return false;

            if (!BlockDefinitelyAfter(succ, before))
                return false;
        }

        return true;
    }

    bool CallDefinitelyAfter(Function& F, const std::string& toCheck,
                             const std::string& before)
    {
        std::vector<BasicBlock*> calls;
        std::vector<BasicBlock*> callsBefore;
        for (auto& BB : F)
        {
            for (auto& I : BB)
            {
                CallInst* callInst = dyn_cast<CallInst>(&I);
                if (callInst != nullptr)
                {
                    if (callInst->getCalledFunction()->getName() == toCheck)
                        calls.push_back(&BB);
                    else if (callInst->getCalledFunction()->getName() == before)
                        callsBefore.push_back(&BB);
                }
            }
        }
        for (auto callBlock : calls)
        {
            for (auto beforeBlock : callsBefore)
            {
                // Calls in the same block.
                if (beforeBlock == callBlock)
                {
                    bool foundAfter = false;
                    for (auto& I : *callBlock)
                    {
                        CallInst* callInst = dyn_cast<CallInst>(&I);
                        if (callInst != nullptr)
                        {
                            if (callInst->getCalledFunction()->getName() == toCheck)
                            {
                                foundAfter = true;
                            }
                            else if (callInst->getCalledFunction()->getName() == before)
                            {
                                if (foundAfter)
                                    return false;
                            }
                        }
                    }
                }
                else // Calls in different blocks.
                    if (!BlockDefinitelyAfter(callBlock, beforeBlock))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool DominatesAllExits(Function& F, DominatorTree& tree, BasicBlock& block)
    {
        std::vector<BasicBlock*> exits;
        for (auto& BB : F)
        {
            for (auto& I : BB)
            {
                ReturnInst* ret = dyn_cast<ReturnInst>(&I);
                if (ret != nullptr)
                {
                    exits.push_back(&BB);
                    break;
                }
            }
        }

        bool dominates = true;
        for (auto exit : exits)
        {
            if (!tree.dominates(&block, exit))
            {
                dominates = false;
                break;
            }
        }

        return dominates;
    }
}; // namespace
} // namespace

char InstrumentPass::ID = 0;

// Register the pass both for when no optimizations and all optimizations are
// enabled.
static void registerInstrumentPass(const PassManagerBuilder&,
                                   legacy::PassManagerBase& PM)
{
    PM.add(new InstrumentPass());
}

static void registerInstrumentPassOpt(const PassManagerBuilder&,
                                      legacy::PassManagerBase& PM)
{
    PM.add(new InstrumentPass());
}

static RegisterStandardPasses
    RegisterMyPass(PassManagerBuilder::EP_EnabledOnOptLevel0,
                   registerInstrumentPass);

static RegisterStandardPasses
    RegisterMyPassOpt(PassManagerBuilder::EP_ModuleOptimizerEarly,
                      registerInstrumentPassOpt);
