#pragma once
#include "Debug.h"
#include <algorithm>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <vector>
#include <set>

void PanicIfError(std::error_code err);

void OutputVerbose(const std::string& msg, bool verbose = false);

void OutputWarning(const std::string& warning);

void OutputAlways(const std::string& msg);

std::string StringFromVector(const std::vector<std::string>& vec, const std::string& separator = ", ");
std::string StringFromSet(const std::set<std::string>& s, const std::string& separator = ", ");

std::string SanitizeFilename(const std::string& filename);