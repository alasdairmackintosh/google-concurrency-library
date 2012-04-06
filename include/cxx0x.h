#ifndef CXX0X_H
#define CXX0X_H

// See ../testing/cxx0x_test.cc for examples of use.

#if defined(__GNUC__) && defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ == 4
#if __GNUC_MINOR__ >= 3
#define HAS_CXX0X_STATIC_ASSERT
#define HAS_CXX0X_RVREF
#endif
#if __GNUC_MINOR__ >= 4
#define HAS_CXX0X_DEFAULTED
#define HAS_CXX0X_DELETED
#define HAS_CXX0X_AUTO_VAR
#define HAS_CXX0X_EXPLICIT_AGGR_INIT
#endif
#if __GNUC_MINOR__ >= 5
#define HAS_CXX0X_EXPLICIT_CONV
#endif
#endif

namespace std {

// static assert

#ifdef HAS_CXX0X_STATIC_ASSERT
#define CXX0X_STATIC_ASSERT(EXPR, VARNAME) static_assert(EXPR, # VARNAME);
#define CXX0X_CLASS_STATIC_ASSERT(EXPR, VARNAME) static_assert(EXPR, # VARNAME);
#else
template <bool t> class static_asserter;
template<> class static_asserter<true> { };
#define CXX0X_STATIC_ASSERT(EXPR, VARNAME) \
std::static_asserter<EXPR> VARNAME;
#define CXX0X_CLASS_STATIC_ASSERT(EXPR, VARNAME) \
static std::static_asserter<EXPR> VARNAME;
#endif

// defaulted functions

#ifdef HAS_CXX0X_DEFAULTED
#define CXX0X_DEFAULTED_EASY =default;
#define CXX0X_DEFAULTED_HARD( BODY ) =default;
#else
#define CXX0X_DEFAULTED_EASY { }
#define CXX0X_DEFAULTED_HARD( BODY ) BODY
#endif

// aggregate initialization

#ifdef HAS_CXX0X_EXPLICIT_AGGR_INIT
#define CXX0X_AGGR_INIT( DECLS ) DECLS
#define CXX0X_NO_AGGR_INIT( DECLS )
#else
#define CXX0X_AGGR_INIT( DECLS )
#define CXX0X_NO_AGGR_INIT( DECLS ) DECLS
#endif

// deleted functions

#ifdef HAS_CXX0X_DELETED
#define CXX0X_DELETED =delete;
#else
#define CXX0X_DELETED ;
#endif

// private members in trivial classes

#ifdef HAS_CXX0X_TRIVIAL_PRIVATE
#define CXX0X_TRIVIAL_PRIVATE private:
#else
#define CXX0X_TRIVIAL_PRIVATE
#endif

// auto variables

#ifdef HAS_CXX0X_AUTO_VAR
#define CXX0X_AUTO_VAR( VARNAME, EXPR ) auto VARNAME = EXPR
#else
#define CXX0X_AUTO_VAR( VARNAME, EXPR ) __typeof(EXPR) VARNAME = EXPR
#endif

// nullptr

#ifdef HAS_CXX0X_NULLPTR
#define CXX0X_NULLPTR nullptr
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
#define CXX0X_NULLPTR ::std::nullptr
#endif

// constexpr variables, functions, and constructors

#ifdef HAS_CXX0X_CONSTEXPR_VAR
#define CXX0X_CONSTEXPR_VAR constexpr
#else
#define CXX0X_CONSTEXPR_VAR static const
#endif

#ifdef HAS_CXX0X_CONSTEXPR_FUN
#define CXX0X_CONSTEXPR_FUN constexpr
#else
#define CXX0X_CONSTEXPR_FUN const
#endif

#ifdef HAS_CXX0X_CONSTEXPR_CTOR
#define CXX0X_CONSTEXPR_CTOR constexpr
#else
#define CXX0X_CONSTEXPR_CTOR
#endif

// explicit conversions

#ifdef HAS_CXX0X_EXPLICIT_CONV
#define CXX0X_EXPLICIT_CONV explicit
#else
#define CXX0X_EXPLICIT_CONV
#endif

// thread-local storage
// TODO(alasdair): Fix MacOS (__APPLE__) handling of thread_local. Currently
// commented out to enable some development on Mac.

// Note that we cannot rely on constructors and destructors.
#ifdef HAS_CXX0X_TLS
#define CXX0X_THREAD_LOCAL thread_local
#else
#ifdef __APPLE__
#define CXX0X_THREAD_LOCAL
#else
#define CXX0X_THREAD_LOCAL __thread
#endif
#endif
// rvalue reference declarations

#ifdef HAS_CXX0X_RVREF
#define CXX0X_RVREF( x ) x
#else
#define CXX0X_RVREF( x )
#endif

} // namespace std

#endif
