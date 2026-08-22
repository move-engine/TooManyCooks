// Copyright (c) 2023-2026 Logan McDougall
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

// Define any of the following function-like macros before including TMC to
// route coroutine-frame and owned coro_functor allocations through a custom
// allocator:
//
//   TMC_ALLOCATE(Size)
//   TMC_ALLOCATE_ALIGNED(Size, Alignment)
//   TMC_DEALLOCATE(Ptr, Size)
//   TMC_DEALLOCATE_ALIGNED(Ptr, Size, Alignment)
//
// Allocation functions must either return non-null storage or terminate. TMC
// terminates if a custom allocation function returns nullptr. Each allocation
// and its matching deallocation must use the same allocator.

#include "tmc/detail/compat.hpp"

#include <cstddef>
#include <exception>
#include <new>

namespace tmc {
namespace detail {

inline void* allocate(std::size_t Size) noexcept {
#ifdef TMC_ALLOCATE
  void* result = TMC_ALLOCATE(Size);
#else
  void* result = ::operator new(Size);
#endif
  if (result == nullptr) {
    std::terminate();
  }
  return result;
}

inline void* allocate_aligned(std::size_t Size, std::size_t Alignment) noexcept {
#ifdef TMC_ALLOCATE_ALIGNED
  void* result = TMC_ALLOCATE_ALIGNED(Size, Alignment);
#else
  void* result = ::operator new(Size, std::align_val_t{Alignment});
#endif
  if (result == nullptr) {
    std::terminate();
  }
  return result;
}

inline void deallocate(void* Ptr, std::size_t Size) noexcept {
#ifdef TMC_DEALLOCATE
  TMC_DEALLOCATE(Ptr, Size);
#elif TMC_SIZED_DEALLOCATION
  ::operator delete(Ptr, Size);
#else
  static_cast<void>(Size);
  ::operator delete(Ptr);
#endif
}

inline void deallocate_aligned(
  void* Ptr, std::size_t Size, std::size_t Alignment
) noexcept {
#ifdef TMC_DEALLOCATE_ALIGNED
  TMC_DEALLOCATE_ALIGNED(Ptr, Size, Alignment);
#elif TMC_SIZED_DEALLOCATION
  ::operator delete(Ptr, Size, std::align_val_t{Alignment});
#else
  static_cast<void>(Size);
  ::operator delete(Ptr, std::align_val_t{Alignment});
#endif
}

} // namespace detail
} // namespace tmc
