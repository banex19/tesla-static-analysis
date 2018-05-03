#include "ThinTeslaAssertion.h"

using namespace tesla;

std::vector<ThinTeslaEventPtr> GetOROptionalBlock(std::vector<ThinTeslaEventPtr>& events, size_t start)
{
    std::vector<ThinTeslaEventPtr> block;

    for (size_t i = start; i < events.size(); ++i)
    {
        if (events[i]->isOR || events[i]->isOptional)
        {
            block.push_back(events[i]);
        }
        else
            break;
    }

    return block;
}

void ThinTeslaAssertionBuilder::BuildAssertion()
{
    isThreadLocal = desc->context() == tesla::AutomatonDescription_Context_ThreadLocal;

    entryBound = &automaton->beginning();
    ConvertExp(desc->expression());
    exitBound = &automaton->end();
}

void ThinTeslaAssertionBuilder::LinkEvents()
{
    for (auto& assertion : assertions)
    {
        bool beforeAssertion = true;
        for (size_t i = 0; i < assertion->events.size() - 1; ++i)
        {
            auto event = assertion->events[i];
            auto next = assertion->events[i + 1];

            if (event->IsAssertion())
                beforeAssertion = false;

            event->isBeforeAssertion = beforeAssertion;

            if (next->isOR || next->isOptional)
            {
                auto block = GetOROptionalBlock(assertion->events, i + 1);

                for (auto& orEvent : block)
                {
                    event->successors.push_back(orEvent);
                }

                assert(i + 1 + block.size() < assertion->events.size());
                auto firstNonOR = assertion->events[i + 1 + block.size()];
                block.push_back(firstNonOR);

                for (size_t k = 0; k < block.size(); ++k)
                {
                    for (size_t n = k + 1; n < block.size(); ++n)
                    {
                        block[k]->successors.push_back(block[n]);
                    }
                }

                i = i + block.size() - 1;
                continue;
            }
            else
            {
                event->successors.push_back(next);
            }
        }
    }
}

void ThinTeslaAssertionBuilder::ConvertExp(const Expression& exp)
{
    if (exp.type() == tesla::Expression_Type_NULL_EXPR)
    {
        return;
    }
    else if (exp.type() == tesla::Expression_Type_SEQUENCE)
    {
        for (const auto& subExp : exp.sequence().expression())
        {
            ConvertExp(subExp);
        }
    }
    else if (exp.type() == tesla::Expression_Type_BOOLEAN_EXPR)
    {
        ConvertBoolean(exp.booleanexpr());
    }
    else if (exp.type() == tesla::Expression_Type_FUNCTION)
    {
        ConvertFunction(exp);
    }
    else if (exp.type() == tesla::Expression_Type_ASSERTION_SITE)
    {
        ConvertAssertionSite(exp.assertsite());
    }
    else
    {
        assert(false && "Expression type not supported");
    }
}

bool ThinTeslaAssertionBuilder::CheckBooleanSubexpressions(const BooleanExpr& exp)
{
    const auto& exps = exp.expression();
    if (exps.size() < 2)
        return false;

    bool anyIsSequence = false;
    for (const auto& exp : exps)
    {
        if (exp.type() == tesla::Expression_Type_SEQUENCE)
        {
            anyIsSequence = true;
            break;
        }
    }

    if (anyIsSequence) // If at least one subexpression is a sequence, then all must be sequences and this must be the top expression.
    {
        if (desc->expression().type() != Expression_Type_BOOLEAN_EXPR || desc->expression().booleanexpr() != exp)
        {
            return false;
        }

        for (const auto& exp : exps)
        {
            if (exp.type() != tesla::Expression_Type_SEQUENCE)
                return false;
        }

        multipleAutomata = true;
    }

    return true;
}

void ThinTeslaAssertionBuilder::ConvertBoolean(const BooleanExpr& exp)
{
    if (exp.operation() == tesla::BooleanExpr_Operation_BE_Or)
    {
        assert(!currentlyInOR);

        currentlyInOR = true;

        if (!CheckBooleanSubexpressions(exp))
        {
            assert(false && "Boolean expression of invalid sub expressions");
        }

        if (multipleAutomata)
            currentlyInOR = false;

        for (const auto& subExp : exp.expression())
        {
            if (multipleAutomata)
            {
                AddAssertion();
            }

            ConvertExp(subExp);
        }

        currentlyInOR = false;
    }
    else
        assert(false && "Boolean expression not supported");
}

void ThinTeslaAssertionBuilder::ConvertAssertionSite(const AssertionSite& site)
{
    const auto& location = site.location();

    std::shared_ptr<ThinTeslaAssertionSite> event =
        std::make_shared<ThinTeslaAssertionSite>(site.function().name(),
                                                 location.filename(), (size_t)location.line(), (size_t)location.counter());

    AddEvent(event);
    assertionFilename = location.filename();
    assertionLine = location.line();
    assertionCounter = location.counter();
}

void ThinTeslaAssertionBuilder::ConvertFunction(const Expression& fun)
{
    const FunctionEvent& function = fun.function();

    bool hasParameterizedArgs = false;
    for (const auto& arg : function.argument())
    {
        if (arg.type() != Argument_Type_Any)
        {
            hasParameterizedArgs = true;
            break;
        }
    }

    if (!hasParameterizedArgs)
    {
        if (function.has_expectedreturnvalue() && function.expectedreturnvalue().type() != Argument_Type_Any)
            hasParameterizedArgs = true;
    }

    if (hasParameterizedArgs)
    {
        std::shared_ptr<ThinTeslaParametricFunction> event =
            std::make_shared<ThinTeslaParametricFunction>(function.function().name(), function.context() == FunctionEvent_CallContext_Callee);

        for (const auto& arg : function.argument())
        {
            AddArgumentToParametricEvent(event, arg);
        }

        if (function.has_expectedreturnvalue())
        {
            AddArgumentToParametricEvent(event, function.expectedreturnvalue(), true);
        }

        AddEvent(event);
    }
    else
    {
        std::shared_ptr<ThinTeslaFunction> event =
            std::make_shared<ThinTeslaFunction>(function.function().name(), function.context() == FunctionEvent_CallContext_Callee);

        AddEvent(event);
    }

    affectedFunctions.insert(function.function().name());
}

void ThinTeslaAssertionBuilder::AddArgumentToParametricEvent(std::shared_ptr<ThinTeslaParametricFunction> event, const tesla::Argument& arg, bool returnValue)
{
    if (arg.type() == Argument_Type_Any)
    {
        if (!returnValue)
            event->numTotalParams++;
        return;
    }

    if (arg.type() != Argument_Type_Constant && arg.type() != Argument_Type_Variable && arg.type() != Argument_Type_Field)
    {
        llvm::errs() << "Argument type: " << arg.type() << "\n";
        assert(false && "Argument type not supported");
    }

    ThinTeslaParameter param;

    if (arg.type() == Argument_Type_Constant)
    {
        param = ThinTeslaParameter(event->numTotalParams, arg.value());
    }
    else if (arg.type() == Argument_Type_Variable)
    {
        param = ThinTeslaParameter(event->numTotalParams, arg.name());
    }
    else
    {
        param = ThinTeslaParameter(event->numTotalParams, arg.field().base().name(), arg.field().index());
    }

    if (!returnValue)
    {
        event->numTotalParams++;
        event->AddParameter(param);
    }
    else
    {
        event->AddReturnValue(param);
    }
}
