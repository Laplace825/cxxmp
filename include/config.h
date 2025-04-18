#pragma once

#include <string_view>

namespace cxxmp {

namespace versions {

constexpr std::string_view BUILD_TYPE      = "Release";
constexpr std::string_view PROGRAM_NAME    = "cxxmp";
constexpr std::string_view PROGRAM_VERSION = "0.0.1";
constexpr std::string_view GIT_HEAD        = "HEAD";
constexpr std::string_view GIT_BRANCH      = "HEAD";
inline constexpr std::string_view VERSION_INFO_STRING
  = "Project: cxxmp v0.0.1\n"
    "Build: Release\n"
    "System: Darwin-24.4.0\n"
    "Compiler: Clang 20.1.3\n"
    "Git: HEAD (HEAD)\n";
}

}
