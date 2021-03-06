#pragma once

#include <atomic>
#include <functional>
#include <mutex>

#include "util/type_traits.hpp"

namespace otto::util {

  /// Wrap any type in a mutex, and guarantee that it is locked on every access.
  template<typename T>
  class locked {
    T contents;

  public:
    template<typename... Args>
    locked(Args&&... args) : contents{std::forward<Args>(args)...}
    {}

    /// Direct access to the mutex to do manual locking
    std::mutex mutex;

    /// Use the wrapped value while locked
    /// Invoke `f` with the wrapped value as the only parameter
    ///
    /// \param f Invocable as `f(T)`
    /// \returns The result of calling `f(t)`
    /// \requires `Func` is invocable as `Func(T&)`
    template<typename Func>
    auto apply(Func&& f)
    {
      auto lock = std::unique_lock(mutex);
      return std::invoke(std::forward<Func>(f), contents);
    }

    /// Use the wrapped value while locked
    /// Invoke `f` with the wrapped value as the only parameter
    ///
    /// \param f Invocable as `f(T)`
    /// \returns The result of calling `f(t)`
    /// \requires `Func` is invocable as `Func(const T&)`
    template<typename Func>
    auto apply(Func&& f) const
    {
      auto lock = std::unique_lock(mutex);
      return std::invoke(std::forward<Func>(f), contents);
    }

    /// Get a non-locked reference to the value.
    ///
    /// This is of course highly unsafe, and defeats the purpose of the wrapper
    T& unsafe_access()
    {
      return contents;
    }


    /// Get a non-locked reference to the value.
    ///
    /// This is of course highly unsafe, and defeats the purpose of the wrapper
    const T& unsafe_access() const
    {
      return contents;
    }
  };

  struct clear_inner {
    template<typename T>
    void operator()(T& inner, T& outer)
    {
      inner.clear();
    }
  };
  struct clear_outer {
    template<typename T>
    void operator()(T& inner, T& outer)
    {
      outer.clear();
    }
  };


  /// Inner and outer buffers swapped atomically
  ///
  /// The inner buffer can always be accessed lock-free,
  /// the outer is locked on swap.
  ///
  /// \tparam T needs .clear(), which is called on swap
  template<typename T, typename AfterSwap = clear_outer>
  struct double_buffered {
    constexpr double_buffered(const T& inner,
                              const T& outer) noexcept(std::is_nothrow_copy_constructible_v<T>)
      : _store{{inner, outer}}
    {}

    constexpr double_buffered(T&& inner, const T& outer) noexcept(
      std::is_nothrow_copy_constructible_v<T>&& std::is_nothrow_move_constructible_v<T>)
      : _store{{std::move(inner), outer}}
    {}

    constexpr double_buffered(const T& inner, T&& outer = T{}) noexcept(
      std::is_nothrow_copy_constructible_v<T>&& std::is_nothrow_move_constructible_v<T>)
      : _store{{inner, std::move(outer)}}
    {}

    constexpr double_buffered(T&& inner = T{},
                              T&& outer = T{}) noexcept(std::is_nothrow_move_constructible_v<T>)
      : _store{{std::move(inner), std::move(outer)}}
    {}

    constexpr T& inner() noexcept
    {
      return _store[_inner_idx];
    }

    constexpr const T& inner() const noexcept
    {
      return _store[_inner_idx];
    }

    constexpr T& outer() noexcept
    {
      std::unique_lock lock(_outer_lock);
      return _store[(_inner_idx + 1) % 2];
    }

    constexpr const T& outer() const noexcept
    {
      std::unique_lock lock(_outer_lock);
      return _store[(_inner_idx + 1) % 2];
    }

    template<typename Func>
    constexpr decltype(auto) outer_locked(Func&& f)
    {
      std::unique_lock lock(_outer_lock);
      return std::invoke(std::forward<Func>(f), _store[(_inner_idx + 1) % 2]);
    }

    template<typename Func>
    constexpr decltype(auto) outer_locked(Func&& f) const
    // TODO: clang 5 does not implement this
    // noexcept(std::is_nothrow_invocable_v<Func, const T&>)
    {
      std::unique_lock lock(_outer_lock);
      return std::invoke(std::forward<Func>(f), _store[(_inner_idx + 1) % 2]);
    }

    constexpr void swap() noexcept
    {
      std::unique_lock lock(_outer_lock);
      _inner_idx = (_inner_idx + 1) % 2;
      AfterSwap{}(_store[_inner_idx % 2], _store[(_inner_idx + 1) % 2]);
    }

  private:
    std::array<T, 2> _store;
    std::atomic<char> _inner_idx = 0;
    std::mutex _outer_lock;
  };

} // namespace otto::util

//  LocalWords:  util
