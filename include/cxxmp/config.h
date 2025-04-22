#pragma once

#include <string_view>

namespace cxxmp {

namespace sys {

constexpr size_t CXXMP_PROC_COUNT = 8;

}

namespace versions {

constexpr std::string_view BUILD_TYPE      = "Release";
constexpr std::string_view PROGRAM_NAME    = "cxxmp";
constexpr std::string_view PROGRAM_VERSION = "0.0.1";
constexpr std::string_view GIT_HEAD        = "fd9f4748d4feed4706ce9bce1f5c566763fe2a37";
constexpr std::string_view GIT_BRANCH      = "main";
inline constexpr std::string_view VERSION_INFO_STRING =
  R"(Project: cxxmp v0.0.1
Build: Release
System: Darwin-24.4.0
Compiler: Clang 20.1.3
Git: fd9f4748d4feed4706ce9bce1f5c566763fe2a37 (main)
)";
} // namespace versions

} // namespace cxxmp
