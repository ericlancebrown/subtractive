#pragma once

#include <tuple>

namespace libsubtractive::grbl
{
using VersionIndex = unsigned long;
using Subversion = char;
using VersionData = std::tuple<VersionIndex, VersionIndex, Subversion>;
}  // namespace libsubtractive::grbl
