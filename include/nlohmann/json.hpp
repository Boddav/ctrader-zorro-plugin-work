//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

/****************************************************************************\
 * Note on documentation: The source files contain links to the online      *
 * documentation of the public API at https://json.nlohmann.me. This URL    *
 * contains the most recent documentation and should also be applicable to  *
 * previous versions; documentation for deprecated functions is not         *
 * removed, but marked deprecated. See "Generate documentation" section in  *
 * file docs/README.md.                                                     *
\****************************************************************************/

#ifndef INCLUDE_NLOHMANN_JSON_HPP_
#define INCLUDE_NLOHMANN_JSON_HPP_

#include <algorithm> // all_of, find, for_each
#include <cstddef> // nullptr_t, ptrdiff_t, size_t
#include <functional> // hash, less
#include <initializer_list> // initializer_list
#ifndef JSON_NO_IO
    #include <iosfwd> // istream, ostream
#endif  // JSON_NO_IO
#include <iterator> // random_access_iterator_tag
#include <memory> // unique_ptr
#include <string> // string, stoi, to_string
#include <utility> // declval, forward, move, pair, swap
#include <vector> // vector

// #include <nlohmann/adl_serializer.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



#include <utility>

// #include <nlohmann/detail/abi_macros.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



// This file contains all macro definitions affecting or depending on the ABI

#ifndef JSON_SKIP_LIBRARY_VERSION_CHECK
    #if defined(NLOHMANN_JSON_VERSION_MAJOR) && defined(NLOHMANN_JSON_VERSION_MINOR) && defined(NLOHMANN_JSON_VERSION_PATCH)
        #if NLOHMANN_JSON_VERSION_MAJOR != 3 || NLOHMANN_JSON_VERSION_MINOR != 12 || NLOHMANN_JSON_VERSION_PATCH != 0
            #warning "Already included a different version of the library!"
        #endif
    #endif
#endif

#define NLOHMANN_JSON_VERSION_MAJOR 3   // NOLINT(modernize-macro-to-enum)
#define NLOHMANN_JSON_VERSION_MINOR 12  // NOLINT(modernize-macro-to-enum)
#define NLOHMANN_JSON_VERSION_PATCH 0   // NOLINT(modernize-macro-to-enum)

#ifndef JSON_DIAGNOSTICS
    #define JSON_DIAGNOSTICS 0
#endif

#ifndef JSON_DIAGNOSTIC_POSITIONS
    #define JSON_DIAGNOSTIC_POSITIONS 0
#endif

#ifndef JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
    #define JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON 0
#endif

#if JSON_DIAGNOSTICS
    #define NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS _diag
#else
    #define NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS
#endif

#if JSON_DIAGNOSTIC_POSITIONS
    #define NLOHMANN_JSON_ABI_TAG_DIAGNOSTIC_POSITIONS _dp
#else
    #define NLOHMANN_JSON_ABI_TAG_DIAGNOSTIC_POSITIONS
#endif

#if JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
    #define NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON _ldvcmp
#else
    #define NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON
#endif

#ifndef NLOHMANN_JSON_NAMESPACE_NO_VERSION
    #define NLOHMANN_JSON_NAMESPACE_NO_VERSION 0
#endif

// Construct the namespace ABI tags component
#define NLOHMANN_JSON_ABI_TAGS_CONCAT_EX(a, b, c) json_abi ## a ## b ## c
#define NLOHMANN_JSON_ABI_TAGS_CONCAT(a, b, c) \
    NLOHMANN_JSON_ABI_TAGS_CONCAT_EX(a, b, c)

#define NLOHMANN_JSON_ABI_TAGS                                       \
    NLOHMANN_JSON_ABI_TAGS_CONCAT(                                   \
            NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS,                       \
            NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON, \
            NLOHMANN_JSON_ABI_TAG_DIAGNOSTIC_POSITIONS)

// Construct the namespace version component
#define NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT_EX(major, minor, patch) \
    _v ## major ## _ ## minor ## _ ## patch
#define NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT(major, minor, patch) \
    NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT_EX(major, minor, patch)

#if NLOHMANN_JSON_NAMESPACE_NO_VERSION
#define NLOHMANN_JSON_NAMESPACE_VERSION
#else
#define NLOHMANN_JSON_NAMESPACE_VERSION                                 \
    NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT(NLOHMANN_JSON_VERSION_MAJOR, \
                                           NLOHMANN_JSON_VERSION_MINOR, \
                                           NLOHMANN_JSON_VERSION_PATCH)
#endif

// Combine namespace components
#define NLOHMANN_JSON_NAMESPACE_CONCAT_EX(a, b) a ## b
#define NLOHMANN_JSON_NAMESPACE_CONCAT(a, b) \
    NLOHMANN_JSON_NAMESPACE_CONCAT_EX(a, b)

#ifndef NLOHMANN_JSON_NAMESPACE
#define NLOHMANN_JSON_NAMESPACE               \
    nlohmann::NLOHMANN_JSON_NAMESPACE_CONCAT( \
            NLOHMANN_JSON_ABI_TAGS,           \
            NLOHMANN_JSON_NAMESPACE_VERSION)
#endif

#ifndef NLOHMANN_JSON_NAMESPACE_BEGIN
#define NLOHMANN_JSON_NAMESPACE_BEGIN                \
    namespace nlohmann                               \
    {                                                \
    inline namespace NLOHMANN_JSON_NAMESPACE_CONCAT( \
                NLOHMANN_JSON_ABI_TAGS,              \
                NLOHMANN_JSON_NAMESPACE_VERSION)     \
    {
#endif

#ifndef NLOHMANN_JSON_NAMESPACE_END
#define NLOHMANN_JSON_NAMESPACE_END                                     \
    }  /* namespace (inline namespace) NOLINT(readability/namespace) */ \
    }  // namespace nlohmann
#endif

// #include <nlohmann/detail/conversions/from_json.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



#include <algorithm> // transform
#include <array> // array
#include <forward_list> // forward_list
#include <iterator> // inserter, front_inserter, end
#include <map> // map
#include <string> // string
#include <tuple> // tuple, make_tuple
#include <type_traits> // is_arithmetic, is_same, is_enum, underlying_type, is_convertible
#include <unordered_map> // unordered_map
#include <utility> // pair, declval
#include <valarray> // valarray

// #include <nlohmann/detail/exceptions.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



#include <cstddef> // nullptr_t
#include <exception> // exception
#if JSON_DIAGNOSTICS
    #include <numeric> // accumulate
#endif
#include <stdexcept> // runtime_error
#include <string> // to_string
#include <vector> // vector

// #include <nlohmann/detail/value_t.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



#include <array> // array
#include <cstddef> // size_t
#include <cstdint> // uint8_t
#include <string> // string

// #include <nlohmann/detail/macro_scope.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



#include <utility> // declval, pair
// #include <nlohmann/detail/meta/detected.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



#include <type_traits>

// #include <nlohmann/detail/meta/void_t.hpp>
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT



// #include <nlohmann/detail/abi_macros.hpp>


NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

template<typename ...Ts> struct make_void
{
    using type = void;
};
template<typename ...Ts> using void_t = typename make_void<Ts...>::type;

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END


NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

// https://en.cppreference.com/w/cpp/experimental/is_detected
struct nonesuch
{
    nonesuch() = delete;
    ~nonesuch() = delete;
    nonesuch(nonesuch const&) = delete;
    nonesuch(nonesuch const&&) = delete;
    void operator=(nonesuch const&) = delete;
    void operator=(nonesuch&&) = delete;
};

template<class Default,
         class AlwaysVoid,
         template<class...> class Op,
         class... Args>
struct detector
{
    using value_t = std::false_type;
    using type = Default;
};

template<class Default, template<class...> class Op, class... Args>
struct detector<Default, void_t<Op<Args...>>, Op, Args...>
{
    using value_t = std::true_type;
    using type = Op<Args...>;
};

template<template<class...> class Op, class... Args>
using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;

template<template<class...> class Op, class... Args>
struct is_detected_lazy : is_detected<Op, Args...> { };

template<template<class...> class Op, class... Args>
using detected_t = typename detector<nonesuch, void, Op, Args...>::type;

template<class Default, template<class...> class Op, class... Args>
using detected_or = detector<Default, void, Op, Args...>;

template<class Default, template<class...> class Op, class... Args>
using detected_or_t = typename detected_or<Default, Op, Args...>::type;

template<class Expected, template<class...> class Op, class... Args>
using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

template<class To, template<class...> class Op, class... Args>
using is_detected_convertible =
    std::is_convertible<detected_t<Op, Args...>, To>;

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END

// #include <nlohmann/thirdparty/hedley/hedley.hpp>


//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2016 - 2021 Evan Nemerson <evan@nemerson.com>
// SPDX-License-Identifier: MIT

/* Hedley - https://nemequ.github.io/hedley
 * Created by Evan Nemerson <evan@nemerson.com>
 */

#if !defined(JSON_HEDLEY_VERSION) || (JSON_HEDLEY_VERSION < 15)
#if defined(JSON_HEDLEY_VERSION)
    #undef JSON_HEDLEY_VERSION
#endif
#define JSON_HEDLEY_VERSION 15

#if defined(JSON_HEDLEY_STRINGIFY_EX)
    #undef JSON_HEDLEY_STRINGIFY_EX
#endif
#define JSON_HEDLEY_STRINGIFY_EX(x) #x

#if defined(JSON_HEDLEY_STRINGIFY)
    #undef JSON_HEDLEY_STRINGIFY
#endif
#define JSON_HEDLEY_STRINGIFY(x) JSON_HEDLEY_STRINGIFY_EX(x)

#if defined(JSON_HEDLEY_CONCAT_EX)
    #undef JSON_HEDLEY_CONCAT_EX
#endif
#define JSON_HEDLEY_CONCAT_EX(a,b) a##b

#if defined(JSON_HEDLEY_CONCAT)
    #undef JSON_HEDLEY_CONCAT
#endif
#define JSON_HEDLEY_CONCAT(a,b) JSON_HEDLEY_CONCAT_EX(a,b)

#if defined(JSON_HEDLEY_CONCAT3_EX)
    #undef JSON_HEDLEY_CONCAT3_EX
#endif
#define JSON_HEDLEY_CONCAT3_EX(a,b,c) a##b##c

#if defined(JSON_HEDLEY_CONCAT3)
    #undef JSON_HEDLEY_CONCAT3
#endif
#define JSON_HEDLEY_CONCAT3(a,b,c) JSON_HEDLEY_CONCAT3_EX(a,b,c)

#if defined(JSON_HEDLEY_VERSION_ENCODE)
    #undef JSON_HEDLEY_VERSION_ENCODE
#endif
#define JSON_HEDLEY_VERSION_ENCODE(major,minor,revision) (((major) * 1000000) + ((minor) * 1000) + (revision))

#if defined(JSON_HEDLEY_VERSION_DECODE_MAJOR)
    #undef JSON_HEDLEY_VERSION_DECODE_MAJOR
#endif
#define JSON_HEDLEY_VERSION_DECODE_MAJOR(version) ((version) / 1000000)

#if defined(JSON_HEDLEY_VERSION_DECODE_MINOR)
    #undef JSON_HEDLEY_VERSION_DECODE_MINOR
#endif
#define JSON_HEDLEY_VERSION_DECODE_MINOR(version) (((version) % 1000000) / 1000)

#if defined(JSON_HEDLEY_VERSION_DECODE_REVISION)
    #undef JSON_HEDLEY_VERSION_DECODE_REVISION
#endif
#define JSON_HEDLEY_VERSION_DECODE_REVISION(version) ((version) % 1000)

#if defined(JSON_HEDLEY_GNUC_VERSION)
    #undef JSON_HEDLEY_GNUC_VERSION
#endif
#if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
    #define JSON_HEDLEY_GNUC_VERSION JSON_HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__)
    #define JSON_HEDLEY_GNUC_VERSION JSON_HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, 0)
#endif

#if defined(JSON_HEDLEY_GNUC_VERSION_CHECK)
    #undef JSON_HEDLEY_GNUC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_GNUC_VERSION)
    #define JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_GNUC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_MSVC_VERSION)
    #undef JSON_HEDLEY_MSVC_VERSION
#endif
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000) && !defined(__ICL)
    #define JSON_HEDLEY_MSVC_VERSION JSON_HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 10000000, (_MSC_FULL_VER % 10000000) / 100000, (_MSC_FULL_VER % 100000) / 100)
