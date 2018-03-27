#pragma once

#include <string>
#include <vector>

#include <tesla.pb.h>

using namespace tesla;

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
};