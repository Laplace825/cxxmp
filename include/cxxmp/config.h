#pragma once

#include <string_view>

namespace cxxmp {

namespace sys {

constexpr size_t CXXMP_PROC_COUNT = 8;

}

namespace versions {

constexpr std::string_view BUILD_TYPE      = "Release";
constexpr std::string_view PROGRAM_NAME    = "cxxmp";
constexpr std::string_view PROGRAM_VERSION = "0.2.1";
constexpr std::string_view GIT_HEAD        = "513fbf7dae89083dff9a9b93ac66ada345b75794";
constexpr std::string_view GIT_BRANCH      = "main";
inline constexpr std::string_view VERSION_INFO_STRING =
  R"(Project: cxxmp v0.2.1
Build: Release
System: Darwin-24.4.0
Compiler: Clang 20.1.4
Git: 513fbf7dae89083dff9a9b93ac66ada345b75794 (main)
)";
} // namespace versions

} // namespace cxxmp