#elif defined(_MSC_FULL_VER) && !defined(__ICL)
    #define JSON_HEDLEY_MSVC_VERSION JSON_HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 1000000, (_MSC_FULL_VER % 1000000) / 10000, (_MSC_FULL_VER % 10000) / 10)
#elif defined(_MSC_VER) && !defined(__ICL)
    #define JSON_HEDLEY_MSVC_VERSION JSON_HEDLEY_VERSION_ENCODE(_MSC_VER / 100, _MSC_VER % 100, 0)
#endif

#if defined(JSON_HEDLEY_MSVC_VERSION_CHECK)
    #undef JSON_HEDLEY_MSVC_VERSION_CHECK
#endif
#if !defined(JSON_HEDLEY_MSVC_VERSION)
    #define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
    #define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_FULL_VER >= ((major * 10000000) + (minor * 100000) + (patch)))
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
    #define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_FULL_VER >= ((major * 1000000) + (minor * 10000) + (patch)))
#else
    #define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_VER >= ((major * 100) + (minor)))
#endif

#if defined(JSON_HEDLEY_INTEL_VERSION)
    #undef JSON_HEDLEY_INTEL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && !defined(__ICL)
    #define JSON_HEDLEY_INTEL_VERSION JSON_HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, __INTEL_COMPILER_UPDATE)
#elif defined(__INTEL_COMPILER) && !defined(__ICL)
    #define JSON_HEDLEY_INTEL_VERSION JSON_HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, 0)
#endif

#if defined(JSON_HEDLEY_INTEL_VERSION_CHECK)
    #undef JSON_HEDLEY_INTEL_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_INTEL_VERSION)
    #define JSON_HEDLEY_INTEL_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_INTEL_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_INTEL_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_INTEL_CL_VERSION)
    #undef JSON_HEDLEY_INTEL_CL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && defined(__ICL)
    #define JSON_HEDLEY_INTEL_CL_VERSION JSON_HEDLEY_VERSION_ENCODE(__INTEL_COMPILER, __INTEL_COMPILER_UPDATE, 0)
#endif

#if defined(JSON_HEDLEY_INTEL_CL_VERSION_CHECK)
    #undef JSON_HEDLEY_INTEL_CL_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_INTEL_CL_VERSION)
    #define JSON_HEDLEY_INTEL_CL_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_INTEL_CL_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_INTEL_CL_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_PGI_VERSION)
    #undef JSON_HEDLEY_PGI_VERSION
#endif
#if defined(__PGI) && defined(__PGIC__) && defined(__PGIC_MINOR__) && defined(__PGIC_PATCHLEVEL__)
    #define JSON_HEDLEY_PGI_VERSION JSON_HEDLEY_VERSION_ENCODE(__PGIC__, __PGIC_MINOR__, __PGIC_PATCHLEVEL__)
#endif

#if defined(JSON_HEDLEY_PGI_VERSION_CHECK)
    #undef JSON_HEDLEY_PGI_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_PGI_VERSION)
    #define JSON_HEDLEY_PGI_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_PGI_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_PGI_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_SUNPRO_VERSION)
    #undef JSON_HEDLEY_SUNPRO_VERSION
#endif
#if defined(__SUNPRO_C) && (__SUNPRO_C > 0x1000)
    #define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((((__SUNPRO_C >> 16) & 0xf) * 10) + ((__SUNPRO_C >> 12) & 0xf), (((__SUNPRO_C >> 8) & 0xf) * 10) + ((__SUNPRO_C >> 4) & 0xf), (__SUNPRO_C & 0xf) * 10)
#elif defined(__SUNPRO_C)
    #define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((__SUNPRO_C >> 8) & 0xf, (__SUNPRO_C >> 4) & 0xf, (__SUNPRO_C) & 0xf)
#elif defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x1000)
    #define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((((__SUNPRO_CC >> 16) & 0xf) * 10) + ((__SUNPRO_CC >> 12) & 0xf), (((__SUNPRO_CC >> 8) & 0xf) * 10) + ((__SUNPRO_CC >> 4) & 0xf), (__SUNPRO_CC & 0xf) * 10)
#elif defined(__SUNPRO_CC)
    #define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((__SUNPRO_CC >> 8) & 0xf, (__SUNPRO_CC >> 4) & 0xf, (__SUNPRO_CC) & 0xf)
#endif

#if defined(JSON_HEDLEY_SUNPRO_VERSION_CHECK)
    #undef JSON_HEDLEY_SUNPRO_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_SUNPRO_VERSION)
    #define JSON_HEDLEY_SUNPRO_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_SUNPRO_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_SUNPRO_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_EMSCRIPTEN_VERSION)
    #undef JSON_HEDLEY_EMSCRIPTEN_VERSION
#endif
#if defined(__EMSCRIPTEN__)
    #define JSON_HEDLEY_EMSCRIPTEN_VERSION JSON_HEDLEY_VERSION_ENCODE(__EMSCRIPTEN_major__, __EMSCRIPTEN_minor__, __EMSCRIPTEN_tiny__)
#endif

#if defined(JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK)
    #undef JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_EMSCRIPTEN_VERSION)
    #define JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_EMSCRIPTEN_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_ARM_VERSION)
    #undef JSON_HEDLEY_ARM_VERSION
#endif
#if defined(__CC_ARM) && defined(__ARMCOMPILER_VERSION)
    #define JSON_HEDLEY_ARM_VERSION JSON_HEDLEY_VERSION_ENCODE(__ARMCOMPILER_VERSION / 1000000, (__ARMCOMPILER_VERSION % 1000000) / 10000, (__ARMCOMPILER_VERSION % 10000) / 100)
#elif defined(__CC_ARM) && defined(__ARMCC_VERSION)
    #define JSON_HEDLEY_ARM_VERSION JSON_HEDLEY_VERSION_ENCODE(__ARMCC_VERSION / 1000000, (__ARMCC_VERSION % 1000000) / 10000, (__ARMCC_VERSION % 10000) / 100)
#endif

#if defined(JSON_HEDLEY_ARM_VERSION_CHECK)
    #undef JSON_HEDLEY_ARM_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_ARM_VERSION)
    #define JSON_HEDLEY_ARM_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_ARM_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_ARM_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_IBM_VERSION)
    #undef JSON_HEDLEY_IBM_VERSION
#endif
#if defined(__ibmxl__)
    #define JSON_HEDLEY_IBM_VERSION JSON_HEDLEY_VERSION_ENCODE(__ibmxl_version__, __ibmxl_release__, __ibmxl_modification__)
#elif defined(__xlC__) && defined(__xlC_ver__)
    #define JSON_HEDLEY_IBM_VERSION JSON_HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, (__xlC_ver__ >> 8) & 0xff)
#elif defined(__xlC__)
    #define JSON_HEDLEY_IBM_VERSION JSON_HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, 0)
#endif

#if defined(JSON_HEDLEY_IBM_VERSION_CHECK)
    #undef JSON_HEDLEY_IBM_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_IBM_VERSION)
    #define JSON_HEDLEY_IBM_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_IBM_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_IBM_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TI_VERSION)
    #undef JSON_HEDLEY_TI_VERSION
#endif
#if \
    defined(__TI_COMPILER_VERSION__) && \
    ( \
      defined(__TMS470__) || defined(__TI_ARM__) || \
      defined(__MSP430__) || \
      defined(__TMS320C2000__) \
    )
#if (__TI_COMPILER_VERSION__ >= 16000000)
    #define JSON_HEDLEY_TI_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#endif

#if defined(JSON_HEDLEY_TI_VERSION_CHECK)
    #undef JSON_HEDLEY_TI_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_VERSION)
    #define JSON_HEDLEY_TI_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TI_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TI_CL2000_VERSION)
    #undef JSON_HEDLEY_TI_CL2000_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C2000__)
    #define JSON_HEDLEY_TI_CL2000_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(JSON_HEDLEY_TI_CL2000_VERSION_CHECK)
    #undef JSON_HEDLEY_TI_CL2000_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL2000_VERSION)
    #define JSON_HEDLEY_TI_CL2000_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL2000_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TI_CL2000_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TI_CL430_VERSION)
    #undef JSON_HEDLEY_TI_CL430_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__MSP430__)
    #define JSON_HEDLEY_TI_CL430_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(JSON_HEDLEY_TI_CL430_VERSION_CHECK)
    #undef JSON_HEDLEY_TI_CL430_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL430_VERSION)
    #define JSON_HEDLEY_TI_CL430_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL430_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TI_CL430_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TI_ARMCL_VERSION)
    #undef JSON_HEDLEY_TI_ARMCL_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && (defined(__TMS470__) || defined(__TI_ARM__))
    #define JSON_HEDLEY_TI_ARMCL_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(JSON_HEDLEY_TI_ARMCL_VERSION_CHECK)
    #undef JSON_HEDLEY_TI_ARMCL_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_ARMCL_VERSION)
    #define JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_ARMCL_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TI_CL6X_VERSION)
    #undef JSON_HEDLEY_TI_CL6X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C6X__)
    #define JSON_HEDLEY_TI_CL6X_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(JSON_HEDLEY_TI_CL6X_VERSION_CHECK)
    #undef JSON_HEDLEY_TI_CL6X_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL6X_VERSION)
    #define JSON_HEDLEY_TI_CL6X_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL6X_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TI_CL6X_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TI_CL7X_VERSION)
    #undef JSON_HEDLEY_TI_CL7X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__C7000__)
    #define JSON_HEDLEY_TI_CL7X_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(JSON_HEDLEY_TI_CL7X_VERSION_CHECK)
    #undef JSON_HEDLEY_TI_CL7X_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL7X_VERSION)
    #define JSON_HEDLEY_TI_CL7X_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL7X_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TI_CL7X_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TI_CLPRU_VERSION)
    #undef JSON_HEDLEY_TI_CLPRU_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__PRU__)
    #define JSON_HEDLEY_TI_CLPRU_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(JSON_HEDLEY_TI_CLPRU_VERSION_CHECK)
    #undef JSON_HEDLEY_TI_CLPRU_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CLPRU_VERSION)
    #define JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CLPRU_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_CRAY_VERSION)
    #undef JSON_HEDLEY_CRAY_VERSION
#endif
#if defined(_CRAYC)
    #if defined(_RELEASE_PATCHLEVEL)
        #define JSON_HEDLEY_CRAY_VERSION JSON_HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, _RELEASE_PATCHLEVEL)
    #else
        #define JSON_HEDLEY_CRAY_VERSION JSON_HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, 0)
    #endif
#endif

#if defined(JSON_HEDLEY_CRAY_VERSION_CHECK)
    #undef JSON_HEDLEY_CRAY_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_CRAY_VERSION)
    #define JSON_HEDLEY_CRAY_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_CRAY_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_CRAY_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_IAR_VERSION)
    #undef JSON_HEDLEY_IAR_VERSION
#endif
#if defined(__IAR_SYSTEMS_ICC__)
    #if __VER__ > 1000
        #define JSON_HEDLEY_IAR_VERSION JSON_HEDLEY_VERSION_ENCODE((__VER__ / 1000000), ((__VER__ / 1000) % 1000), (__VER__ % 1000))
    #else
        #define JSON_HEDLEY_IAR_VERSION JSON_HEDLEY_VERSION_ENCODE(__VER__ / 100, __VER__ % 100, 0)
    #endif
#endif

#if defined(JSON_HEDLEY_IAR_VERSION_CHECK)
    #undef JSON_HEDLEY_IAR_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_IAR_VERSION)
    #define JSON_HEDLEY_IAR_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_IAR_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_IAR_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_TINYC_VERSION)
    #undef JSON_HEDLEY_TINYC_VERSION
#endif
#if defined(__TINYC__)
    #define JSON_HEDLEY_TINYC_VERSION JSON_HEDLEY_VERSION_ENCODE(__TINYC__ / 1000, (__TINYC__ / 100) % 10, __TINYC__ % 100)
#endif

#if defined(JSON_HEDLEY_TINYC_VERSION_CHECK)
    #undef JSON_HEDLEY_TINYC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TINYC_VERSION)
    #define JSON_HEDLEY_TINYC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TINYC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_TINYC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_DMC_VERSION)
    #undef JSON_HEDLEY_DMC_VERSION
