/*
 * This file is derived from util_scope_guard.hpp in Atmosphere-libs
 * https://github.com/Atmosphere-NX/Atmosphere-libs
 *
 * Original work:
 * Copyright (c) Atmosphère-NX
 *
 * Modifications:
 * Copyright (c) 2026 ndeadly
 *  - Adapted ON_SCOPE_EXIT and related macros for use with libnx and removed Atmosphère-specific dependencies
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <utility>

#define NON_COPYABLE(cls) \
    cls(const cls&) = delete; \
    cls& operator=(const cls&) = delete

#define ALWAYS_INLINE_LAMBDA __attribute__((always_inline))
#define ALWAYS_INLINE inline __attribute__((always_inline))

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(pref) CONCATENATE(pref, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(pref) CONCATENATE(pref, __LINE__)
#endif

template<class F>
class ScopeGuard {
    NON_COPYABLE(ScopeGuard);
    private:
        F f;
        bool active;
    public:
        constexpr ALWAYS_INLINE ScopeGuard(F f) : f(std::move(f)), active(true) { }
        constexpr ALWAYS_INLINE ~ScopeGuard() { if (active) { f(); } }
        constexpr ALWAYS_INLINE void Cancel() { active = false; }

        constexpr ALWAYS_INLINE ScopeGuard(ScopeGuard&& rhs) : f(std::move(rhs.f)), active(rhs.active) {
            rhs.Cancel();
        }

        ScopeGuard &operator=(ScopeGuard&& rhs) = delete;
};

template<class F>
constexpr ALWAYS_INLINE ScopeGuard<F> MakeScopeGuard(F f) {
    return ScopeGuard<F>(std::move(f));
}

enum class ScopeGuardOnExit {};

template <typename F>
constexpr ALWAYS_INLINE ScopeGuard<F> operator+(ScopeGuardOnExit, F&& f) {
    return ScopeGuard<F>(std::forward<F>(f));
}

#define SCOPE_GUARD   ScopeGuardOnExit() + [&]() ALWAYS_INLINE_LAMBDA
#define ON_SCOPE_EXIT auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE_) = SCOPE_GUARD
