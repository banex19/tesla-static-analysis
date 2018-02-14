#include <string>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/raw_ostream.h>
#include "Debug.h"

#include "Utils.h"

void PanicIfError(std::error_code err)
{
    if (err)
        tesla::panic(err.message());
}

void OutputVerbose(const std::string &msg, bool verbose)
{
    if (verbose)
    {
        llvm::outs() << msg << "\n";
    }
}

void OutputWarning(const std::string &warning)
{
    llvm::errs() << "WARNING: " << warning << "\n";
}

void OutputAlways(const std::string &msg)
{
    llvm::outs() << msg << "\n";
}

std::string StringFromVector(const std::vector<std::string> &vec, const std::string &separator)
{
    std::string str;

    for (size_t i = 0; i < vec.size(); ++i)
    {
        str += vec[i];

        if (i != vec.size() - 1)
            str += separator + " ";
    }

    return str;
}

std::string SanitizeFilename(const std::string &filename)
{
    std::string newFilename = filename;
    std::replace(newFilename.begin(), newFilename.end(), '.', '-');
    std::replace(newFilename.begin(), newFilename.end(), '/', '_');
    std::replace(newFilename.begin(), newFilename.end(), '\\', '_');

    return newFilename;
}