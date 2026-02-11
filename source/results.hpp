/*
 * This file is derived from results_common.hpp in Atmosphere-libs
 * https://github.com/Atmosphere-NX/Atmosphere-libs
 *
 * Original work:
 * Copyright (c) Atmosphère-NX
 *
 * Modifications:
 * Copyright (c) 2026 ndeadly
 *  - Adapted result macros for use with libnx and removed Atmosphère-specific dependencies
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
#include <switch.h>
#include <concepts>

#define RESULT_SUCCESS 0

constinit inline Result __TmpCurrentResultReference = RESULT_SUCCESS;

#define R_RETURN(res_expr)                                                                                                              \
    {                                                                                                                                   \
        const Result _tmp_r_throw_rc = (res_expr);                                                                                      \
        if constexpr (std::same_as<decltype(__TmpCurrentResultReference), Result &>) { __TmpCurrentResultReference = _tmp_r_throw_rc; } \
        return _tmp_r_throw_rc;                                                                                                         \
    }

#define R_SUCCEED() R_RETURN(RESULT_SUCCESS)

#define R_THROW(res_expr) R_RETURN(res_expr)

#define R_TRY(res_expr)                                                       \
    {                                                                         \
        if (const auto _tmp_r_try_rc = (res_expr); R_FAILED(_tmp_r_try_rc)) { \
            R_THROW(_tmp_r_try_rc);                                           \
        }                                                                     \
    }

#define R_ABORT_UNLESS(res_expr)                                              \
    {                                                                         \
        if (const auto _tmp_r_try_rc = (res_expr); R_FAILED(_tmp_r_try_rc)) { \
            diagAbortWithResult(_tmp_r_try_rc);                               \
        }                                                                     \
    }