#endif
#if defined(__DMC__)
    #define JSON_HEDLEY_DMC_VERSION JSON_HEDLEY_VERSION_ENCODE(__DMC__ >> 8, (__DMC__ >> 4) & 0xf, __DMC__ & 0xf)
#endif

#if defined(JSON_HEDLEY_DMC_VERSION_CHECK)
    #undef JSON_HEDLEY_DMC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_DMC_VERSION)
    #define JSON_HEDLEY_DMC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_DMC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_DMC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_COMPCERT_VERSION)
    #undef JSON_HEDLEY_COMPCERT_VERSION
#endif
#if defined(__COMPCERT_VERSION__)
    #define JSON_HEDLEY_COMPCERT_VERSION JSON_HEDLEY_VERSION_ENCODE(__COMPCERT_VERSION__ / 10000, (__COMPCERT_VERSION__ / 100) % 100, __COMPCERT_VERSION__ % 100)
#endif

#if defined(JSON_HEDLEY_COMPCERT_VERSION_CHECK)
    #undef JSON_HEDLEY_COMPCERT_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_COMPCERT_VERSION)
    #define JSON_HEDLEY_COMPCERT_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_COMPCERT_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_COMPCERT_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_PELLES_VERSION)
    #undef JSON_HEDLEY_PELLES_VERSION
#endif
#if defined(__POCC__)
    #define JSON_HEDLEY_PELLES_VERSION JSON_HEDLEY_VERSION_ENCODE(__POCC__ / 100, __POCC__ % 100, 0)
#endif

#if defined(JSON_HEDLEY_PELLES_VERSION_CHECK)
    #undef JSON_HEDLEY_PELLES_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_PELLES_VERSION)
    #define JSON_HEDLEY_PELLES_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_PELLES_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_PELLES_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_MCST_LCC_VERSION)
    #undef JSON_HEDLEY_MCST_LCC_VERSION
#endif
#if defined(__LCC__) && defined(__LCC_MINOR__)
    #define JSON_HEDLEY_MCST_LCC_VERSION JSON_HEDLEY_VERSION_ENCODE(__LCC__ / 100, __LCC__ % 100, __LCC_MINOR__)
#endif

#if defined(JSON_HEDLEY_MCST_LCC_VERSION_CHECK)
    #undef JSON_HEDLEY_MCST_LCC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_MCST_LCC_VERSION)
    #define JSON_HEDLEY_MCST_LCC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_MCST_LCC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_MCST_LCC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_GCC_VERSION)
    #undef JSON_HEDLEY_GCC_VERSION
#endif
#if \
    defined(JSON_HEDLEY_GNUC_VERSION) && \
    !defined(__clang__) && \
    !defined(JSON_HEDLEY_INTEL_VERSION) && \
    !defined(JSON_HEDLEY_PGI_VERSION) && \
    !defined(JSON_HEDLEY_ARM_VERSION) && \
    !defined(JSON_HEDLEY_CRAY_VERSION) && \
    !defined(JSON_HEDLEY_TI_VERSION) && \
    !defined(JSON_HEDLEY_TI_ARMCL_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL430_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL2000_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL6X_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL7X_VERSION) && \
    !defined(JSON_HEDLEY_TI_CLPRU_VERSION) && \
    !defined(__COMPCERT__) && \
    !defined(JSON_HEDLEY_MCST_LCC_VERSION)
    #define JSON_HEDLEY_GCC_VERSION JSON_HEDLEY_GNUC_VERSION
#endif

#if defined(JSON_HEDLEY_GCC_VERSION_CHECK)
    #undef JSON_HEDLEY_GCC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_GCC_VERSION)
    #define JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_GCC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
    #define JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(JSON_HEDLEY_HAS_ATTRIBUTE)
    #undef JSON_HEDLEY_HAS_ATTRIBUTE
#endif
#if \
  defined(__has_attribute) && \
  ( \
    (!defined(JSON_HEDLEY_IAR_VERSION) || JSON_HEDLEY_IAR_VERSION_CHECK(8,5,9)) \
  )
#  define JSON_HEDLEY_HAS_ATTRIBUTE(attribute) __has_attribute(attribute)
#else
#  define JSON_HEDLEY_HAS_ATTRIBUTE(attribute) (0)
#endif

#if defined(JSON_HEDLEY_GNUC_HAS_ATTRIBUTE)
    #undef JSON_HEDLEY_GNUC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
    #define JSON_HEDLEY_GNUC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_HAS_ATTRIBUTE(attribute)
#else
    #define JSON_HEDLEY_GNUC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_GCC_HAS_ATTRIBUTE)
    #undef JSON_HEDLEY_GCC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
    #define JSON_HEDLEY_GCC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_HAS_ATTRIBUTE(attribute)
#else
    #define JSON_HEDLEY_GCC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_HAS_CPP_ATTRIBUTE)
    #undef JSON_HEDLEY_HAS_CPP_ATTRIBUTE
#endif
#if \
    defined(__has_cpp_attribute) && \
    defined(__cplusplus) && \
    (!defined(JSON_HEDLEY_SUNPRO_VERSION) || JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0))
    #define JSON_HEDLEY_HAS_CPP_ATTRIBUTE(attribute) __has_cpp_attribute(attribute)
#else
    #define JSON_HEDLEY_HAS_CPP_ATTRIBUTE(attribute) (0)
#endif

#if defined(JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS)
    #undef JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS
#endif
#if !defined(__cplusplus) || !defined(__has_cpp_attribute)
    #define JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) (0)
#elif \
    !defined(JSON_HEDLEY_PGI_VERSION) && \
    !defined(JSON_HEDLEY_IAR_VERSION) && \
    (!defined(JSON_HEDLEY_SUNPRO_VERSION) || JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0)) && \
    (!defined(JSON_HEDLEY_MSVC_VERSION) || JSON_HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    #define JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) JSON_HEDLEY_HAS_CPP_ATTRIBUTE(ns::attribute)
#else
    #define JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) (0)
#endif

#if defined(JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE)
    #undef JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
    #define JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) __has_cpp_attribute(attribute)
#else
    #define JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE)
    #undef JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
    #define JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) __has_cpp_attribute(attribute)
#else
    #define JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_HAS_BUILTIN)
    #undef JSON_HEDLEY_HAS_BUILTIN
#endif
#if defined(__has_builtin)
    #define JSON_HEDLEY_HAS_BUILTIN(builtin) __has_builtin(builtin)
#else
    #define JSON_HEDLEY_HAS_BUILTIN(builtin) (0)
#endif

#if defined(JSON_HEDLEY_GNUC_HAS_BUILTIN)
    #undef JSON_HEDLEY_GNUC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
    #define JSON_HEDLEY_GNUC_HAS_BUILTIN(builtin,major,minor,patch) __has_builtin(builtin)
#else
    #define JSON_HEDLEY_GNUC_HAS_BUILTIN(builtin,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_GCC_HAS_BUILTIN)
    #undef JSON_HEDLEY_GCC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
    #define JSON_HEDLEY_GCC_HAS_BUILTIN(builtin,major,minor,patch) __has_builtin(builtin)
#else
    #define JSON_HEDLEY_GCC_HAS_BUILTIN(builtin,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_HAS_FEATURE)
    #undef JSON_HEDLEY_HAS_FEATURE
#endif
#if defined(__has_feature)
    #define JSON_HEDLEY_HAS_FEATURE(feature) __has_feature(feature)
#else
    #define JSON_HEDLEY_HAS_FEATURE(feature) (0)
#endif

#if defined(JSON_HEDLEY_GNUC_HAS_FEATURE)
    #undef JSON_HEDLEY_GNUC_HAS_FEATURE
#endif
#if defined(__has_feature)
    #define JSON_HEDLEY_GNUC_HAS_FEATURE(feature,major,minor,patch) __has_feature(feature)
#else
    #define JSON_HEDLEY_GNUC_HAS_FEATURE(feature,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_GCC_HAS_FEATURE)
    #undef JSON_HEDLEY_GCC_HAS_FEATURE
#endif
#if defined(__has_feature)
    #define JSON_HEDLEY_GCC_HAS_FEATURE(feature,major,minor,patch) __has_feature(feature)
#else
    #define JSON_HEDLEY_GCC_HAS_FEATURE(feature,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_HAS_EXTENSION)
    #undef JSON_HEDLEY_HAS_EXTENSION
#endif
#if defined(__has_extension)
    #define JSON_HEDLEY_HAS_EXTENSION(extension) __has_extension(extension)
#else
    #define JSON_HEDLEY_HAS_EXTENSION(extension) (0)
#endif

#if defined(JSON_HEDLEY_GNUC_HAS_EXTENSION)
    #undef JSON_HEDLEY_GNUC_HAS_EXTENSION
#endif
#if defined(__has_extension)
    #define JSON_HEDLEY_GNUC_HAS_EXTENSION(extension,major,minor,patch) __has_extension(extension)
#else
    #define JSON_HEDLEY_GNUC_HAS_EXTENSION(extension,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_GCC_HAS_EXTENSION)
    #undef JSON_HEDLEY_GCC_HAS_EXTENSION
#endif
#if defined(__has_extension)
    #define JSON_HEDLEY_GCC_HAS_EXTENSION(extension,major,minor,patch) __has_extension(extension)
#else
    #define JSON_HEDLEY_GCC_HAS_EXTENSION(extension,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE)
    #undef JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
    #define JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) __has_declspec_attribute(attribute)
#else
    #define JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) (0)
#endif

#if defined(JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE)
    #undef JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
    #define JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) __has_declspec_attribute(attribute)
#else
    #define JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE)
    #undef JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
    #define JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) __has_declspec_attribute(attribute)
#else
    #define JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_HAS_WARNING)
    #undef JSON_HEDLEY_HAS_WARNING
#endif
#if defined(__has_warning)
    #define JSON_HEDLEY_HAS_WARNING(warning) __has_warning(warning)
#else
    #define JSON_HEDLEY_HAS_WARNING(warning) (0)
#endif

#if defined(JSON_HEDLEY_GNUC_HAS_WARNING)
    #undef JSON_HEDLEY_GNUC_HAS_WARNING
#endif
#if defined(__has_warning)
    #define JSON_HEDLEY_GNUC_HAS_WARNING(warning,major,minor,patch) __has_warning(warning)
#else
    #define JSON_HEDLEY_GNUC_HAS_WARNING(warning,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_GCC_HAS_WARNING)
    #undef JSON_HEDLEY_GCC_HAS_WARNING
#endif
#if defined(__has_warning)
    #define JSON_HEDLEY_GCC_HAS_WARNING(warning,major,minor,patch) __has_warning(warning)
#else
    #define JSON_HEDLEY_GCC_HAS_WARNING(warning,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
    defined(__clang__) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(18,4,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,7,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(2,0,1) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,0,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_CRAY_VERSION_CHECK(5,0,0) || \
    JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,17) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(8,0,0) || \
    (JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) && defined(__C99_PRAGMA_OPERATOR))
    #define JSON_HEDLEY_PRAGMA(value) _Pragma(#value)
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0)
    #define JSON_HEDLEY_PRAGMA(value) __pragma(value)
#else
    #define JSON_HEDLEY_PRAGMA(value)
#endif

#if defined(JSON_HEDLEY_DIAGNOSTIC_PUSH)
    #undef JSON_HEDLEY_DIAGNOSTIC_PUSH
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_POP)
    #undef JSON_HEDLEY_DIAGNOSTIC_POP
#endif
#if defined(__clang__)
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
    #define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
    #define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,6,0)
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
    #define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH __pragma(warning(push))
    #define JSON_HEDLEY_DIAGNOSTIC_POP __pragma(warning(pop))
#elif JSON_HEDLEY_ARM_VERSION_CHECK(5,6,0)
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("push")
    #define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("pop")
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,4,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,1,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0)
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("diag_push")
    #define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("diag_pop")
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(2,90,0)
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
    #define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#else
    #define JSON_HEDLEY_DIAGNOSTIC_PUSH
    #define JSON_HEDLEY_DIAGNOSTIC_POP
#endif

/* JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_ is for
   HEDLEY INTERNAL USE ONLY.  API subject to change without notice. */
#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
    #undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_
