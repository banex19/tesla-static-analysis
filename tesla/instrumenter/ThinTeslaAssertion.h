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

    bool isConstant = false;
    size_t constantValue = 0;
    std::string varName = "";

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
        if (true || !IsInstrumented())                                                                  \
            visitor.InstrumentEvent(M, assertion, *this);                                               \
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

    bool IsInstrumented() { return alreadyInstrumented; }
    void SetInstrumented() { alreadyInstrumented = true; }

    bool IsEnd() { return successors.size() == 0; }
    bool IsStart() { return id == 0; }

    virtual bool IsAssertion() { return false; }

    virtual std::string GetInstrumentationTarget() { return ""; }
    virtual bool NeedsParametricInstrumentation() { return false; }

    virtual size_t GetMatchDataSize() { return 0; }
    virtual std::vector<ThinTeslaParameter> GetParameters() { return {}; }
    virtual ThinTeslaParameter GetReturnValue() { return ThinTeslaParameter(); }

    size_t id = 0;
    std::vector<std::shared_ptr<ThinTeslaEvent>> successors;

  private:
    bool alreadyInstrumented = false;
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

class ThinTeslaAssertion
{
  public:
    ThinTeslaAssertion(const tesla::Manifest& manifest, const tesla::Usage* automaton) : automaton(automaton)
    {
        const tesla::Automaton* aut = manifest.FindAutomatonSafe(automaton->identifier());
        assert(aut != nullptr);
        desc = &aut->assertion;

        BuildAssertion();
        LinkEvents();
        DetermineProperties();
    }

    std::vector<ThinTeslaEventPtr> events;
    std::string assertionFilename;
    size_t assertionLine, assertionCounter;
    std::set<std::string> affectedFunctions;

    bool isDeterministic = true;
    bool isThreadLocal = false;

  private:
    void BuildAssertion();

    void DetermineProperties()
    {
        bool beforeAssertion = true;
        for (auto event : events)
        {
            if (event->IsAssertion())
                beforeAssertion = false;

            event->isBeforeAssertion = beforeAssertion;

            if (!event->isDeterministic)
            {
                isDeterministic = false;
            }
        }
    }

    void AddEvent(ThinTeslaEventPtr event)
    {
        if (isOR)
        {
            event->isOR = true;
        }

        events.push_back(event);
        events[events.size() - 1]->id = events.size() - 1;
    }

    void LinkEvents();

    void ConvertExp(const tesla::Expression& exp);
    void ConvertBoolean(const tesla::BooleanExpr& exp);
    void ConvertAssertionSite(const tesla::AssertionSite& site);
    void ConvertFunction(const tesla::Expression& fun);

    void AddArgumentToParametricEvent(std::shared_ptr<ThinTeslaParametricFunction> event, const tesla::Argument& arg, bool returnValue = false);

    const tesla::Usage* automaton;
    const tesla::AutomatonDescription* desc = nullptr;

    bool isOR = false;
};