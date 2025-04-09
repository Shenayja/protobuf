// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
#include "google/protobuf/port.h"

#include <stdio.h>
#include <stdlib.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include <gtest/gtest.h>
#include "absl/base/config.h"

// Must be included last
#include "absl/base/optimization.h"
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace internal {

int assume_var_for_test = 1;

TEST(PortDeathTest, ProtobufAssume) {
  PROTOBUF_ASSUME(assume_var_for_test == 1);
#ifdef GTEST_HAS_DEATH_TEST
#if defined(NDEBUG)
  // If NDEBUG is defined, then instead of reliably crashing, the code below
  // will assume a false statement. This is undefined behavior which will trip
  // up sanitizers.
  GTEST_SKIP() << "Can't test PROTOBUF_ASSUME()";
#else
  EXPECT_DEBUG_DEATH(
      PROTOBUF_ASSUME(assume_var_for_test == 2),
      "port_test\\.cc:.*Assumption failed: 'assume_var_for_test == 2'");
#endif
#endif
}

TEST(PortDeathTest, UnreachableTrapsOnDebugMode) {
#ifdef GTEST_HAS_DEATH_TEST
#if defined(NDEBUG)
  // In NDEBUG we crash with a UD instruction, so we don't get the "Assumption
  // failed" error.
  GTEST_SKIP() << "Can't test __builtin_unreachable()";
#elif ABSL_HAVE_BUILTIN(__builtin_FILE)
  EXPECT_DEATH(Unreachable(),
               "port_test\\.cc:.*Assumption failed: 'Unreachable'");
#else
  EXPECT_DEATH(Unreachable(), "Assumption failed: 'Unreachable'");
#endif
#endif
}

#ifdef GTEST_HAS_DEATH_TEST
TEST(PortDeathTest, PrefetchWorksForValidOptsAndDiagnosesInvalidOpts) {
  constexpr size_t kBufSize = ABSL_CACHELINE_SIZE * 100;
  std::byte buf[kBufSize];
  constexpr uintptr_t kOkOffset = kBufSize / 2;
  constexpr uintptr_t kJustBeyondOffset = kBufSize;
  constexpr uintptr_t kWrapAroundOffset =
      std::numeric_limits<uintptr_t>::max() - 1;
  // A prefetch of a guaranteed valid address (using lines).
  {
    static constexpr PrefetchOpts kOpts = {
        .num = {1, kLines},
        .from = {kOkOffset, kBytes},
    };
    EXPECT_NO_FATAL_FAILURE(Prefetch<kOpts>(buf));
  }
  // A prefetch of a guaranteed valid address (using bytes not wholly divisible
  // into lines).
  {
    static constexpr PrefetchOpts kOpts = {
        .num = {ABSL_CACHELINE_SIZE + ABSL_CACHELINE_SIZE / 2, kBytes},
        .from = {kOkOffset, kBytes},
    };
    EXPECT_NO_FATAL_FAILURE(Prefetch<kOpts>(buf));
  }
  // Hit the tail for-loop constexpr path (barely).
  {
    static constexpr PrefetchOpts kOpts = {
        .num = {20, kLines},
        .from = {kOkOffset, kBytes},
    };
    EXPECT_NO_FATAL_FAILURE(Prefetch<kOpts>(buf));
  }
  // Hit the tail for-loop constexpr path (a lot).
  {
    static constexpr PrefetchOpts kOpts = {
        .num = {100, kLines},
        .from = {kOkOffset, kBytes},
    };
    EXPECT_NO_FATAL_FAILURE(Prefetch<kOpts>(buf));
  }
  // Pretend `buf` actually holds an array of strings.
  {
    static constexpr PrefetchOpts kOpts = {
        .num = {2, kObjects},
        .from = {kOkOffset, kBytes},
    };
    // Need a lambda because EXPECT_NO_FATAL_FAILURE can't handle commas.
    const auto wrapper = [](const void* ptr) {
      Prefetch<kOpts, std::string>(ptr);
    };
    EXPECT_NO_FATAL_FAILURE(wrapper(buf));
  }
  // A prefetch of an invalid address (beyond the end of the buffer) is valid
  // and is just a no-op.
  {
    static constexpr PrefetchOpts kOpts = {
        .num = {2, kLines},
        .from = {kJustBeyondOffset, kBytes},
    };
    EXPECT_NO_FATAL_FAILURE(Prefetch<kOpts>(buf));
  }
  // A prefetch with a wrap-around offset should be diagnosed in debug builds.
  {
    static constexpr PrefetchOpts kOpts = {
        .num = {2, kLines},
        .from = {kWrapAroundOffset, kBytes},
    };
#ifndef NDEBUG
    // We want to catch overflows in address + offset computation in debug
    // builds, because that in itself is a UB...
    EXPECT_DEATH(Prefetch<kOpts>(buf), "assertion failed.*");
#else
    // ... but that UB doesn't matter for prefetching, though, because the
    // prefetch instruction is just a no-op for invalid addresses. So we turn
    // off the UB check in opt builds.
    EXPECT_NO_FATAL_FAILURE(Prefetch<kOpts>(buf));
#endif
  }
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
