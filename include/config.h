#pragma once

#include <string_view>

namespace cxxmp {

namespace versions {

constexpr std::string_view BUILD_TYPE      = "Release";
constexpr std::string_view PROGRAM_NAME    = "cxxmp";
constexpr std::string_view PROGRAM_VERSION = "0.0.1";
constexpr std::string_view GIT_HEAD        = "7e7d8d970922a007e31619d11485131b72751367";
constexpr std::string_view GIT_BRANCH      = "main";
inline constexpr std::string_view VERSION_INFO_STRING =
  R"(Project: cxxmp v0.0.1
Build: Release
System: Darwin-24.4.0
Compiler: Clang 20.1.3
Git: 7e7d8d970922a007e31619d11485131b72751367 (main)
)";
} // namespace versions

} // namespace cxxmp
