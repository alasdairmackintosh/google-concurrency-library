// Copyright 2009 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CXX11_H
#define CXX11_H

// See ../testing/cxx11_test.cc for examples of use.

#if defined(__GNUC__) && __GNUC__ == 4

#ifndef __APPLE__
#define HAS_CXX11_PRIMITIVE_THREAD_LOCAL
#endif

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
#if __GNUC_MINOR__ >= 3
#define HAS_CXX11_STATIC_ASSERT
#define HAS_CXX11_RVREF
#define HAS_CXX11_VARIADIC_TMPL
#define HAS_CXX11_DFLT_FUNC_TMPL_ARGS
#define HAS_CXX11_DECL_TYPE
#define HAS_CXX11_EXTERN_TEMPLATE
#define HAS_CXX11___FUNC__
#define HAS_CXX11_C99_PREPROCESSOR
#define HAS_CXX11_LONG_LONG
#endif
#if __GNUC_MINOR__ >= 4
#define HAS_CXX11_DEFAULTED
#define HAS_CXX11_DELETED
#define HAS_CXX11_AUTO_VAR
#define HAS_CXX11_NEW_FUNC_SYNTAX
#define HAS_CXX11_EXPLICIT_AGGR_INIT
#define HAS_CXX11_STRONG_ENUM
#define HAS_CXX11_VARIADIC_TMPL_TMPL
#define HAS_CXX11_INIT_LIST
#define HAS_CXX11_EXPR_SFINAE
#define HAS_CXX11_NEW_CHAR_TYPES
#define HAS_CXX11_EXTENDED_SIZEOF
#define HAS_CXX11_INLINE_NAMESPACE
#define HAS_CXX11_ATOMIC_OPS
#define HAS_CXX11_PROPOGATE_EXCEPT
#endif
#if __GNUC_MINOR__ >= 5
#define HAS_CXX11_EXPLICIT_CONV
#define HAS_CXX11_LAMBDA
#define HAS_CXX11_UNICODE_STRING_LIT
#define HAS_CXX11_RAW_STRING_LIT
#define HAS_CXX11_UCN_NAME_LIT
#define HAS_CXX11_STANDARD_LAYOUT
#define HAS_CXX11_LOCAL_TMPL_ARGS
#endif
#if __GNUC_MINOR__ >= 6
#define HAS_CXX11_MOVE_SPECIAL
#define HAS_CXX11_NULLPTR
#define HAS_CXX11_FORWARD_ENUM
#define HAS_CXX11_CONSTEXPR_VAR
#define HAS_CXX11_CONSTEXPR_FLD
#define HAS_CXX11_CONSTEXPR_FUN
#define HAS_CXX11_CONSTEXPR_CTOR
#define HAS_CXX11_UNRESTRICT_UNION
#define HAS_CXX11_RANGE_FOR
#define HAS_CXX11_CORE_NOEXCEPT
#endif
#if __GNUC_MINOR__ >= 7
#define HAS_CXX11_EXTENDED_FRIEND
#define HAS_CXX11_EXPLICIT_OVERRIDE
#endif
/* Unimplemented:
// #define HAS_CXX11_RVREF_THIS
// #define HAS_CXX11_FIELD_INIT
// #define HAS_CXX11_TMPL_ALIAS
// #define HAS_CXX11_USER_DEFINED_LIT
// #define HAS_CXX11_GARBAGE_COLLECTION
// #define HAS_CXX11_SEQUENCE_POINTS
// #define HAS_CXX11_DEPENDENCY_ORDERING
// #define HAS_CXX11_QUICK_EXIT
// #define HAS_CXX11_ATOMIC_IN_SIGNAL
// #define HAS_CXX11_TRIVIAL_THREAD_LOCAL
// #define HAS_CXX11_NON_TRIVIAL_THREAD_LOCAL
// #define HAS_CXX11_DYNAMIC_INIT_CONCUR
// #define HAS_CXX11_EXTENDED_INTEGRAL
// #define HAS_CXX11_TRIVIAL_PRIVATE
*/
#endif
#endif

