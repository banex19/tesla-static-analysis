#pragma once
#include <string>

namespace tesla
{
struct CachedFile
{
    std::string filename;
    size_t timestamp;
};
} // namespace tesla