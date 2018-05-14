#pragma once

#include "../../libtesla/c_thintesla/TeslaState.h"
#include "llvm/Support/raw_ostream.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include "Automaton.h"
#include "Manifest.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <set>

class ThinTeslaAssertion;
class ThinTeslaEvent;
class ThinTeslaFunction;
class ThinTeslaParametricFunction;
class ThinTeslaAssertionSite;

struct ThinTeslaParameter
{
    ThinTeslaParameter()
    {
    }

    ThinTeslaParameter(size_t index, size_t constantValue)
        : exists(true), isConstant(true), constantValue(constantValue), index(index)
    {
    }

    ThinTeslaParameter(size_t index, const std::string& varName)
        : exists(true), isConstant(false), varName(varName), index(index)
    {
    }

    ThinTeslaParameter(size_t index, const std::string& varName, size_t fieldIndex)
        : exists(true), isConstant(false), varName(varName), isIndirection(true), fieldIndex(fieldIndex), index(index)
    {
    }

    bool isConstant = false;
    size_t constantValue = 0;
    std::string varName = "";
    bool isIndirection = false;
    size_t fieldIndex = 0;

    bool exists = false;

    size_t index = 0;
};

class ThinTeslaEventVisitor
{
  public:
    virtual ~ThinTeslaEventVisitor() {}
    virtual void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent&) = 0;
    virtual void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaFunction&) = 0;
    virtual void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaParametricFunction&) = 0;
    virtual void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaAssertionSite&) = 0;
};

#define VISITOR_ACCEPT                                                                                  \
    virtual void Accept(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEventVisitor& visitor) \
    {                                                                                                   \
        visitor.InstrumentEvent(M, assertion, *this);                                                   \
    }

class ThinTeslaEvent
{
  public:
    VISITOR_ACCEPT

    virtual ~ThinTeslaEvent(){};

    bool isOptional = false;
    bool isOR = false;
    bool isDeterministic = true;
    bool isBeforeAssertion = false;
    bool isInitial = false;

    bool IsEnd() { return successors.size() == 0; }
    bool IsFinal() // At least one of the successors is the end event.
    {
        for (auto& succ : successors)
            if (succ->IsEnd())
                return true;
        return false;
    }
    bool IsStart() { return id == 0; }
    bool IsInitial() // This is a successor of the first temporal bound.
    {
        return isInitial;
    }

    virtual bool IsAssertion() { return false; }

    virtual std::string GetInstrumentationTarget() { return ""; }
    virtual bool NeedsParametricInstrumentation() { return false; }

    virtual size_t GetMatchDataSize() { return 0; }
    virtual std::vector<ThinTeslaParameter> GetParameters() { return {}; }
    virtual ThinTeslaParameter GetReturnValue() { return ThinTeslaParameter(); }

    size_t id = 0;
    std::vector<std::shared_ptr<ThinTeslaEvent>> successors;
};

using ThinTeslaEventPtr = std::shared_ptr<ThinTeslaEvent>;

class ThinTeslaFunction : public ThinTeslaEvent
{
  public:
    ThinTeslaFunction(const std::string& functionName, bool callee = false)
        : functionName(functionName), calleeInstrumentation(callee)
    {
    }

    VISITOR_ACCEPT

    std::string GetInstrumentationTarget() { return functionName; }

    virtual ~ThinTeslaFunction(){};

    const std::string functionName;
    bool calleeInstrumentation;
};

class ThinTeslaAssertionSite : public ThinTeslaFunction
{
  public:
    ThinTeslaAssertionSite(const std::string& functionName, const std::string& filename, size_t lineNumber, size_t counter)
        : ThinTeslaFunction(functionName, false), filename(filename), line(lineNumber), counter(counter)
    {
    }

    bool IsAssertion() { return true; }

    VISITOR_ACCEPT

    std::string filename;
    size_t line;
    size_t counter;
};

class ThinTeslaParametricFunction : public ThinTeslaFunction
{
  public:
    ThinTeslaParametricFunction(const std::string& functionName, bool callee = false)
        : ThinTeslaFunction(functionName, callee)
    {
    }

    VISITOR_ACCEPT

    bool NeedsParametricInstrumentation() { return true; }

    void AddParameter(ThinTeslaParameter param)
    {
        if (!param.isConstant)
            isDeterministic = false;
        params.push_back(param);
    }

    void AddReturnValue(ThinTeslaParameter val)
    {
        if (!val.isConstant)
            isDeterministic = false;
        returnValue = val;
    }

    size_t GetMatchDataSize()
    {
        size_t size = 0;
        for (auto& param : params)
        {
            if (!param.isConstant)
                size++;
        }

        if (returnValue.exists && !returnValue.isConstant)
            size++;

        return size;
    }

    std::vector<ThinTeslaParameter> GetParameters() { return params; };
    ThinTeslaParameter GetReturnValue() { return returnValue; };

    std::vector<ThinTeslaParameter> params;
    ThinTeslaParameter returnValue;

    size_t numTotalParams = 0;
};

