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
#include <llvm/Analysis/TargetLibraryInfo.h>
using namespace llvm;

#include "Manifest.h"
#include "Remove.h"
#include "ThinTeslaAssertion.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

struct FunctionProperties
{
    FunctionProperties(const std::string& function)
        : function(function)
    {
    }

    bool atMostCalledOnce = false;
    bool neverCalled = false;
    bool definitelyHappens = false;

    std::string function;
};

struct ValueProperties
{
    ValueProperties(ThinTeslaEventPtr event)
        : event(event)
    {
    }

    bool alwaysCalledWithCorrectParams = false;

    ThinTeslaEventPtr event;
};

struct TemporalProperties
{
    TemporalProperties(const std::string& before, const std::string& after)
        : before(before), after(after)
    {
    }

    bool definitelyBefore = false;
    bool definitelyAfter = false;
    bool firstEventAfter = false;

    std::string before;
    std::string after;
};

const bool GUIDELINE_MODE = true;

/*
X (ASSERTION) Y

X SURELY_BEFORE ASSERTION => If X and ASSERTION happen, X surely happened before
X MUST_HAPPEN => X surely happens
X MUST_HAPPEN_BEFORE ASSERTION => If ASSERTION happens, X surely happened before

main -> x -> y -> z
main -> x -> k
main -> x -> y -> k
main -> x -> p
main -> x -> w -> p
main -> x -> y -> f

z => k
                                           
In x, p is after k
In x, p is after y 
In x, w is after k
In x, w is after y
In y, f is after k
*/