namespace std {

// static assert

#ifdef HAS_CXX11_STATIC_ASSERT
#define CXX11_STATIC_ASSERT(EXPR, VARNAME) static_assert(EXPR, # VARNAME);
#define CXX11_CLASS_STATIC_ASSERT(EXPR, VARNAME) static_assert(EXPR, # VARNAME);
#else
template <bool t> class static_asserter;
template<> class static_asserter<true> { };
#define CXX11_STATIC_ASSERT(EXPR, VARNAME) \
std::static_asserter<EXPR> VARNAME;
#define CXX11_CLASS_STATIC_ASSERT(EXPR, VARNAME) \
static std::static_asserter<EXPR> VARNAME;
#endif

// defaulted functions

#ifdef HAS_CXX11_DEFAULTED
#define CXX11_DEFAULTED_EASY =default;
#define CXX11_DEFAULTED_HARD( BODY ) =default;
#else
#define CXX11_DEFAULTED_EASY { }
#define CXX11_DEFAULTED_HARD( BODY ) BODY
#endif

// deleted functions

#ifdef HAS_CXX11_DELETED
#define CXX11_DELETED =delete;
#else
#define CXX11_DELETED ;
#endif

// aggregate initialization

#ifdef HAS_CXX11_EXPLICIT_AGGR_INIT
#define CXX11_AGGR_INIT( DECLS ) DECLS
#define CXX11_NO_AGGR_INIT( DECLS )
#else
#define CXX11_AGGR_INIT( DECLS )
#define CXX11_NO_AGGR_INIT( DECLS ) DECLS
#endif

// private members in trivial classes

#ifdef HAS_CXX11_TRIVIAL_PRIVATE
#define CXX11_TRIVIAL_PRIVATE private:
#else
#define CXX11_TRIVIAL_PRIVATE
#endif

// auto variables

#ifdef HAS_CXX11_AUTO_VAR
#define CXX11_AUTO_VAR( VARNAME, EXPR ) auto VARNAME = EXPR
#else
#define CXX11_AUTO_VAR( VARNAME, EXPR ) __typeof(EXPR) VARNAME = EXPR
#endif

// nullptr

#ifdef HAS_CXX11_NULLPTR
#define CXX11_NULLPTR nullptr
#else
const                        // this is a const object
class {
public:
  template<typename T>             // convertible to any type
    operator T*() const            // of null non-member pointer...
    { return 0; }
  template<typename C, typename T> // or any type of null
    operator T C::*() const        // member pointer...
    { return 0; }
private:
  void operator&() const;    // whose address can't be taken
} nullptr = {};              // and whose name is nullptr
#define CXX11_NULLPTR ::std::nullptr
#endif

// constexpr variables, functions, and constructors

#ifdef HAS_CXX11_CONSTEXPR_VAR
#define CXX11_CONSTEXPR_VAR constexpr
#else
#define CXX11_CONSTEXPR_VAR static const
#endif

#ifdef HAS_CXX11_CONSTEXPR_FLD
#define CXX11_CONSTEXPR_FLD constexpr
#else
#define CXX11_CONSTEXPR_FLD const
#endif

#ifdef HAS_CXX11_CONSTEXPR_FUN
#define CXX11_CONSTEXPR_FUN constexpr
#else
#define CXX11_CONSTEXPR_FUN inline
#endif

#ifdef HAS_CXX11_CONSTEXPR_CTOR
#define CXX11_CONSTEXPR_CTOR constexpr
#else
#define CXX11_CONSTEXPR_CTOR inline
#endif

// explicit conversions

#ifdef HAS_CXX11_EXPLICIT_CONV
#define CXX11_EXPLICIT_CONV explicit
#else
#define CXX11_EXPLICIT_CONV
#endif

// thread local variables

#if defined HAS_CXX11_NON_TRIVIAL_THREAD_LOCAL
#define CXX11_NON_TRIVIAL_THREAD_LOCAL thread_local
#define CXX11_TRIVIAL_THREAD_LOCAL thread_local
#elif defined HAS_CXX11_TRIVIAL_THREAD_LOCAL
#define CXX11_TRIVIAL_THREAD_LOCAL thread_local
#elif defined HAS_CXX11_PRIMITIVE_THREAD_LOCAL
#define HAS_CXX11_TRIVIAL_THREAD_LOCAL
#define CXX11_TRIVIAL_THREAD_LOCAL __thread
#endif

// rvalue reference declarations

#ifdef HAS_CXX11_RVREF
#define CXX11_RVREF( x ) x
#else
#define CXX11_RVREF( x )
#endif

#ifdef HAS_CXX11_MOVE_SPECIAL
#define CXX11_MOVE_SPECIAL( x ) x
#else
#define CXX11_MOVE_SPECIAL( x )
#endif

// strong enums and their qualification

#ifdef HAS_CXX11_STRONG_ENUM
#define CXX11_ENUM_CLASS enum class
#define CXX11_ENUM_QUAL( e ) e::
#else
#define CXX11_ENUM_CLASS enum
#define CXX11_ENUM_QUAL( e )
#endif

} // namespace std

#endif
