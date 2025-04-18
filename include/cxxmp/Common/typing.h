#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace cxxmp::typing {

template < typename _Fn >
concept Callable = requires(_Fn f) {
    {
        f()
    } -> ::std::same_as< void >;
};

template < typename _Key >
concept Hashable = requires(_Key key) {
    {
        std::hash< _Key >{}(key)
    } -> std::same_as< std::size_t >;
};

using u8  = ::std::uint8_t;
using u16 = ::std::uint16_t;
using u32 = ::std::uint32_t;
using i8  = ::std::int8_t;
using i16 = ::std::int16_t;
using i32 = ::std::int32_t;
using i64 = ::std::int64_t;
using u64 = ::std::uint64_t;
using f64 = double;
using f32 = float;

template < typename T >
using Result = ::std::pair< bool, T >;

template < typename T >
using Option = ::std::optional< T >;

constexpr auto None = ::std::nullopt;

// unique_ptr
template < typename T >
using Box = ::std::unique_ptr< T >;

// Ref Count (shared_ptr)
template < typename T >
using Rc = ::std::shared_ptr< T >;

// Ref Count (weak_ptr)
template < typename T >
using Weak = ::std::weak_ptr< T >;

// Atomic Ref Count (atomic_shared_ptr)
template < typename T >
using Arc = ::std::atomic< ::std::shared_ptr< T > >;

} // namespace cxxmp::typing