#endif
#if defined(__cplusplus)
#  if JSON_HEDLEY_HAS_WARNING("-Wc++98-compat")
#    if JSON_HEDLEY_HAS_WARNING("-Wc++17-extensions")
#      if JSON_HEDLEY_HAS_WARNING("-Wc++1z-extensions")
#        define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
    _Pragma("clang diagnostic ignored \"-Wc++17-extensions\"") \
    _Pragma("clang diagnostic ignored \"-Wc++1z-extensions\"") \
    xpr \
    JSON_HEDLEY_DIAGNOSTIC_POP
#      else
#        define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
    _Pragma("clang diagnostic ignored \"-Wc++17-extensions\"") \
    xpr \
    JSON_HEDLEY_DIAGNOSTIC_POP
#      endif
#    else
#      define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
    xpr \
    JSON_HEDLEY_DIAGNOSTIC_POP
#    endif
#  endif
#endif
#if !defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(x) x
#endif

#if defined(JSON_HEDLEY_CONST_CAST)
    #undef JSON_HEDLEY_CONST_CAST
#endif
#if defined(__cplusplus)
#  define JSON_HEDLEY_CONST_CAST(T, expr) (const_cast<T>(expr))
#elif \
  JSON_HEDLEY_HAS_WARNING("-Wcast-qual") || \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,6,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define JSON_HEDLEY_CONST_CAST(T, expr) (__extension__ ({ \
        JSON_HEDLEY_DIAGNOSTIC_PUSH \
        JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL \
        ((T) (expr)); \
        JSON_HEDLEY_DIAGNOSTIC_POP \
    }))
#else
#  define JSON_HEDLEY_CONST_CAST(T, expr) ((T) (expr))
#endif

#if defined(JSON_HEDLEY_REINTERPRET_CAST)
    #undef JSON_HEDLEY_REINTERPRET_CAST
#endif
#if defined(__cplusplus)
    #define JSON_HEDLEY_REINTERPRET_CAST(T, expr) (reinterpret_cast<T>(expr))
#else
    #define JSON_HEDLEY_REINTERPRET_CAST(T, expr) ((T) (expr))
#endif

#if defined(JSON_HEDLEY_STATIC_CAST)
    #undef JSON_HEDLEY_STATIC_CAST
#endif
#if defined(__cplusplus)
    #define JSON_HEDLEY_STATIC_CAST(T, expr) (static_cast<T>(expr))
#else
    #define JSON_HEDLEY_STATIC_CAST(T, expr) ((T) (expr))
#endif

#if defined(JSON_HEDLEY_CPP_CAST)
    #undef JSON_HEDLEY_CPP_CAST
#endif
#if defined(__cplusplus)
#  if JSON_HEDLEY_HAS_WARNING("-Wold-style-cast")
#    define JSON_HEDLEY_CPP_CAST(T, expr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wold-style-cast\"") \
    ((T) (expr)) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#  elif JSON_HEDLEY_IAR_VERSION_CHECK(8,3,0)
#    define JSON_HEDLEY_CPP_CAST(T, expr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("diag_suppress=Pe137") \
    JSON_HEDLEY_DIAGNOSTIC_POP
#  else
#    define JSON_HEDLEY_CPP_CAST(T, expr) ((T) (expr))
#  endif
#else
#  define JSON_HEDLEY_CPP_CAST(T, expr) (expr)
#endif

