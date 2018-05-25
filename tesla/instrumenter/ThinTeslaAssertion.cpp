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

    auto automata = GetTopLevelAutomata();
    if (automata.size() > 1)
        multipleAutomata = true;

    entryBound = &automaton->beginning();

    for (auto& automaton : automata)
    {
        AddAssertion();
        ConvertExp(*automaton);
    }

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

                if (next->isOptional) // If this is an optional block, we could skip it entirely.
                {
                    event->successors.push_back(firstNonOR);
                }

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

void ThinTeslaAssertionBuilder::ConvertExp(const tesla::Expression& exp)
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

std::vector<const tesla::Expression*> ThinTeslaAssertionBuilder::GetTopLevelAutomata()
{
    std::vector<const tesla::Expression*> automata;

    bool multiAutomata = false;
    if (desc->expression().type() == Expression_Type_BOOLEAN_EXPR)
    {
        const auto& exp = desc->expression().booleanexpr();
        const auto& exps = exp.expression();
        for (const auto& exp : exps)
        {
            if (exp.type() == tesla::Expression_Type_SEQUENCE)
            {
                multiAutomata = true;
                break;
            }
        }

        GetTopLevelAutomataRec(exp, automata);
    }

    if (!multiAutomata)
    {
        automata.push_back(&desc->expression());
    }

    return automata;
}

void ThinTeslaAssertionBuilder::GetTopLevelAutomataRec(const BooleanExpr& exp, std::vector<const tesla::Expression*>& automata)
{
    const auto& exps = exp.expression();
    for (const auto& sub : exps)
    {
        if (sub.type() == tesla::Expression_Type_SEQUENCE)
        {
            automata.push_back(&sub);
        }
        else if (sub.type() == tesla::Expression_Type_BOOLEAN_EXPR)
        {
            GetTopLevelAutomataRec(sub.booleanexpr(), automata);
        }
        else
        {
            assert(false && "Expected top-level automaton");
        }
    }
}

void ThinTeslaAssertionBuilder::ConvertBoolean(const BooleanExpr& exp)
{
    if (exp.operation() == tesla::BooleanExpr_Operation_BE_Or)
    {
        assert(!currentlyInOR);

        currentlyInOR = true;

        for (const auto& subExp : exp.expression())
        {
            if (subExp.type() == tesla::Expression_Type_SEQUENCE)
            {
                assert(false && "Cannot have a sequence inside a sequence");
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

void ThinTeslaAssertionBuilder::ConvertFunction(const tesla::Expression& fun)
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

    if (fun.isoptional())
        SetLastEventOptional();

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