cl::opt<std::string>
    firstFunctionName("fn1", cl::Required);
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

    std::string bound;
    std::set<std::string> events;

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

    std::vector<std::string> GetCommonPrefix(const std::vector<std::string>& first, const std::vector<std::string>& second)
    {
        std::vector<std::string> common;

        for (size_t i = 0; i < first.size() && i < second.size(); ++i)
        {
            if (first[i] == second[i])
                common.push_back(first[i]);
            else
                break;
        }

        return common;
    }

    FunctionProperties ReportFunctionProperties(Module& M, CallGraph& graph, const std::string& bound, const std::string& function)
    {
        std::vector<std::string> path;

        FunctionProperties prop{function};
        llvm::errs() << "--------------------- "
                     << function << "\n";
        prop.definitelyHappens = DefinitelyHappens(M, graph, bound, function, path);
        if (prop.definitelyHappens)
            llvm::errs() << "[RESULT] Function " << function << " is definitely called\n";
        prop.atMostCalledOnce = AtMostCalledOnce(M, graph, bound, function);
        if (prop.atMostCalledOnce)
            llvm::errs() << "[RESULT] Function " << function << " is at most called once\n";
        prop.neverCalled = DefinitelyNeverCalled(M, graph, bound, function);
        if (prop.neverCalled)
            llvm::errs() << "[RESULT] Function " << function << " is definitely never called\n";

        llvm::errs() << "\n\n";

        return prop;
    }

    TemporalProperties ReportFunctionDependencies(Module& M, CallGraph& graph, const std::string& bound, const std::string& first, const std::string& second)
    {
        TemporalProperties prop{first, second};
        llvm::errs() << "--------------------- "
                     << first << " ---> " << second << "\n";

        prop.definitelyBefore = DefinitelyCalledAfter(M, graph, bound, first, second);
        if (prop.definitelyBefore)
            llvm::errs() << "[RESULT] Function " << first << " is definitely called before " << second << "\n";

        prop.definitelyAfter = DefinitelyCalledAfter(M, graph, bound, second, first);
        if (prop.definitelyAfter)
            llvm::errs() << "[RESULT] Function " << first << " is definitely called after " << second << "\n";

        prop.firstEventAfter = IsFirstEventAfter(M, graph, bound, first, second);
        if (prop.firstEventAfter)
            llvm::errs() << "[RESULT] Function " << second << " is definitely the first event to be called after " << first << "\n";

        llvm::errs() << "\n\n";

        return prop;
    }

    ValueProperties ReportValueProperties(Module& M, CallGraph& graph, const std::string& bound, ThinTeslaEventPtr& event)
    {
        ValueProperties prop{event};
        std::string function = event->GetInstrumentationTarget();
        if (event->NeedsParametricInstrumentation())
        {
            llvm::errs() << "--------------------- (value) " << function << "\n";
            auto params = event->GetParameters();

            prop.alwaysCalledWithCorrectParams = AlwaysCalledWithParameters(M, graph, bound, function, params);
            if (prop.alwaysCalledWithCorrectParams)
                llvm::errs() << "[RESULT] Function " << function << " is always called with the correct parameters\n";

            llvm::errs() << "\n\n";
        }
        return prop;
    }

    bool runOnTesla(Module& M)
    {
        std::unique_ptr<tesla::Manifest> manifest(tesla::Manifest::load(llvm::errs()));
        if (!manifest)
        {
            llvm::errs() << "Unable to load TESLA manifest";
            return false;
        }

        std::vector<ThinTeslaAssertion> assertions;

        for (auto& automaton : manifest->RootAutomata())
        {
            ThinTeslaAssertionBuilder builder{*manifest, automaton};
            for (auto& assertion : builder.GetAssertions())
            {
                assertions.push_back(*assertion);
            }
        }

        for (auto& assertion : assertions)
        {
            events.clear();

            for (size_t i = 1; i < assertion.events.size() - 2; ++i)
            {
                auto event = assertion.events[i];
                events.insert(event->IsAssertion() ? "__tesla_inline_assertion" : event->GetInstrumentationTarget());
            }

            for (size_t i = 1; i < assertion.events.size() - 2; ++i)
            {
                ThinTeslaEventPtr first = assertion.events[i];
                ThinTeslaEventPtr second = assertion.events[i + 1];
                std::string firstTarget = first->IsAssertion() ? "__tesla_inline_assertion" : first->GetInstrumentationTarget();
                std::string secondTarget = second->IsAssertion() ? "__tesla_inline_assertion" : second->GetInstrumentationTarget();

                llvm::errs() << "**************************************************\n";
                llvm::errs() << "Event " << first->id << ": " << firstTarget << " ---> ";
                llvm::errs() << "Event " << second->id << ": " << secondTarget << "\n";
                Analyse(M, assertion.events[0]->GetInstrumentationTarget(), first, second, second->IsFinal());
                llvm::errs() << "**************************************************\n\n";
            }
        }

        legacy::PassManager Passes;

        // Add an appropriate TargetLibraryInfo pass for the module's triple.
        auto TLI = new TargetLibraryInfoWrapperPass(Triple(M.getTargetTriple()));
        Passes.add(TLI);

        Passes.add(new tesla::RemoveInstrumenter(*manifest, true));

        Passes.run(M);

        return false;
    }

    virtual bool runOnModule(Module& M)
    {
        return runOnTesla(M);
    }

    bool Analyse(Module& M, const std::string& bound, ThinTeslaEventPtr& first, ThinTeslaEventPtr& second, bool secondIsFinal)
    {
        CallGraph graph{M};

        // graph.print(llvm::errs());

        const std::string& before = first->IsAssertion() ? "__tesla_inline_assertion" : first->GetInstrumentationTarget();
        const std::string& after = second->IsAssertion() ? "__tesla_inline_assertion" : second->GetInstrumentationTarget();
        const std::string& assertion = "__tesla_inline_assertion";

        if (M.getFunction(firstFunctionName) == nullptr || M.getFunction(secondFunctionName) == nullptr)
        {
            llvm::errs() << "Invalid functions\n";
            return false;
        }

        bool hasRecursion = false;

        auto pathsToBefore = GetPathsTo(M, graph, bound, before, hasRecursion);
        for (auto& path : pathsToBefore)
        {
            llvm::errs() << "Path to " << before << ": " << StringFromVector(path, " --> ") << "\n";
        }
        llvm::errs() << "There is recursion: " << hasRecursion << "\n";
        if (hasRecursion)
            return false;

        auto pathsToAfter = GetPathsTo(M, graph, bound, after, hasRecursion);
        for (auto& path : pathsToAfter)
        {
            llvm::errs() << "Path to " << after << ": " << StringFromVector(path, " --> ") << "\n";
        }
        llvm::errs() << "There is recursion: " << hasRecursion << "\n";
        if (hasRecursion)
            return false;

        FunctionProperties beforeProp = ReportFunctionProperties(M, graph, bound, before);
        FunctionProperties afterProp = ReportFunctionProperties(M, graph, bound, after);
        ValueProperties beforeValueProp = ReportValueProperties(M, graph, bound, first);
        ValueProperties afterValueProp = ReportValueProperties(M, graph, bound, second);
        TemporalProperties temporalProp = ReportFunctionDependencies(M, graph, bound, before, after);

        if (beforeProp.definitelyHappens && afterProp.definitelyHappens)
        {
            if ((beforeProp.atMostCalledOnce && afterProp.atMostCalledOnce) || (secondIsFinal && GUIDELINE_MODE))
            {
                if (temporalProp.firstEventAfter)
                {
                    llvm::errs() << "[OPTIMIZATION] Check for function " << after << " can be omitted\n";
                }
            }
        }
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

    std::vector<CallInst*> GetAllCallsAfter(Instruction* instr)
    {
        std::vector<CallInst*> calls;
        BasicBlock* parent = instr->getParent();

        bool afterInstr = false;
        for (auto& I : *parent)
        {
            if (&I == instr)
            {
                afterInstr = true;
                continue;
            }

            if (!afterInstr)
                continue;

            CallInst* call = dyn_cast<CallInst>(&I);
            if (call != nullptr)
            {
                calls.push_back(call);
            }
        }

        std::set<BasicBlock*> visited;

        for (auto succ : successors(parent))
        {
            GetAllCallsAfterAux(succ, calls, visited);
        }

        return calls;
    }

    void GetAllCallsAfterAux(BasicBlock* block, std::vector<CallInst*> calls, std::set<BasicBlock*> visited)
    {
        if (visited.find(block) != visited.end())
            return;

        for (auto& I : *block)
        {
            CallInst* call = dyn_cast<CallInst>(&I);
            if (call != nullptr)
            {
                calls.push_back(call);
            }
        }

        visited.insert(block);

        for (auto succ : successors(block))
        {
            GetAllCallsAfterAux(succ, calls, visited);
        }
    }

    std::map<size_t, ThinTeslaParameter> ParameterMap(std::vector<ThinTeslaParameter>& params)
    {
        std::map<size_t, ThinTeslaParameter> map;

        for (auto& param : params)
        {
            map[param.index] = param;
        }

        return map;
    }

    size_t GetConstantValue(Constant* val)
    {
        val = val->stripPointerCasts();
        ConstantInt* intVal = dyn_cast<ConstantInt>(val);
        if (intVal != nullptr)
        {
            return intVal->getLimitedValue();
        }
        ConstantPointerNull* nullConstant = dyn_cast<ConstantPointerNull>(val);
        if (nullConstant != nullptr)
        {
            return 0;
        }

        assert(false);
    }

    bool AlwaysCalledWithParameters(Module& M, CallGraph& graph, const std::string& bound, const std::string& function, std::vector<ThinTeslaParameter>& params)
    {
        bool hasRecursion = false;
        auto paths = GetPathsTo(M, graph, bound, function, hasRecursion);

        for (auto& param : params)
        {
            if (!param.isConstant)
                return false;
        }
        auto paramMap = ParameterMap(params);

        for (auto& path : paths)
        {
            if (path.size() <= 1)
                continue;

            std::string parentFunctionName = path[path.size() - 2];
            if (parentFunctionName != function)
            {
                auto calls = GetAllCallsToFunction(M.getFunction(parentFunctionName), function);

                for (auto& call : calls)
                {
                    for (size_t i = 0; i < call->getNumArgOperands(); ++i)
                    {
                        Value* arg = call->getArgOperand(i);

                        if (paramMap.find(i) != paramMap.end())
                        {

                            Constant* constant = dyn_cast<Constant>(arg);
                            if (constant != nullptr)
                            {
                                if (GetConstantValue(constant) != paramMap[i].constantValue)
                                    return false;
                            }
                            else
                            {
                                return false;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    bool MayHappen(Module& M, CallGraph& graph, const std::string& bound, const std::string& functionName)
    {
        bool hasRecursion = false;
        auto paths = GetPathsTo(M, graph, bound, functionName, hasRecursion);

        for (auto& path : paths)
        {
            if (path.size() > 1)
                return true;
        }

        return false;
    }

    bool DefinitelyHappens(Module& M, CallGraph& graph, const std::string& bound, const std::string& functionName, std::vector<std::string>& goodPath)
    {
        goodPath = {};

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
                    BasicBlock* callBlock = call->getParent();

                    if (DominatesAllExits(*parentFunction, tree, *callBlock))
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
            {
                goodPath = path;
                return true;
            }
        }

        return false;
    }

    bool DefinitelyCalledAfter(Module& M, CallGraph& graph, const std::string& bound, const std::string& before, const std::string& after)
    {
        if (before == after)
            return false;

        bool hasRecursion = false;
        auto pathsToBefore = GetPathsTo(M, graph, bound, before, hasRecursion);
        auto pathsToAfter = GetPathsTo(M, graph, bound, after, hasRecursion);

        for (auto& pathToAfter : pathsToAfter)
        {
            for (auto& pathToBefore : pathsToBefore)
            {
                int i = 0;
                for (; i < pathToBefore.size() && i < pathToAfter.size(); ++i)
                {
                    if (pathToBefore[i] != pathToAfter[i])
                        break;
                }

                assert(i > 0);

                //     assert(i < pathToBefore.size());
                //   assert(i < pathToAfter.size());

                assert(pathToBefore[i] != pathToAfter[i]);

                //   llvm::errs() << "Checking that in " << pathToBefore[i - 1] << ", " << pathToAfter[i] << " is always after " << pathToBefore[i] << "\n";

                bool defAfter = CallDefinitelyAfter(*M.getFunction(pathToBefore[i - 1]),
                                                    pathToAfter[i], pathToBefore[i]);

                /*   llvm::errs() << "Calls to " << after << " definitely after " << pathToBefore[pathToBefore.size() - 1] << " (via " << pathToBefore[i - 1] << ") : "
                             << (defAfter ? "yes" : "no")
                             << "\n";  */

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

        for (auto& path : pathsToFunc)
        {
            if (path.size() >= 2)
                return false;
        }

        return true;
    }

    bool IsFirstEventAfter(Module& M, CallGraph& graph, const std::string& bound, const std::string& before, const std::string& after)
    {
        bool hasRecursion = false;
        auto pathsToBefore = GetPathsTo(M, graph, bound, before, hasRecursion);
        auto pathsToAfter = GetPathsTo(M, graph, bound, after, hasRecursion);

        if (pathsToBefore.size() == 0 || pathsToAfter.size() == 0)
            return false;

        for (auto& pathToBefore : pathsToBefore)
        {
            if (!CheckPath(M, graph, bound, before, after, pathToBefore))
                return false;
        }

        return true;
    }

    bool
    CheckPath(Module& M, CallGraph& graph, const std::string& bound, const std::string& before, const std::string& after,
              std::vector<std::string>& pathToBefore)
    {
        bool hasRecursion = false;
        auto pathsToAfter = GetPathsTo(M, graph, bound, after, hasRecursion);

        for (auto& path : pathsToAfter)
        {
            bool happens = false;

            auto common = GetCommonPrefix(path, pathToBefore);
            if (path.size() > common.size() && common[common.size() - 1] == before) // Before function contains a call to "after".
            {
                happens = MayHappen(M, graph, before, after);
            }

            if (happens) // Call definitely happens in the bounds of "before".
            {
                for (auto& event : events) // Check that all other events cannot happen before "after".
                {
                    if (event == after)
                        continue;

                    bool definitelyAfter = DefinitelyCalledAfter(M, graph, before, after, event);
                    if (!definitelyAfter)
                    {
                        llvm::errs() << "Event " << event << " is not definitely after " << after << " in " << before << "\n";
                        //  llvm::errs() << "Path to before: " << StringFromVector(pathToBefore) << "\n";
                        return false;
                    }
                }

                return true;
            }

            std::string& lastCommonFunction = common[common.size() - 1];
            size_t firstDifferent = common.size();

            // Check that this happens after "before".
            happens = MayHappen(M, graph, lastCommonFunction, path[firstDifferent]);
            bool definitelyAfter = DefinitelyCalledAfter(M, graph, lastCommonFunction, pathToBefore[firstDifferent], path[firstDifferent]);

            if (happens && definitelyAfter)
            {
                // Now check that it is not possible that any other event happens between "before" and "after".
                for (auto& event : events)
                {
                    if (event == after)
                        continue;

                    for (size_t i = firstDifferent; i < pathToBefore.size(); ++i)
                    {
                        if (event == before)
                            break;

                        bool neverCalled = DefinitelyNeverCalled(M, graph, pathToBefore[i], event);
                        if (!neverCalled)
                        {
                            llvm::errs() << "Event " << event << " is called in " << pathToBefore[i] << "\n";
                            //   llvm::errs() << "Path to before: " << StringFromVector(pathToBefore) << "\n";
                            goto trynewpath;
                        }
                    }

                    definitelyAfter = DefinitelyNeverCalled(M, graph, lastCommonFunction, event) ||
                                      CallDefinitelyNotAfter(M.getFunction(lastCommonFunction), pathToBefore[firstDifferent], event) ||
                                      DefinitelyCalledAfter(M, graph, lastCommonFunction, path[firstDifferent], event);
                    if (!definitelyAfter)
                    {
                        // llvm::errs() << "Event " << event << " is not definitely after " << path[firstDifferent] << " in " << lastCommonFunction << "\n";
                        //   llvm::errs() << "Path: " << StringFromVector(path) << "\n";
                        // llvm::errs() << "Path to before: " << StringFromVector(pathToBefore) << "\n";
                        goto trynewpath;
                    }
                }

                return true;
            }

        trynewpath:
            (void)true;
        }

        return false;
    }

    bool DoesConditionDependOnArgs(Function& F, BasicBlock& block)
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
        std::set<BasicBlock*> visited{start};
        return BlockDefinitelyAfterAux(start, before, visited);
    }

    bool BlockDefinitelyAfterAux(BasicBlock* start, BasicBlock* before, std::set<BasicBlock*> visited)
    {
        for (auto succ : successors(start))
        {
            if (visited.find(succ) != visited.end()) // We went back in a loop without encountering "before".
            {

                return true;
            }

            if (succ == before) // We found "before".
            {

                return false;
            }

            visited.insert(succ);

            if (!BlockDefinitelyAfterAux(succ, before, visited)) // Check successor.
                return false;
        }

        return true;
    }

    bool CallDefinitelyAfterInBlock(BasicBlock* block, const std::string& before, const std::string& after)
    {
        bool foundAfter = false;
        for (auto& I : *block)
        {
            CallInst* callInst = dyn_cast<CallInst>(&I);
            if (callInst != nullptr)
            {
                if (!foundAfter && callInst->getCalledFunction()->getName() == after)
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
        return true;
    }

    bool CallDefinitelyNotAfter(Function* F, const std::string& before, const std::string& after)
    {
        auto callsBefore = GetAllCallsToFunction(F, before);
        auto callsAfter = GetAllCallsToFunction(F, after);

        for (auto& callBefore : callsBefore)
        {
            BasicBlock* beforeBlock = callBefore->getParent();

            for (auto& callAfter : callsAfter)
            {

                BasicBlock* afterBlock = callAfter->getParent();

                if (beforeBlock == afterBlock && !CallDefinitelyAfterInBlock(beforeBlock, after, before))
                {

                    return false;
                }
                else if (beforeBlock != afterBlock && !BlockDefinitelyAfter(beforeBlock, afterBlock))
                {

                    return false;
                }
            }
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
                if (beforeBlock == callBlock && !CallDefinitelyAfterInBlock(callBlock, before, toCheck))
                {
                    return false;
                }
                else // Calls in different blocks.
                    if (beforeBlock != callBlock && !BlockDefinitelyAfter(callBlock, beforeBlock))
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