struct ThinTeslaAssertion
{
  public:
    ThinTeslaAssertion(size_t id)
        : id(id)
    {
    }

    bool IsLinked()
    {
        return linkedAssertions.size() > 0;
    }

    bool IsLinkMaster()
    {
        return linkMaster;
    }

    std::vector<ThinTeslaEventPtr> events;
    size_t id = 0;

    bool isDeterministic = true;
    bool isThreadLocal = false;

    std::string assertionFilename;
    size_t assertionLine, assertionCounter;
    std::set<std::string> affectedFunctions;

    std::vector<std::shared_ptr<ThinTeslaAssertion>> linkedAssertions;
    bool linkMaster = false;
};

using AssertionPtr = std::shared_ptr<ThinTeslaAssertion>;

class ThinTeslaAssertionBuilder
{
  public:
    ThinTeslaAssertionBuilder(const tesla::Manifest& manifest, const tesla::Usage* automaton) : automaton(automaton)
    {
        const tesla::Automaton* aut = manifest.FindAutomatonSafe(automaton->identifier());
        assert(aut != nullptr);
        desc = &aut->assertion;

        BuildAssertion();

        assert(assertions.size() > 0);

        if (assertions.size() > 1)
        {
            assert(multipleAutomata);
        }

        SetAssertionGlobalProperties();
        LinkEvents();
        DetermineProperties();
    }

    std::vector<AssertionPtr> GetAssertions()
    {
        return assertions;
    }

    bool HasMultipleAssertions()
    {
        return assertions.size() > 1;
    }

  private:
    void BuildAssertion();

    void AddAssertion()
    {
        AssertionPtr assertion = std::make_shared<ThinTeslaAssertion>(assertions.size());
        assertions.push_back(assertion);
        currentAssertion = assertion;

        bool inOR = currentlyInOR;
        currentlyInOR = false;
        ConvertExp(*entryBound);
        currentlyInOR = inOR;
    }

    void SetAssertionGlobalProperties()
    {
        for (auto assertion : assertions)
        {
            assertion->isThreadLocal = isThreadLocal;
            assertion->assertionFilename = assertionFilename;
            assertion->assertionLine = assertionLine;
            assertion->assertionCounter = assertionCounter;
            assertion->affectedFunctions = affectedFunctions;

            currentAssertion = assertion;
            bool inOR = currentlyInOR;
            currentlyInOR = false;
            ConvertExp(*exitBound);
            currentlyInOR = inOR;
        }

        for (size_t i = 0; i < assertions.size(); ++i)
        {
            if (i == assertions.size() - 1)
            {
                assertions[i]->linkMaster = true;
            }

            for (size_t j = 0; j < assertions.size(); ++j)
            {
                if (i != j)
                {
                    assertions[i]->linkedAssertions.push_back(assertions[j]);
                }
            }
        }
    }

    void DetermineProperties()
    {
        for (auto assertion : assertions)
        {
            bool beforeAssertion = true;

            ThinTeslaEventPtr start = assertion->events[0];

            for (auto event : assertion->events)
            {
                if (event->IsAssertion())
                    beforeAssertion = false;

                event->isBeforeAssertion = beforeAssertion;

                if (!event->isDeterministic)
                {
                    assertion->isDeterministic = false;
                }

                for (auto& startSucc : start->successors)
                {
                    if (startSucc == event)
                        event->isInitial = true;
                }
            }

            if (beforeAssertion)
                assert(false && "Automaton does not contain an assertion site");
        }
    }

    void AddEvent(ThinTeslaEventPtr event)
    {
        if (currentAssertion == nullptr)
            AddAssertion();

        if (currentlyInOR)
        {
            event->isOR = true;
        }

        currentAssertion->events.push_back(event);
        currentAssertion->events[currentAssertion->events.size() - 1]->id = currentAssertion->events.size() - 1;
    }

    void SetLastEventOptional()
    {
        assert(currentAssertion != nullptr && currentAssertion->events.size() > 0);

        currentAssertion->events[currentAssertion->events.size() - 1]->isOptional = true;
    }

    void LinkEvents();

    void ConvertExp(const tesla::Expression& exp);
    void ConvertBoolean(const tesla::BooleanExpr& exp);
    void ConvertAssertionSite(const tesla::AssertionSite& site);
    void ConvertFunction(const tesla::Expression& fun);
    bool CheckBooleanSubexpressions(const tesla::BooleanExpr& exp);

    void AddArgumentToParametricEvent(std::shared_ptr<ThinTeslaParametricFunction> event, const tesla::Argument& arg, bool returnValue = false);

    const tesla::Usage* automaton;
    const tesla::AutomatonDescription* desc = nullptr;

    bool currentlyInOR = false;

    std::vector<AssertionPtr> assertions;
    AssertionPtr currentAssertion = nullptr;

    bool multipleAutomata = false;
    const tesla::Expression* entryBound = nullptr;
    const tesla::Expression* exitBound = nullptr;

    bool isThreadLocal = false;
    std::string assertionFilename;
    size_t assertionLine, assertionCounter;
    std::set<std::string> affectedFunctions;
};