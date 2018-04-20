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

    bool IsInstrumented() { return alreadyInstrumented; }
    void SetInstrumented() { alreadyInstrumented = true; }

    bool IsEnd() { return successors.size() == 0; }
    bool IsStart() { return id == 0; }

    virtual size_t GetMatchDataSize() { return 0; }

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

    VISITOR_ACCEPT

    std::string filename;
    size_t line;
    size_t counter;
};

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
        : exists(true), isConstant(false), constantValue(constantValue), index(index)
    {
    }

    bool isConstant = false;
    size_t constantValue = 0;
    std::string varName = "";

    bool exists = false;

    size_t index = 0;
};

class ThinTeslaParametricFunction : public ThinTeslaFunction
{
  public:
    ThinTeslaParametricFunction(const std::string& functionName, bool callee = false)
        : ThinTeslaFunction(functionName, callee)
    {
    }

    VISITOR_ACCEPT

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

  private:
    void BuildAssertion();

    void DetermineProperties()
    {
        for (auto event : events)
        {
            if (!event->isDeterministic)
            {
                isDeterministic = false;
                break;
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

/*
class AutomatonParser
{
  public:
    static std::string GetStringIdentifier(const AutomatonDescription& desc, const std::string& customFilename = "")
    {
        return GetStringIdentifierFromIdentifier(desc.identifier(), customFilename);
    }

    static std::string GetStringIdentifier(const Usage& usage, const std::string& customFilename = "")
    {
        return GetStringIdentifierFromIdentifier(usage.identifier(), customFilename);
    }

    static std::set<std::string> GetAffectedFunctions(const AutomatonDescription& desc)
    {
        std::vector<const Expression*> exps;
        exps.push_back(&desc.expression());

        return ParseExpressionTree(exps);
    }

    static std::set<std::string> GetAffectedFunctions(const Usage& usage)
    {
        std::vector<const Expression*> exps;
        exps.push_back(&usage.beginning());
        exps.push_back(&usage.end());

        return ParseExpressionTree(exps);
    }

  private:
    static std::string GetStringIdentifierFromIdentifier(const Identifier& id, const std::string& customFilename)
    {
        std::string filename = customFilename == "" ? id.location().filename() : customFilename;
        return filename + "$" + std::to_string(id.location().line()) + "$" + std::to_string(id.location().counter());
    }

    static std::set<std::string> ParseExpressionTree(std::vector<const Expression*> exps)
    {
        std::set<std::string> affectedFunctions;

        while (exps.size() > 0)
        {
            const tesla::Expression* exp = exps.back();
            exps.pop_back();

            if (exp->type() == tesla::Expression_Type_BOOLEAN_EXPR)
            {
                for (const auto& subExps : exp->booleanexpr().expression())
                {
                    exps.push_back(&subExps);
                }
            }
            else if (exp->type() == tesla::Expression_Type_SEQUENCE)
            {
                for (const auto& subExps : exp->sequence().expression())
                {
                    exps.push_back(&subExps);
                }
            }
            else if (exp->type() == tesla::Expression_Type_FUNCTION)
            {
                affectedFunctions.insert(exp->function().function().name());
            }
        }

        return affectedFunctions;
    }
}; */