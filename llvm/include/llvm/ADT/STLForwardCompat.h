//===- STLForwardCompat.h - Library features from future STLs ------C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains library features backported from future STL versions.
///
/// These should be replaced with their STL counterparts as the C++ version LLVM
/// is compiled with is updated.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_STLFORWARDCOMPAT_H
#define LLVM_ADT_STLFORWARDCOMPAT_H

#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>

namespace llvm {

//===----------------------------------------------------------------------===//
//     Features from C++20
//===----------------------------------------------------------------------===//

template <typename T>
struct remove_cvref // NOLINT(readability-identifier-naming)
{
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <typename T>
using remove_cvref_t // NOLINT(readability-identifier-naming)
    = typename llvm::remove_cvref<T>::type;

// TODO: Remove this in favor of std::type_identity<T> once we switch to C++23.
template <typename T>
struct type_identity // NOLINT(readability-identifier-naming)
{
  using type = T;
};

// TODO: Remove this in favor of std::type_identity_t<T> once we switch to
// C++23.
template <typename T>
using type_identity_t // NOLINT(readability-identifier-naming)
    = typename llvm::type_identity<T>::type;

/// C++20 constexpr invoke. This uses `std::apply` (constexpr in C++17) to
/// achieve constexpr invocation.
template <typename FnT, typename... ArgsT>
constexpr std::invoke_result_t<FnT, ArgsT...>
invoke(FnT &&Fn, ArgsT &&...Args) { // NOLINT(readability-identifier-naming)
  return std::apply(std::forward<FnT>(Fn),
                    std::forward_as_tuple(std::forward<ArgsT>(Args)...));
}

//===----------------------------------------------------------------------===//
//     Features from C++23
//===----------------------------------------------------------------------===//

// TODO: Remove this in favor of std::optional<T>::transform once we switch to
// C++23.
template <typename T, typename Function>
auto transformOptional(const std::optional<T> &O, const Function &F)
    -> std::optional<decltype(F(*O))> {
  if (O)
    return F(*O);
  return std::nullopt;
}

// TODO: Remove this in favor of std::optional<T>::transform once we switch to
// C++23.
template <typename T, typename Function>
auto transformOptional(std::optional<T> &&O, const Function &F)
    -> std::optional<decltype(F(*std::move(O)))> {
  if (O)
    return F(*std::move(O));
  return std::nullopt;
}

/// Returns underlying integer value of an enum. Backport of C++23
/// std::to_underlying.
template <typename Enum>
[[nodiscard]] constexpr std::underlying_type_t<Enum> to_underlying(Enum E) {
  return static_cast<std::underlying_type_t<Enum>>(E);
}

// A tag for constructors accepting ranges.
struct from_range_t {
  explicit from_range_t() = default;
};
inline constexpr from_range_t from_range{};

//===----------------------------------------------------------------------===//
//     Bind functions from C++20 / C++23 / C++26
//===----------------------------------------------------------------------===//

namespace detail {
// Tag for constructing with a runtime callable.
struct RuntimeFnTag {};
// Tag for constructing with a compile-time constant callable.
struct ConstantFnTag {};

/// Stores a callable as a data member.
template <typename FnT> struct FnHolder {
  FnT Fn;

  template <typename FnArgT>
  constexpr explicit FnHolder(FnArgT &&F) : Fn(std::forward<FnArgT>(F)) {}

  constexpr FnT &get() { return Fn; }
  constexpr const FnT &get() const { return Fn; }
};

/// Holds a compile-time constant callable (empty storage).
template <auto ConstFn> struct FnConstant {
  constexpr decltype(auto) get() const { return ConstFn; }
};

// Storage class for bind_front/bind_back that properly handles const/non-const
// qualification of the wrapper when invoking the stored callable.
// If BindFront is true, bound args are prepended; otherwise appended.
// FnStorageT is either FnHolder<FnT> (runtime) or FnConstant<ConstFn>.
template <bool BindFront, typename BoundArgsTupleT, typename FnStorageT,
          typename IndicesT>
class BindStorage;

template <bool BindFront, typename BoundArgsTupleT, typename FnStorageT,
          size_t... Indices>
class BindStorage<BindFront, BoundArgsTupleT, FnStorageT,
                  std::index_sequence<Indices...>> {
  BoundArgsTupleT BoundArgs;
  // This may be empty for const functions, hence the `no_unique_address`.
  [[no_unique_address]] FnStorageT FnStorage;

public:
  // Constructor for FnHolder (runtime callable).
  template <typename FnArgT, typename... BoundArgsArgT>
  constexpr BindStorage(RuntimeFnTag, FnArgT &&F, BoundArgsArgT &&...Args)
      : BoundArgs(std::forward<BoundArgsArgT>(Args)...),
        FnStorage(std::forward<FnArgT>(F)) {}

  // Constructor for FnConstant (compile-time callable).
  template <typename... BoundArgsArgT>
  constexpr BindStorage(ConstantFnTag, BoundArgsArgT &&...Args)
      : BoundArgs(std::forward<BoundArgsArgT>(Args)...), FnStorage() {}

  template <typename... CallArgsT>
  constexpr auto operator()(CallArgsT &&...CallArgs) {
    if constexpr (BindFront)
      return llvm::invoke(FnStorage.get(), std::get<Indices>(BoundArgs)...,
                          std::forward<CallArgsT>(CallArgs)...);
    else
      return llvm::invoke(FnStorage.get(), std::forward<CallArgsT>(CallArgs)...,
                          std::get<Indices>(BoundArgs)...);
  }

  template <typename... CallArgsT>
  constexpr auto operator()(CallArgsT &&...CallArgs) const {
    if constexpr (BindFront)
      return llvm::invoke(FnStorage.get(), std::get<Indices>(BoundArgs)...,
                          std::forward<CallArgsT>(CallArgs)...);
    else
      return llvm::invoke(FnStorage.get(), std::forward<CallArgsT>(CallArgs)...,
                          std::get<Indices>(BoundArgs)...);
  }
};
} // end namespace detail

/// C++20 bind_front. Prepends bound arguments to the callable. All bind
/// arguments and the callable are forwarded and *stored* by value. If you would
/// like to pass by reference, use `std::ref` or `std::cref`.
template <typename FnT, typename... BindArgsT>
constexpr auto bind_front(FnT &&Fn, // NOLINT(readability-identifier-naming)
                          BindArgsT &&...BindArgs) {
  return detail::BindStorage</*BindFront=*/true,
                             std::tuple<std::decay_t<BindArgsT>...>,
                             detail::FnHolder<std::decay_t<FnT>>,
                             std::index_sequence_for<BindArgsT...>>(
      detail::RuntimeFnTag{}, std::forward<FnT>(Fn),
      std::forward<BindArgsT>(BindArgs)...);
}

/// C++26 bind_front with compile-time callable. Prepends bound arguments.
/// Bound arguments are forwarded and *stored* by value.
template <auto ConstFn, typename... BindArgsT>
constexpr auto
bind_front(BindArgsT &&...BindArgs) { // NOLINT(readability-identifier-naming)
  if constexpr (std::is_pointer_v<decltype(ConstFn)> ||
                std::is_member_pointer_v<decltype(ConstFn)>)
    static_assert(ConstFn != nullptr);

  return detail::BindStorage<
      /*BindFront=*/true, std::tuple<std::decay_t<BindArgsT>...>,
      detail::FnConstant<ConstFn>, std::index_sequence_for<BindArgsT...>>(
      detail::ConstantFnTag{}, std::forward<BindArgsT>(BindArgs)...);
}

/// C++23 bind_back. Appends bound arguments to the callable. All bind
/// arguments and the callable are forwarded and *stored* by value. If you would
/// like to pass by reference, use `std::ref` or `std::cref`.
template <typename FnT, typename... BindArgsT>
constexpr auto bind_back(FnT &&Fn, // NOLINT(readability-identifier-naming)
                         BindArgsT &&...BindArgs) {
  return detail::BindStorage</*BindFront=*/false,
                             std::tuple<std::decay_t<BindArgsT>...>,
                             detail::FnHolder<std::decay_t<FnT>>,
                             std::index_sequence_for<BindArgsT...>>(
      detail::RuntimeFnTag{}, std::forward<FnT>(Fn),
      std::forward<BindArgsT>(BindArgs)...);
}

/// C++26 bind_back with compile-time callable. Appends bound arguments.
/// Bound arguments are forwarded and *stored* by value.
template <auto ConstFn, typename... BindArgsT>
constexpr auto
bind_back(BindArgsT &&...BindArgs) { // NOLINT(readability-identifier-naming)
  if constexpr (std::is_pointer_v<decltype(ConstFn)> ||
                std::is_member_pointer_v<decltype(ConstFn)>)
    static_assert(ConstFn != nullptr);

  return detail::BindStorage<
      /*BindFront=*/false, std::tuple<std::decay_t<BindArgsT>...>,
      detail::FnConstant<ConstFn>, std::index_sequence_for<BindArgsT...>>(
      detail::ConstantFnTag{}, std::forward<BindArgsT>(BindArgs)...);
}
} // namespace llvm

#endif // LLVM_ADT_STLFORWARDCOMPAT_H