#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED)
    #undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wdeprecated-declarations")
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("warning(disable:1478 1786)")
#elif JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED __pragma(warning(disable:1478 1786))
#elif JSON_HEDLEY_PGI_VERSION_CHECK(20,7,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1216,1444,1445")
#elif JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1444")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,3,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED __pragma(warning(disable:4996))
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1444")
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1291,1718")
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) && !defined(__cplusplus)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("error_messages(off,E_DEPRECATED_ATT,E_DEPRECATED_ATT_MESS)")
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) && defined(__cplusplus)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("error_messages(off,symdeprecated,symdeprecated2)")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress=Pe1444,Pe1215")
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(2,90,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("warn(disable:2241)")
#else
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif

#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS)
    #undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-pragmas")
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("clang diagnostic ignored \"-Wunknown-pragmas\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("warning(disable:161)")
#elif JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS __pragma(warning(disable:161))
#elif JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 1675")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,3,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("GCC diagnostic ignored \"-Wunknown-pragmas\"")
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS __pragma(warning(disable:4068))
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(16,9,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress=Pe161")
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 161")
#else
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif

#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES)
    #undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-attributes")
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("clang diagnostic ignored \"-Wunknown-attributes\"")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,6,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(17,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("warning(disable:1292)")
#elif JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES __pragma(warning(disable:1292))
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(19,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES __pragma(warning(disable:5030))
#elif JSON_HEDLEY_PGI_VERSION_CHECK(20,7,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097,1098")
#elif JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097")
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,14,0) && defined(__cplusplus)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("error_messages(off,attrskipunsup)")
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(18,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,3,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1173")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress=Pe1097")
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097")
#else
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif

#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL)
    #undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wcast-qual")
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("warning(disable:2203 2331)")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(3,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#else
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif

#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION)
    #undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunused-function")
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("clang diagnostic ignored \"-Wunused-function\"")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(1,0,0)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION __pragma(warning(disable:4505))
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("diag_suppress 3142")
#else
    #define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION
#endif

#if defined(JSON_HEDLEY_DEPRECATED)
    #undef JSON_HEDLEY_DEPRECATED
#endif
#if defined(JSON_HEDLEY_DEPRECATED_FOR)
    #undef JSON_HEDLEY_DEPRECATED_FOR
#endif
#if \
    JSON_HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_DEPRECATED(since) __declspec(deprecated("Since " # since))
    #define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __declspec(deprecated("Since " #since "; use " #replacement))
#elif \
    (JSON_HEDLEY_HAS_EXTENSION(attribute_deprecated_with_message) && !defined(JSON_HEDLEY_IAR_VERSION)) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,5,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(5,6,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(18,1,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(18,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,3,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_DEPRECATED(since) __attribute__((__deprecated__("Since " #since)))
    #define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __attribute__((__deprecated__("Since " #since "; use " #replacement)))
#elif defined(__cplusplus) && (__cplusplus >= 201402L)
    #define JSON_HEDLEY_DEPRECATED(since) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[deprecated("Since " #since)]])
    #define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[deprecated("Since " #since "; use " #replacement)]])
#elif \
    JSON_HEDLEY_HAS_ATTRIBUTE(deprecated) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
    #define JSON_HEDLEY_DEPRECATED(since) __attribute__((__deprecated__))
    #define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __attribute__((__deprecated__))
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_PELLES_VERSION_CHECK(6,50,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_DEPRECATED(since) __declspec(deprecated)
    #define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __declspec(deprecated)
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
    #define JSON_HEDLEY_DEPRECATED(since) _Pragma("deprecated")
    #define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) _Pragma("deprecated")
#else
    #define JSON_HEDLEY_DEPRECATED(since)
    #define JSON_HEDLEY_DEPRECATED_FOR(since, replacement)
#endif

#if defined(JSON_HEDLEY_UNAVAILABLE)
    #undef JSON_HEDLEY_UNAVAILABLE
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(warning) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,3,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_UNAVAILABLE(available_since) __attribute__((__warning__("Not available until " #available_since)))
#else
    #define JSON_HEDLEY_UNAVAILABLE(available_since)
#endif

#if defined(JSON_HEDLEY_WARN_UNUSED_RESULT)
    #undef JSON_HEDLEY_WARN_UNUSED_RESULT
#endif
#if defined(JSON_HEDLEY_WARN_UNUSED_RESULT_MSG)
    #undef JSON_HEDLEY_WARN_UNUSED_RESULT_MSG
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(warn_unused_result) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0) && defined(__cplusplus)) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
    #define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) __attribute__((__warn_unused_result__))
#elif (JSON_HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard) >= 201907L)
    #define JSON_HEDLEY_WARN_UNUSED_RESULT JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
    #define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard(msg)]])
#elif JSON_HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard)
    #define JSON_HEDLEY_WARN_UNUSED_RESULT JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
    #define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#elif defined(_Check_return_) /* SAL */
    #define JSON_HEDLEY_WARN_UNUSED_RESULT _Check_return_
    #define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) _Check_return_
#else
    #define JSON_HEDLEY_WARN_UNUSED_RESULT
    #define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg)
#endif

#if defined(JSON_HEDLEY_SENTINEL)
    #undef JSON_HEDLEY_SENTINEL
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(sentinel) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(5,4,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_SENTINEL(position) __attribute__((__sentinel__(position)))
#else
    #define JSON_HEDLEY_SENTINEL(position)
#endif

#if defined(JSON_HEDLEY_NO_RETURN)
    #undef JSON_HEDLEY_NO_RETURN
#endif
#if JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
    #define JSON_HEDLEY_NO_RETURN __noreturn
#elif \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define JSON_HEDLEY_NO_RETURN _Noreturn
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
    #define JSON_HEDLEY_NO_RETURN JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[noreturn]])
#elif \
    JSON_HEDLEY_HAS_ATTRIBUTE(noreturn) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,2,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
    #define JSON_HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
    #define JSON_HEDLEY_NO_RETURN _Pragma("does_not_return")
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_NO_RETURN __declspec(noreturn)
#elif JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,0,0) && defined(__cplusplus)
    #define JSON_HEDLEY_NO_RETURN _Pragma("FUNC_NEVER_RETURNS;")
#elif JSON_HEDLEY_COMPCERT_VERSION_CHECK(3,2,0)
    #define JSON_HEDLEY_NO_RETURN __attribute((noreturn))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(9,0,0)
    #define JSON_HEDLEY_NO_RETURN __declspec(noreturn)
#else
    #define JSON_HEDLEY_NO_RETURN
#endif

#if defined(JSON_HEDLEY_NO_ESCAPE)
    #undef JSON_HEDLEY_NO_ESCAPE
#endif
#if JSON_HEDLEY_HAS_ATTRIBUTE(noescape)
    #define JSON_HEDLEY_NO_ESCAPE __attribute__((__noescape__))
#else
    #define JSON_HEDLEY_NO_ESCAPE
#endif

#if defined(JSON_HEDLEY_UNREACHABLE)
    #undef JSON_HEDLEY_UNREACHABLE
#endif
#if defined(JSON_HEDLEY_UNREACHABLE_RETURN)
    #undef JSON_HEDLEY_UNREACHABLE_RETURN
#endif
#if defined(JSON_HEDLEY_ASSUME)
    #undef JSON_HEDLEY_ASSUME
#endif
#if \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_ASSUME(expr) __assume(expr)
#elif JSON_HEDLEY_HAS_BUILTIN(__builtin_assume)
    #define JSON_HEDLEY_ASSUME(expr) __builtin_assume(expr)
#elif \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0)
    #if defined(__cplusplus)
        #define JSON_HEDLEY_ASSUME(expr) std::_nassert(expr)
    #else
        #define JSON_HEDLEY_ASSUME(expr) _nassert(expr)
    #endif
#endif
#if \
    (JSON_HEDLEY_HAS_BUILTIN(__builtin_unreachable) && (!defined(JSON_HEDLEY_ARM_VERSION))) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,5,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(18,10,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(13,1,5) || \
    JSON_HEDLEY_CRAY_VERSION_CHECK(10,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_UNREACHABLE() __builtin_unreachable()
#elif defined(JSON_HEDLEY_ASSUME)
    #define JSON_HEDLEY_UNREACHABLE() JSON_HEDLEY_ASSUME(0)
#endif
#if !defined(JSON_HEDLEY_ASSUME)
    #if defined(JSON_HEDLEY_UNREACHABLE)
        #define JSON_HEDLEY_ASSUME(expr) JSON_HEDLEY_STATIC_CAST(void, ((expr) ? 1 : (JSON_HEDLEY_UNREACHABLE(), 1)))
    #else
        #define JSON_HEDLEY_ASSUME(expr) JSON_HEDLEY_STATIC_CAST(void, expr)
    #endif
#endif
#if defined(JSON_HEDLEY_UNREACHABLE)
    #if  \
        JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
        JSON_HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0)
        #define JSON_HEDLEY_UNREACHABLE_RETURN(value) return (JSON_HEDLEY_STATIC_CAST(void, JSON_HEDLEY_ASSUME(0)), (value))
    #else
        #define JSON_HEDLEY_UNREACHABLE_RETURN(value) JSON_HEDLEY_UNREACHABLE()
    #endif
#else
    #define JSON_HEDLEY_UNREACHABLE_RETURN(value) return (value)
#endif
#if !defined(JSON_HEDLEY_UNREACHABLE)
    #define JSON_HEDLEY_UNREACHABLE() JSON_HEDLEY_ASSUME(0)
#endif

JSON_HEDLEY_DIAGNOSTIC_PUSH
#if JSON_HEDLEY_HAS_WARNING("-Wpedantic")
    #pragma clang diagnostic ignored "-Wpedantic"
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic") && defined(__cplusplus)
    #pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif
#if JSON_HEDLEY_GCC_HAS_WARNING("-Wvariadic-macros",4,0,0)
    #if defined(__clang__)
        #pragma clang diagnostic ignored "-Wvariadic-macros"
    #elif defined(JSON_HEDLEY_GCC_VERSION)
        #pragma GCC diagnostic ignored "-Wvariadic-macros"
    #endif
#endif
#if defined(JSON_HEDLEY_NON_NULL)
    #undef JSON_HEDLEY_NON_NULL
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(nonnull) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0)
    #define JSON_HEDLEY_NON_NULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#else
    #define JSON_HEDLEY_NON_NULL(...)
#endif
JSON_HEDLEY_DIAGNOSTIC_POP

#if defined(JSON_HEDLEY_PRINTF_FORMAT)
    #undef JSON_HEDLEY_PRINTF_FORMAT
#endif
#if defined(__MINGW32__) && JSON_HEDLEY_GCC_HAS_ATTRIBUTE(format,4,4,0) && !defined(__USE_MINGW_ANSI_STDIO)
    #define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(ms_printf, string_idx, first_to_check)))
#elif defined(__MINGW32__) && JSON_HEDLEY_GCC_HAS_ATTRIBUTE(format,4,4,0) && defined(__USE_MINGW_ANSI_STDIO)
    #define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(gnu_printf, string_idx, first_to_check)))
#elif \
    JSON_HEDLEY_HAS_ATTRIBUTE(format) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(5,6,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(__printf__, string_idx, first_to_check)))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(6,0,0)
    #define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __declspec(vaformat(printf,string_idx,first_to_check))
#else
    #define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check)
#endif

#if defined(JSON_HEDLEY_CONSTEXPR)
    #undef JSON_HEDLEY_CONSTEXPR
#endif
#if defined(__cplusplus)
    #if __cplusplus >= 201103L
        #define JSON_HEDLEY_CONSTEXPR JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(constexpr)
    #endif
#endif
#if !defined(JSON_HEDLEY_CONSTEXPR)
    #define JSON_HEDLEY_CONSTEXPR
#endif

#if defined(JSON_HEDLEY_PREDICT)
    #undef JSON_HEDLEY_PREDICT
#endif
#if defined(JSON_HEDLEY_LIKELY)
    #undef JSON_HEDLEY_LIKELY
#endif
#if defined(JSON_HEDLEY_UNLIKELY)
    #undef JSON_HEDLEY_UNLIKELY
#endif
#if defined(JSON_HEDLEY_UNPREDICTABLE)
    #undef JSON_HEDLEY_UNPREDICTABLE
#endif
#if JSON_HEDLEY_HAS_BUILTIN(__builtin_unpredictable)
    #define JSON_HEDLEY_UNPREDICTABLE(expr) __builtin_unpredictable((expr))
#endif
#if \
  (JSON_HEDLEY_HAS_BUILTIN(__builtin_expect_with_probability) && !defined(JSON_HEDLEY_PGI_VERSION)) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(9,0,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define JSON_HEDLEY_PREDICT(expr, value, probability) __builtin_expect_with_probability(  (expr), (value), (probability))
#  define JSON_HEDLEY_PREDICT_TRUE(expr, probability)   __builtin_expect_with_probability(!!(expr),    1   , (probability))
#  define JSON_HEDLEY_PREDICT_FALSE(expr, probability)  __builtin_expect_with_probability(!!(expr),    0   , (probability))
#  define JSON_HEDLEY_LIKELY(expr)                      __builtin_expect(!!(expr),    1                  )
#  define JSON_HEDLEY_UNLIKELY(expr)                    __builtin_expect(!!(expr),    0                  )
#elif \
  (JSON_HEDLEY_HAS_BUILTIN(__builtin_expect) && !defined(JSON_HEDLEY_INTEL_CL_VERSION)) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(3,0,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0) && defined(__cplusplus)) || \
  JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,7,0) || \
  JSON_HEDLEY_TI_CL430_VERSION_CHECK(3,1,0) || \
  JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,1,0) || \
  JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
  JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,27) || \
  JSON_HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define JSON_HEDLEY_PREDICT(expr, expected, probability) \
    (((probability) >= 0.9) ? __builtin_expect((expr), (expected)) : (JSON_HEDLEY_STATIC_CAST(void, expected), (expr)))
#  define JSON_HEDLEY_PREDICT_TRUE(expr, probability) \
    (__extension__ ({ \
        double hedley_probability_ = (probability); \
        ((hedley_probability_ >= 0.9) ? __builtin_expect(!!(expr), 1) : ((hedley_probability_ <= 0.1) ? __builtin_expect(!!(expr), 0) : !!(expr))); \
    }))
#  define JSON_HEDLEY_PREDICT_FALSE(expr, probability) \
    (__extension__ ({ \
        double hedley_probability_ = (probability); \
        ((hedley_probability_ >= 0.9) ? __builtin_expect(!!(expr), 0) : ((hedley_probability_ <= 0.1) ? __builtin_expect(!!(expr), 1) : !!(expr))); \
    }))
#  define JSON_HEDLEY_LIKELY(expr)   __builtin_expect(!!(expr), 1)
#  define JSON_HEDLEY_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
#  define JSON_HEDLEY_PREDICT(expr, expected, probability) (JSON_HEDLEY_STATIC_CAST(void, expected), (expr))
#  define JSON_HEDLEY_PREDICT_TRUE(expr, probability) (!!(expr))
#  define JSON_HEDLEY_PREDICT_FALSE(expr, probability) (!!(expr))
#  define JSON_HEDLEY_LIKELY(expr) (!!(expr))
#  define JSON_HEDLEY_UNLIKELY(expr) (!!(expr))
#endif
#if !defined(JSON_HEDLEY_UNPREDICTABLE)
    #define JSON_HEDLEY_UNPREDICTABLE(expr) JSON_HEDLEY_PREDICT(expr, 1, 0.5)
#endif

#if defined(JSON_HEDLEY_MALLOC)
    #undef JSON_HEDLEY_MALLOC
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(malloc) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(12,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_MALLOC __attribute__((__malloc__))
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
    #define JSON_HEDLEY_MALLOC _Pragma("returns_new_memory")
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_MALLOC __declspec(restrict)
#else
    #define JSON_HEDLEY_MALLOC
#endif

#if defined(JSON_HEDLEY_PURE)
    #undef JSON_HEDLEY_PURE
#endif
#if \
  JSON_HEDLEY_HAS_ATTRIBUTE(pure) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(2,96,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define JSON_HEDLEY_PURE __attribute__((__pure__))
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#  define JSON_HEDLEY_PURE _Pragma("does_not_write_global_data")
#elif defined(__cplusplus) && \
    ( \
      JSON_HEDLEY_TI_CL430_VERSION_CHECK(2,0,1) || \
      JSON_HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0) || \
      JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) \
    )
#  define JSON_HEDLEY_PURE _Pragma("FUNC_IS_PURE;")
#else
#  define JSON_HEDLEY_PURE
#endif

#if defined(JSON_HEDLEY_CONST)
    #undef JSON_HEDLEY_CONST
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(const) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(2,5,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_CONST __attribute__((__const__))
#elif \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
    #define JSON_HEDLEY_CONST _Pragma("no_side_effect")
#else
    #define JSON_HEDLEY_CONST JSON_HEDLEY_PURE
#endif

#if defined(JSON_HEDLEY_RESTRICT)
    #undef JSON_HEDLEY_RESTRICT
#endif
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && !defined(__cplusplus)
    #define JSON_HEDLEY_RESTRICT restrict
#elif \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,4) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,1,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,14,0) && defined(__cplusplus)) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0) || \
    defined(__clang__) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_RESTRICT __restrict
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,3,0) && !defined(__cplusplus)
    #define JSON_HEDLEY_RESTRICT _Restrict
#else
    #define JSON_HEDLEY_RESTRICT
#endif

#if defined(JSON_HEDLEY_INLINE)
    #undef JSON_HEDLEY_INLINE
#endif
#if \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
    (defined(__cplusplus) && (__cplusplus >= 199711L))
    #define JSON_HEDLEY_INLINE inline
#elif \
    defined(JSON_HEDLEY_GCC_VERSION) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(6,2,0)
    #define JSON_HEDLEY_INLINE __inline__
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(12,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,1,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_INLINE __inline
#else
    #define JSON_HEDLEY_INLINE
#endif

#if defined(JSON_HEDLEY_ALWAYS_INLINE)
    #undef JSON_HEDLEY_ALWAYS_INLINE
#endif
#if \
  JSON_HEDLEY_HAS_ATTRIBUTE(always_inline) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
  JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
#  define JSON_HEDLEY_ALWAYS_INLINE __attribute__((__always_inline__)) JSON_HEDLEY_INLINE
#elif \
  JSON_HEDLEY_MSVC_VERSION_CHECK(12,0,0) || \
  JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define JSON_HEDLEY_ALWAYS_INLINE __forceinline
#elif defined(__cplusplus) && \
    ( \
      JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
      JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
      JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
      JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
      JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
      JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) \
    )
#  define JSON_HEDLEY_ALWAYS_INLINE _Pragma("FUNC_ALWAYS_INLINE;")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define JSON_HEDLEY_ALWAYS_INLINE _Pragma("inline=forced")
#else
#  define JSON_HEDLEY_ALWAYS_INLINE JSON_HEDLEY_INLINE
#endif

#if defined(JSON_HEDLEY_NEVER_INLINE)
    #undef JSON_HEDLEY_NEVER_INLINE
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(noinline) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
    #define JSON_HEDLEY_NEVER_INLINE __attribute__((__noinline__))
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_NEVER_INLINE __declspec(noinline)
#elif JSON_HEDLEY_PGI_VERSION_CHECK(10,2,0)
    #define JSON_HEDLEY_NEVER_INLINE _Pragma("noinline")
#elif JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,0,0) && defined(__cplusplus)
    #define JSON_HEDLEY_NEVER_INLINE _Pragma("FUNC_CANNOT_INLINE;")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
    #define JSON_HEDLEY_NEVER_INLINE _Pragma("inline=never")
#elif JSON_HEDLEY_COMPCERT_VERSION_CHECK(3,2,0)
    #define JSON_HEDLEY_NEVER_INLINE __attribute((noinline))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(9,0,0)
    #define JSON_HEDLEY_NEVER_INLINE __declspec(noinline)
#else
    #define JSON_HEDLEY_NEVER_INLINE
#endif

#if defined(JSON_HEDLEY_PRIVATE)
    #undef JSON_HEDLEY_PRIVATE
#endif
#if defined(JSON_HEDLEY_PUBLIC)
    #undef JSON_HEDLEY_PUBLIC
#endif
#if defined(JSON_HEDLEY_IMPORT)
    #undef JSON_HEDLEY_IMPORT
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#  define JSON_HEDLEY_PRIVATE
#  define JSON_HEDLEY_PUBLIC   __declspec(dllexport)
#  define JSON_HEDLEY_IMPORT   __declspec(dllimport)
#else
#  if \
    JSON_HEDLEY_HAS_ATTRIBUTE(visibility) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
    ( \
      defined(__TI_EABI__) && \
      ( \
        (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
        JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) \
      ) \
    ) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#    define JSON_HEDLEY_PRIVATE __attribute__((__visibility__("hidden")))
#    define JSON_HEDLEY_PUBLIC  __attribute__((__visibility__("default")))
#  else
#    define JSON_HEDLEY_PRIVATE
#    define JSON_HEDLEY_PUBLIC
#  endif
#  define JSON_HEDLEY_IMPORT    extern
#endif

#if defined(JSON_HEDLEY_NO_THROW)
    #undef JSON_HEDLEY_NO_THROW
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(nothrow) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_NO_THROW __attribute__((__nothrow__))
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,1,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0)
    #define JSON_HEDLEY_NO_THROW __declspec(nothrow)
#else
    #define JSON_HEDLEY_NO_THROW
#endif

#if defined(JSON_HEDLEY_FALL_THROUGH)
    #undef JSON_HEDLEY_FALL_THROUGH
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(fallthrough) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(7,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_FALL_THROUGH __attribute__((__fallthrough__))
#elif JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(clang,fallthrough)
    #define JSON_HEDLEY_FALL_THROUGH JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[clang::fallthrough]])
#elif JSON_HEDLEY_HAS_CPP_ATTRIBUTE(fallthrough)
    #define JSON_HEDLEY_FALL_THROUGH JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[fallthrough]])
#elif defined(__fallthrough) /* SAL */
    #define JSON_HEDLEY_FALL_THROUGH __fallthrough
#else
    #define JSON_HEDLEY_FALL_THROUGH
#endif

#if defined(JSON_HEDLEY_RETURNS_NON_NULL)
    #undef JSON_HEDLEY_RETURNS_NON_NULL
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(returns_nonnull) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,9,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_RETURNS_NON_NULL __attribute__((__returns_nonnull__))
#elif defined(_Ret_notnull_) /* SAL */
    #define JSON_HEDLEY_RETURNS_NON_NULL _Ret_notnull_
#else
    #define JSON_HEDLEY_RETURNS_NON_NULL
#endif

#if defined(JSON_HEDLEY_ARRAY_PARAM)
    #undef JSON_HEDLEY_ARRAY_PARAM
#endif
#if \
    defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && \
    !defined(__STDC_NO_VLA__) && \
    !defined(__cplusplus) && \
    !defined(JSON_HEDLEY_PGI_VERSION) && \
    !defined(JSON_HEDLEY_TINYC_VERSION)
    #define JSON_HEDLEY_ARRAY_PARAM(name) (name)
#else
    #define JSON_HEDLEY_ARRAY_PARAM(name)
#endif

#if defined(JSON_HEDLEY_IS_CONSTANT)
    #undef JSON_HEDLEY_IS_CONSTANT
#endif
#if defined(JSON_HEDLEY_REQUIRE_CONSTEXPR)
    #undef JSON_HEDLEY_REQUIRE_CONSTEXPR
#endif
/* JSON_HEDLEY_IS_CONSTEXPR_ is for
   HEDLEY INTERNAL USE ONLY.  API subject to change without notice. */
#if defined(JSON_HEDLEY_IS_CONSTEXPR_)
    #undef JSON_HEDLEY_IS_CONSTEXPR_
#endif
#if \
    JSON_HEDLEY_HAS_BUILTIN(__builtin_constant_p) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,19) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
    (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0) && !defined(__cplusplus)) || \
    JSON_HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
    #define JSON_HEDLEY_IS_CONSTANT(expr) __builtin_constant_p(expr)
#endif
#if !defined(__cplusplus)
#  if \
       JSON_HEDLEY_HAS_BUILTIN(__builtin_types_compatible_p) || \
       JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
       JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
       JSON_HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
       JSON_HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
       JSON_HEDLEY_ARM_VERSION_CHECK(5,4,0) || \
       JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,24)
#if defined(__INTPTR_TYPE__)
    #define JSON_HEDLEY_IS_CONSTEXPR_(expr) __builtin_types_compatible_p(__typeof__((1 ? (void*) ((__INTPTR_TYPE__) ((expr) * 0)) : (int*) 0)), int*)
#else
    #include <stdint.h>
    #define JSON_HEDLEY_IS_CONSTEXPR_(expr) __builtin_types_compatible_p(__typeof__((1 ? (void*) ((intptr_t) ((expr) * 0)) : (int*) 0)), int*)
#endif
#  elif \
       ( \
          defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && \
          !defined(JSON_HEDLEY_SUNPRO_VERSION) && \
          !defined(JSON_HEDLEY_PGI_VERSION) && \
          !defined(JSON_HEDLEY_IAR_VERSION)) || \
       (JSON_HEDLEY_HAS_EXTENSION(c_generic_selections) && !defined(JSON_HEDLEY_IAR_VERSION)) || \
       JSON_HEDLEY_GCC_VERSION_CHECK(4,9,0) || \
       JSON_HEDLEY_INTEL_VERSION_CHECK(17,0,0) || \
       JSON_HEDLEY_IBM_VERSION_CHECK(12,1,0) || \
       JSON_HEDLEY_ARM_VERSION_CHECK(5,3,0)
#if defined(__INTPTR_TYPE__)
    #define JSON_HEDLEY_IS_CONSTEXPR_(expr) _Generic((1 ? (void*) ((__INTPTR_TYPE__) ((expr) * 0)) : (int*) 0), int*: 1, void*: 0)
#else
    #include <stdint.h>
    #define JSON_HEDLEY_IS_CONSTEXPR_(expr) _Generic((1 ? (void*) ((intptr_t) * 0) : (int*) 0), int*: 1, void*: 0)
#endif
#  elif \
       defined(JSON_HEDLEY_GCC_VERSION) || \
       defined(JSON_HEDLEY_INTEL_VERSION) || \
       defined(JSON_HEDLEY_TINYC_VERSION) || \
       defined(JSON_HEDLEY_TI_ARMCL_VERSION) || \
       JSON_HEDLEY_TI_CL430_VERSION_CHECK(18,12,0) || \
       defined(JSON_HEDLEY_TI_CL2000_VERSION) || \
       defined(JSON_HEDLEY_TI_CL6X_VERSION) || \
       defined(JSON_HEDLEY_TI_CL7X_VERSION) || \
       defined(JSON_HEDLEY_TI_CLPRU_VERSION) || \
       defined(__clang__)
#    define JSON_HEDLEY_IS_CONSTEXPR_(expr) ( \
        sizeof(void) != \
        sizeof(*( \
                  1 ? \
                  ((void*) ((expr) * 0L) ) : \
                  ((struct { char v[sizeof(void) * 2]; } *) 1) \
                ) \
              ) \
                                            )
#  endif
#endif
#if defined(JSON_HEDLEY_IS_CONSTEXPR_)
    #if !defined(JSON_HEDLEY_IS_CONSTANT)
        #define JSON_HEDLEY_IS_CONSTANT(expr) JSON_HEDLEY_IS_CONSTEXPR_(expr)
    #endif
    #define JSON_HEDLEY_REQUIRE_CONSTEXPR(expr) (JSON_HEDLEY_IS_CONSTEXPR_(expr) ? (expr) : (-1))
#else
    #if !defined(JSON_HEDLEY_IS_CONSTANT)
        #define JSON_HEDLEY_IS_CONSTANT(expr) (0)
    #endif
    #define JSON_HEDLEY_REQUIRE_CONSTEXPR(expr) (expr)
#endif

#if defined(JSON_HEDLEY_BEGIN_C_DECLS)
    #undef JSON_HEDLEY_BEGIN_C_DECLS
#endif
#if defined(JSON_HEDLEY_END_C_DECLS)
    #undef JSON_HEDLEY_END_C_DECLS
#endif
#if defined(JSON_HEDLEY_C_DECL)
    #undef JSON_HEDLEY_C_DECL
#endif
#if defined(__cplusplus)
    #define JSON_HEDLEY_BEGIN_C_DECLS extern "C" {
    #define JSON_HEDLEY_END_C_DECLS }
    #define JSON_HEDLEY_C_DECL extern "C"
#else
    #define JSON_HEDLEY_BEGIN_C_DECLS
    #define JSON_HEDLEY_END_C_DECLS
    #define JSON_HEDLEY_C_DECL
#endif

#if defined(JSON_HEDLEY_STATIC_ASSERT)
    #undef JSON_HEDLEY_STATIC_ASSERT
#endif
#if \
  !defined(__cplusplus) && ( \
      (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
      (JSON_HEDLEY_HAS_FEATURE(c_static_assert) && !defined(JSON_HEDLEY_INTEL_CL_VERSION)) || \
      JSON_HEDLEY_GCC_VERSION_CHECK(6,0,0) || \
      JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      defined(_Static_assert) \
    )
#  define JSON_HEDLEY_STATIC_ASSERT(expr, message) _Static_assert(expr, message)
#elif \
  (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
  JSON_HEDLEY_MSVC_VERSION_CHECK(16,0,0) || \
  JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define JSON_HEDLEY_STATIC_ASSERT(expr, message) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(static_assert(expr, message))
#else
#  define JSON_HEDLEY_STATIC_ASSERT(expr, message)
#endif

#if defined(JSON_HEDLEY_NULL)
    #undef JSON_HEDLEY_NULL
#endif
#if defined(__cplusplus)
    #if __cplusplus >= 201103L
        #define JSON_HEDLEY_NULL JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(nullptr)
    #elif defined(NULL)
        #define JSON_HEDLEY_NULL NULL
    #else
        #define JSON_HEDLEY_NULL JSON_HEDLEY_STATIC_CAST(void*, 0)
    #endif
#elif defined(NULL)
    #define JSON_HEDLEY_NULL NULL
#else
    #define JSON_HEDLEY_NULL ((void*) 0)
#endif

#if defined(JSON_HEDLEY_MESSAGE)
    #undef JSON_HEDLEY_MESSAGE
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#  define JSON_HEDLEY_MESSAGE(msg) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
    JSON_HEDLEY_PRAGMA(message msg) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#elif \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,4,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(message msg)
#elif JSON_HEDLEY_CRAY_VERSION_CHECK(5,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(_CRI message msg)
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(message(msg))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(2,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(message(msg))
#else
#  define JSON_HEDLEY_MESSAGE(msg)
#endif

#if defined(JSON_HEDLEY_WARNING)
    #undef JSON_HEDLEY_WARNING
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#  define JSON_HEDLEY_WARNING(msg) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
    JSON_HEDLEY_PRAGMA(clang warning msg) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#elif \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,8,0) || \
  JSON_HEDLEY_PGI_VERSION_CHECK(18,4,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define JSON_HEDLEY_WARNING(msg) JSON_HEDLEY_PRAGMA(GCC warning msg)
#elif \
  JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0) || \
  JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define JSON_HEDLEY_WARNING(msg) JSON_HEDLEY_PRAGMA(message(msg))
#else
#  define JSON_HEDLEY_WARNING(msg) JSON_HEDLEY_MESSAGE(msg)
#endif

#if defined(JSON_HEDLEY_REQUIRE)
    #undef JSON_HEDLEY_REQUIRE
#endif
#if defined(JSON_HEDLEY_REQUIRE_MSG)
    #undef JSON_HEDLEY_REQUIRE_MSG
#endif
#if JSON_HEDLEY_HAS_ATTRIBUTE(diagnose_if)
#  if JSON_HEDLEY_HAS_WARNING("-Wgcc-compat")
#    define JSON_HEDLEY_REQUIRE(expr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wgcc-compat\"") \
    __attribute__((diagnose_if(!(expr), #expr, "error"))) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#    define JSON_HEDLEY_REQUIRE_MSG(expr,msg) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wgcc-compat\"") \
    __attribute__((diagnose_if(!(expr), msg, "error"))) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#  else
#    define JSON_HEDLEY_REQUIRE(expr) __attribute__((diagnose_if(!(expr), #expr, "error")))
#    define JSON_HEDLEY_REQUIRE_MSG(expr,msg) __attribute__((diagnose_if(!(expr), msg, "error")))
#  endif
#else
#  define JSON_HEDLEY_REQUIRE(expr)
#  define JSON_HEDLEY_REQUIRE_MSG(expr,msg)
#endif

#if defined(JSON_HEDLEY_FLAGS)
    #undef JSON_HEDLEY_FLAGS
#endif
#if JSON_HEDLEY_HAS_ATTRIBUTE(flag_enum) && (!defined(__cplusplus) || JSON_HEDLEY_HAS_WARNING("-Wbitfield-enum-conversion"))
    #define JSON_HEDLEY_FLAGS __attribute__((__flag_enum__))
#else
    #define JSON_HEDLEY_FLAGS
#endif

#if defined(JSON_HEDLEY_FLAGS_CAST)
    #undef JSON_HEDLEY_FLAGS_CAST
#endif
#if JSON_HEDLEY_INTEL_VERSION_CHECK(19,0,0)
#  define JSON_HEDLEY_FLAGS_CAST(T, expr) (__extension__ ({ \
        JSON_HEDLEY_DIAGNOSTIC_PUSH \
        _Pragma("warning(disable:188)") \
        ((T) (expr)); \
        JSON_HEDLEY_DIAGNOSTIC_POP \
    }))
#else
#  define JSON_HEDLEY_FLAGS_CAST(T, expr) JSON_HEDLEY_STATIC_CAST(T, expr)
#endif

#if defined(JSON_HEDLEY_EMPTY_BASES)
    #undef JSON_HEDLEY_EMPTY_BASES
#endif
#if \
    (JSON_HEDLEY_MSVC_VERSION_CHECK(19,0,23918) && !JSON_HEDLEY_MSVC_VERSION_CHECK(20,0,0)) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
    #define JSON_HEDLEY_EMPTY_BASES __declspec(empty_bases)
#else
    #define JSON_HEDLEY_EMPTY_BASES
#endif

/* Remaining macros are deprecated. */

#if defined(JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK)
    #undef JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK
#endif
#if defined(__clang__)
    #define JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major,minor,patch) (0)
#else
    #define JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(JSON_HEDLEY_CLANG_HAS_ATTRIBUTE)
    #undef JSON_HEDLEY_CLANG_HAS_ATTRIBUTE
#endif
#define JSON_HEDLEY_CLANG_HAS_ATTRIBUTE(attribute) JSON_HEDLEY_HAS_ATTRIBUTE(attribute)

#if defined(JSON_HEDLEY_CLANG_HAS_CPP_ATTRIBUTE)
    #undef JSON_HEDLEY_CLANG_HAS_CPP_ATTRIBUTE
#endif
#define JSON_HEDLEY_CLANG_HAS_CPP_ATTRIBUTE(attribute) JSON_HEDLEY_HAS_CPP_ATTRIBUTE(attribute)

#if defined(JSON_HEDLEY_CLANG_HAS_BUILTIN)
    #undef JSON_HEDLEY_CLANG_HAS_BUILTIN
#endif
#define JSON_HEDLEY_CLANG_HAS_BUILTIN(builtin) JSON_HEDLEY_HAS_BUILTIN(builtin)

#if defined(JSON_HEDLEY_CLANG_HAS_FEATURE)
    #undef JSON_HEDLEY_CLANG_HAS_FEATURE
#endif
#define JSON_HEDLEY_CLANG_HAS_FEATURE(feature) JSON_HEDLEY_HAS_FEATURE(feature)

#if defined(JSON_HEDLEY_CLANG_HAS_EXTENSION)
    #undef JSON_HEDLEY_CLANG_HAS_EXTENSION
#endif
#define JSON_HEDLEY_CLANG_HAS_EXTENSION(extension) JSON_HEDLEY_HAS_EXTENSION(extension)

#if defined(JSON_HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE)
    #undef JSON_HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE
#endif
#define JSON_HEDLEY_CLANG_HAS_DECLSPEC_ATTRIBUTE(attribute) JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute)

#if defined(JSON_HEDLEY_CLANG_HAS_WARNING)
    #undef JSON_HEDLEY_CLANG_HAS_WARNING
#endif
#define JSON_HEDLEY_CLANG_HAS_WARNING(warning) JSON_HEDLEY_HAS_WARNING(warning)

#endif /* !defined(JSON_HEDLEY_VERSION) || (JSON_HEDLEY_VERSION < X) */


// This file contains all internal macro definitions (except those affecting ABI)
// You MUST include macro_unscope.hpp at the end of json.hpp to undef all of them

// #include <nlohmann/detail/abi_macros.hpp>


// exclude unsupported compilers
#if !defined(JSON_SKIP_UNSUPPORTED_COMPILER_CHECK)
    #if defined(__clang__)
        #if (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__) < 30400
            #error "unsupported Clang version - see https://github.com/nlohmann/json#supported-compilers"
        #endif
    #elif defined(__GNUC__) && !(defined(__ICC) || defined(__INTEL_COMPILER))
        #if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) < 40800
            #error "unsupported GCC version - see https://github.com/nlohmann/json#supported-compilers"
        #endif
    #endif
#endif

// C++ language standard detection
// if the user manually specified the used C++ version, this is skipped
#if !defined(JSON_HAS_CPP_26) && !defined(JSON_HAS_CPP_23) && !defined(JSON_HAS_CPP_20) && !defined(JSON_HAS_CPP_17) && !defined(JSON_HAS_CPP_14) && !defined(JSON_HAS_CPP_11)
    #if (defined(__cplusplus) && __cplusplus > 202302L) || (defined(_MSVC_LANG) && _MSVC_LANG > 202302L)
        #define JSON_HAS_CPP_26
        #define JSON_HAS_CPP_23
        #define JSON_HAS_CPP_20
        #define JSON_HAS_CPP_17
        #define JSON_HAS_CPP_14
    #elif (defined(__cplusplus) && __cplusplus > 202002L) || (defined(_MSVC_LANG) && _MSVC_LANG > 202002L)
        #define JSON_HAS_CPP_23
        #define JSON_HAS_CPP_20
        #define JSON_HAS_CPP_17
        #define JSON_HAS_CPP_14
    #elif (defined(__cplusplus) && __cplusplus > 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG > 201703L)
        #define JSON_HAS_CPP_20
        #define JSON_HAS_CPP_17
        #define JSON_HAS_CPP_14
    #elif (defined(__cplusplus) && __cplusplus > 201402L) || (defined(_HAS_CXX17) && _HAS_CXX17 == 1) // fix for issue #464
        #define JSON_HAS_CPP_17
        #define JSON_HAS_CPP_14
    #elif (defined(__cplusplus) && __cplusplus > 201103L) || (defined(_HAS_CXX14) && _HAS_CXX14 == 1)
        #define JSON_HAS_CPP_14
    #endif
    // the cpp 11 flag is always specified because it is the minimal required version
    #define JSON_HAS_CPP_11
#endif

#ifdef __has_include
    #if __has_include(<version>)
        #include <version>
    #endif
#endif

#if !defined(JSON_HAS_FILESYSTEM) && !defined(JSON_HAS_EXPERIMENTAL_FILESYSTEM)
    #ifdef JSON_HAS_CPP_17
        #if defined(__cpp_lib_filesystem)
            #define JSON_HAS_FILESYSTEM 1
        #elif defined(__cpp_lib_experimental_filesystem)
            #define JSON_HAS_EXPERIMENTAL_FILESYSTEM 1
        #elif !defined(__has_include)
            #define JSON_HAS_EXPERIMENTAL_FILESYSTEM 1
        #elif __has_include(<filesystem>)
            #define JSON_HAS_FILESYSTEM 1
        #elif __has_include(<experimental/filesystem>)
            #define JSON_HAS_EXPERIMENTAL_FILESYSTEM 1
        #endif

        // std::filesystem does not work on MinGW GCC 8: https://sourceforge.net/p/mingw-w64/bugs/737/
        #if defined(__MINGW32__) && defined(__GNUC__) && __GNUC__ == 8
            #undef JSON_HAS_FILESYSTEM
            #undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
        #endif

        // no filesystem support before GCC 8: https://en.cppreference.com/w/cpp/compiler_support
        #if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 8
            #undef JSON_HAS_FILESYSTEM
            #undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
        #endif

        // no filesystem support before Clang 7: https://en.cppreference.com/w/cpp/compiler_support
        #if defined(__clang_major__) && __clang_major__ < 7
            #undef JSON_HAS_FILESYSTEM
            #undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
        #endif

        // no filesystem support before MSVC 19.14: https://en.cppreference.com/w/cpp/compiler_support
        #if defined(_MSC_VER) && _MSC_VER < 1914
            #undef JSON_HAS_FILESYSTEM
            #undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
        #endif

        // no filesystem support before iOS 13
        #if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 130000
            #undef JSON_HAS_FILESYSTEM
            #undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
        #endif

        // no filesystem support before macOS Catalina
        #if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
            #undef JSON_HAS_FILESYSTEM
            #undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
        #endif
    #endif
#endif

#ifndef JSON_HAS_EXPERIMENTAL_FILESYSTEM
    #define JSON_HAS_EXPERIMENTAL_FILESYSTEM 0
#endif

#ifndef JSON_HAS_FILESYSTEM
    #define JSON_HAS_FILESYSTEM 0
#endif

#ifndef JSON_HAS_THREE_WAY_COMPARISON
    #if defined(__cpp_impl_three_way_comparison) && __cpp_impl_three_way_comparison >= 201907L \
        && defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L
        #define JSON_HAS_THREE_WAY_COMPARISON 1
    #else
        #define JSON_HAS_THREE_WAY_COMPARISON 0
    #endif
#endif

#ifndef JSON_HAS_RANGES
    // ranges header shipping in GCC 11.1.0 (released 2021-04-27) has a syntax error
    #if defined(__GLIBCXX__) && __GLIBCXX__ == 20210427
        #define JSON_HAS_RANGES 0
    #elif defined(__cpp_lib_ranges)
        #define JSON_HAS_RANGES 1
    #else
        #define JSON_HAS_RANGES 0
    #endif
#endif

#ifndef JSON_HAS_STATIC_RTTI
    #if !defined(_HAS_STATIC_RTTI) || _HAS_STATIC_RTTI != 0
        #define JSON_HAS_STATIC_RTTI 1
    #else
        #define JSON_HAS_STATIC_RTTI 0
    #endif
#endif

#ifdef JSON_HAS_CPP_17
    #define JSON_INLINE_VARIABLE inline
#else
    #define JSON_INLINE_VARIABLE
#endif

#if JSON_HEDLEY_HAS_ATTRIBUTE(no_unique_address)
    #define JSON_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
    #define JSON_NO_UNIQUE_ADDRESS
#endif

// disable documentation warnings on clang
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdocumentation"
    #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#endif

// allow disabling exceptions
#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)) && !defined(JSON_NOEXCEPTION)
    #define JSON_THROW(exception) throw exception
    #define JSON_TRY try
    #define JSON_CATCH(exception) catch(exception)
    #define JSON_INTERNAL_CATCH(exception) catch(exception)
#else
    #include <cstdlib>
    #define JSON_THROW(exception) std::abort()
    #define JSON_TRY if(true)
    #define JSON_CATCH(exception) if(false)
    #define JSON_INTERNAL_CATCH(exception) if(false)
#endif

// override exception macros
#if defined(JSON_THROW_USER)
    #undef JSON_THROW
    #define JSON_THROW JSON_THROW_USER
#endif
#if defined(JSON_TRY_USER)
    #undef JSON_TRY
    #define JSON_TRY JSON_TRY_USER
#endif
#if defined(JSON_CATCH_USER)
    #undef JSON_CATCH
    #define JSON_CATCH JSON_CATCH_USER
    #undef JSON_INTERNAL_CATCH
    #define JSON_INTERNAL_CATCH JSON_CATCH_USER
#endif
#if defined(JSON_INTERNAL_CATCH_USER)
    #undef JSON_INTERNAL_CATCH
    #define JSON_INTERNAL_CATCH JSON_INTERNAL_CATCH_USER
#endif

// allow overriding assert
#if !defined(JSON_ASSERT)
    #include <cassert> // assert
    #define JSON_ASSERT(x) assert(x)
#endif

// allow accessing some private functions (needed by the test suite)
#if defined(JSON_TESTS_PRIVATE)
    #define JSON_PRIVATE_UNLESS_TESTED public
#else
    #define JSON_PRIVATE_UNLESS_TESTED private
#endif

/*!
@brief macro to briefly define a mapping between an enum and JSON
@def NLOHMANN_JSON_SERIALIZE_ENUM
@since version 3.4.0
*/
#define NLOHMANN_JSON_SERIALIZE_ENUM(ENUM_TYPE, ...) \
    template<typename BasicJsonType> \
    inline void to_json(BasicJsonType& j, const ENUM_TYPE& e) \
    { \
        /* NOLINTNEXTLINE(modernize-type-traits) we use C++11 */ \
        static_assert(std::is_enum<ENUM_TYPE>::value, #ENUM_TYPE " must be an enum!"); \
        /* NOLINTNEXTLINE(modernize-avoid-c-arrays) we don't want to depend on <array> */ \
        static const std::pair<ENUM_TYPE, BasicJsonType> m[] = __VA_ARGS__; \
        auto it = std::find_if(std::begin(m), std::end(m), \
                               [e](const std::pair<ENUM_TYPE, BasicJsonType>& ej_pair) -> bool \
        { \
            return ej_pair.first == e; \
        }); \
        j = ((it != std::end(m)) ? it : std::begin(m))->second; \
    } \
    template<typename BasicJsonType> \
    inline void from_json(const BasicJsonType& j, ENUM_TYPE& e) \
    { \
        /* NOLINTNEXTLINE(modernize-type-traits) we use C++11 */ \
        static_assert(std::is_enum<ENUM_TYPE>::value, #ENUM_TYPE " must be an enum!"); \
        /* NOLINTNEXTLINE(modernize-avoid-c-arrays) we don't want to depend on <array> */ \
        static const std::pair<ENUM_TYPE, BasicJsonType> m[] = __VA_ARGS__; \
        auto it = std::find_if(std::begin(m), std::end(m), \
                               [&j](const std::pair<ENUM_TYPE, BasicJsonType>& ej_pair) -> bool \
        { \
            return ej_pair.second == j; \
        }); \
        e = ((it != std::end(m)) ? it : std::begin(m))->first; \
    }

// Ugly macros to avoid uglier copy-paste when specializing basic_json. They
// may be removed in the future once the class is split.

#define NLOHMANN_BASIC_JSON_TPL_DECLARATION                                \
    template<template<typename, typename, typename...> class ObjectType,   \
             template<typename, typename...> class ArrayType,              \
             class StringType, class BooleanType, class NumberIntegerType, \
             class NumberUnsignedType, class NumberFloatType,              \
             template<typename> class AllocatorType,                       \
             template<typename, typename = void> class JSONSerializer,     \
             class BinaryType,                                             \
             class CustomBaseClass>

#define NLOHMANN_BASIC_JSON_TPL                                            \
    basic_json<ObjectType, ArrayType, StringType, BooleanType,             \
    NumberIntegerType, NumberUnsignedType, NumberFloatType,                \
    AllocatorType, JSONSerializer, BinaryType, CustomBaseClass>

// Macros to simplify conversion from/to types

#define NLOHMANN_JSON_EXPAND( x ) x
#define NLOHMANN_JSON_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, NAME,...) NAME
#define NLOHMANN_JSON_PASTE(...) NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_GET_MACRO(__VA_ARGS__, \
        NLOHMANN_JSON_PASTE64, \
        NLOHMANN_JSON_PASTE63, \
        NLOHMANN_JSON_PASTE62, \
        NLOHMANN_JSON_PASTE61, \
        NLOHMANN_JSON_PASTE60, \
        NLOHMANN_JSON_PASTE59, \
        NLOHMANN_JSON_PASTE58, \
        NLOHMANN_JSON_PASTE57, \
        NLOHMANN_JSON_PASTE56, \
        NLOHMANN_JSON_PASTE55, \
        NLOHMANN_JSON_PASTE54, \
        NLOHMANN_JSON_PASTE53, \
        NLOHMANN_JSON_PASTE52, \
        NLOHMANN_JSON_PASTE51, \
        NLOHMANN_JSON_PASTE50, \
        NLOHMANN_JSON_PASTE49, \
        NLOHMANN_JSON_PASTE48, \
        NLOHMANN_JSON_PASTE47, \
        NLOHMANN_JSON_PASTE46, \
        NLOHMANN_JSON_PASTE45, \
        NLOHMANN_JSON_PASTE44, \
        NLOHMANN_JSON_PASTE43, \
        NLOHMANN_JSON_PASTE42, \
        NLOHMANN_JSON_PASTE41, \
        NLOHMANN_JSON_PASTE40, \
        NLOHMANN_JSON_PASTE39, \
        NLOHMANN_JSON_PASTE38, \
        NLOHMANN_JSON_PASTE37, \
        NLOHMANN_JSON_PASTE36, \
        NLOHMANN_JSON_PASTE35, \
        NLOHMANN_JSON_PASTE34, \
        NLOHMANN_JSON_PASTE33, \
        NLOHMANN_JSON_PASTE32, \
        NLOHMANN_JSON_PASTE31, \
        NLOHMANN_JSON_PASTE30, \
        NLOHMANN_JSON_PASTE29, \
        NLOHMANN_JSON_PASTE28, \
        NLOHMANN_JSON_PASTE27, \
        NLOHMANN_JSON_PASTE26, \
        NLOHMANN_JSON_PASTE25, \
        NLOHMANN_JSON_PASTE24, \
        NLOHMANN_JSON_PASTE23, \
        NLOHMANN_JSON_PASTE22, \
        NLOHMANN_JSON_PASTE21, \
        NLOHMANN_JSON_PASTE20, \
        NLOHMANN_JSON_PASTE19, \
        NLOHMANN_JSON_PASTE18, \
        NLOHMANN_JSON_PASTE17, \
        NLOHMANN_JSON_PASTE16, \
        NLOHMANN_JSON_PASTE15, \
        NLOHMANN_JSON_PASTE14, \
        NLOHMANN_JSON_PASTE13, \
        NLOHMANN_JSON_PASTE12, \
        NLOHMANN_JSON_PASTE11, \
        NLOHMANN_JSON_PASTE10, \
        NLOHMANN_JSON_PASTE9, \
        NLOHMANN_JSON_PASTE8, \
        NLOHMANN_JSON_PASTE7, \
        NLOHMANN_JSON_PASTE6, \
        NLOHMANN_JSON_PASTE5, \
        NLOHMANN_JSON_PASTE4, \
        NLOHMANN_JSON_PASTE3, \
        NLOHMANN_JSON_PASTE2, \
        NLOHMANN_JSON_PASTE1)(__VA_ARGS__))
#define NLOHMANN_JSON_PASTE2(func, v1) func(v1)
#define NLOHMANN_JSON_PASTE3(func, v1, v2) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE2(func, v2)
#define NLOHMANN_JSON_PASTE4(func, v1, v2, v3) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE3(func, v2, v3)
#define NLOHMANN_JSON_PASTE5(func, v1, v2, v3, v4) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE4(func, v2, v3, v4)
#define NLOHMANN_JSON_PASTE6(func, v1, v2, v3, v4, v5) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE5(func, v2, v3, v4, v5)
#define NLOHMANN_JSON_PASTE7(func, v1, v2, v3, v4, v5, v6) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE6(func, v2, v3, v4, v5, v6)
#define NLOHMANN_JSON_PASTE8(func, v1, v2, v3, v4, v5, v6, v7) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define NLOHMANN_JSON_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define NLOHMANN_JSON_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE9(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define NLOHMANN_JSON_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE10(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)
#define NLOHMANN_JSON_PASTE12(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE11(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)
#define NLOHMANN_JSON_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE12(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define NLOHMANN_JSON_PASTE14(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE13(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)
#define NLOHMANN_JSON_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE14(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define NLOHMANN_JSON_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE15(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define NLOHMANN_JSON_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE16(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define NLOHMANN_JSON_PASTE18(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE17(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)
#define NLOHMANN_JSON_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE18(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define NLOHMANN_JSON_PASTE20(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE19(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)
#define NLOHMANN_JSON_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE20(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define NLOHMANN_JSON_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE21(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define NLOHMANN_JSON_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE22(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define NLOHMANN_JSON_PASTE24(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE23(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23)
#define NLOHMANN_JSON_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE24(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define NLOHMANN_JSON_PASTE26(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE25(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25)
#define NLOHMANN_JSON_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE26(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define NLOHMANN_JSON_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE27(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define NLOHMANN_JSON_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE28(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define NLOHMANN_JSON_PASTE30(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE29(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29)
#define NLOHMANN_JSON_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE30(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define NLOHMANN_JSON_PASTE32(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE31(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31)
#define NLOHMANN_JSON_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE32(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define NLOHMANN_JSON_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE33(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33)
#define NLOHMANN_JSON_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE34(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define NLOHMANN_JSON_PASTE36(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE35(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35)
#define NLOHMANN_JSON_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE36(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define NLOHMANN_JSON_PASTE38(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE37(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37)
#define NLOHMANN_JSON_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE38(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define NLOHMANN_JSON_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE39(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define NLOHMANN_JSON_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE40(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define NLOHMANN_JSON_PASTE42(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE41(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41)
#define NLOHMANN_JSON_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE42(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define NLOHMANN_JSON_PASTE44(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE43(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43)
#define NLOHMANN_JSON_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE44(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define NLOHMANN_JSON_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE45(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define NLOHMANN_JSON_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE46(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define NLOHMANN_JSON_PASTE48(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE47(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47)
#define NLOHMANN_JSON_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE48(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define NLOHMANN_JSON_PASTE50(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE49(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49)
#define NLOHMANN_JSON_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE50(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define NLOHMANN_JSON_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE51(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define NLOHMANN_JSON_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE52(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define NLOHMANN_JSON_PASTE54(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE53(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53)
#define NLOHMANN_JSON_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE54(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define NLOHMANN_JSON_PASTE56(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE55(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55)
#define NLOHMANN_JSON_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE56(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define NLOHMANN_JSON_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE57(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define NLOHMANN_JSON_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE58(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define NLOHMANN_JSON_PASTE60(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE59(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59)
#define NLOHMANN_JSON_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE60(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define NLOHMANN_JSON_PASTE62(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE61(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61)
#define NLOHMANN_JSON_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE62(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)
#define NLOHMANN_JSON_PASTE64(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE63(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63)

#define NLOHMANN_JSON_TO(v1) nlohmann_json_j[#v1] = nlohmann_json_t.v1;
#define NLOHMANN_JSON_FROM(v1) nlohmann_json_j.at(#v1).get_to(nlohmann_json_t.v1);
#define NLOHMANN_JSON_FROM_WITH_DEFAULT(v1) nlohmann_json_t.v1 = !nlohmann_json_j.is_null() ? nlohmann_json_j.value(#v1, nlohmann_json_default_obj.v1) : nlohmann_json_default_obj.v1;

/*!
@brief macro
@def NLOHMANN_DEFINE_TYPE_INTRUSIVE
@since version 3.9.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_type_intrusive/
*/
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT
@since version 3.11.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_type_intrusive/
*/
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Type, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { const Type nlohmann_json_default_obj{}; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE
@since version 3.11.3
@sa https://json.nlohmann.me/api/macros/nlohmann_define_type_intrusive/
*/
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Type, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
@since version 3.9.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_type_non_intrusive/
*/
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT
@since version 3.11.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_type_non_intrusive/
*/
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { const Type nlohmann_json_default_obj{}; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE
@since version 3.11.3
@sa https://json.nlohmann.me/api/macros/nlohmann_define_type_non_intrusive/
*/
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Type, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE
@since version 3.12.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_derived_type/
*/
#define NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(Type, BaseType, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { nlohmann::to_json(nlohmann_json_j, static_cast<const BaseType &>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { nlohmann::from_json(nlohmann_json_j, static_cast<BaseType&>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_WITH_DEFAULT
@since version 3.12.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_derived_type/
*/
#define NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_WITH_DEFAULT(Type, BaseType, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { nlohmann::to_json(nlohmann_json_j, static_cast<const BaseType&>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { nlohmann::from_json(nlohmann_json_j, static_cast<BaseType&>(nlohmann_json_t)); const Type nlohmann_json_default_obj{}; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_ONLY_SERIALIZE
@since version 3.12.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_derived_type/
*/
#define NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_ONLY_SERIALIZE(Type, BaseType, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { nlohmann::to_json(nlohmann_json_j, static_cast<const BaseType &>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE
@since version 3.12.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_derived_type/
*/
#define NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE(Type, BaseType, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { nlohmann::to_json(nlohmann_json_j, static_cast<const BaseType &>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { nlohmann::from_json(nlohmann_json_j, static_cast<BaseType&>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE_WITH_DEFAULT
@since version 3.12.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_derived_type/
*/
#define NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, BaseType, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { nlohmann::to_json(nlohmann_json_j, static_cast<const BaseType &>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { nlohmann::from_json(nlohmann_json_j, static_cast<BaseType&>(nlohmann_json_t)); const Type nlohmann_json_default_obj{}; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }

/*!
@brief macro
@def NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE
@since version 3.12.0
@sa https://json.nlohmann.me/api/macros/nlohmann_define_derived_type/
*/
#define NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Type, BaseType, ...)  \
    template<typename BasicJsonType, nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0> \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) { nlohmann::to_json(nlohmann_json_j, static_cast<const BaseType &>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }

// inspired from https://stackoverflow.com/a/26745591
// allows calling any std function as if (e.g., with begin):
// using