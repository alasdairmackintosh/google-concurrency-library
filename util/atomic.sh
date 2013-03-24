# Copyright 2009 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

####################################################### begin idempotent header

cat <<EOF

#ifndef CXX11_ATOMIC_H
#define CXX11_ATOMIC_H

EOF

#################################################### begin language unification

cat <<EOF

/*
This header implements as much as is currently feasible of
the C++11 atomics,
which is chapter 29 of
http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2010/n3092.pdf,
and the C1x atomics.
*/

#ifdef __cplusplus
#include <cstddef>
#include "cxx11.h"
namespace std {
#else
#include <stddef.h>
#include <stdbool.h>
#endif

EOF

############################################################# memory_order type

cat <<EOF

typedef enum memory_order {
memory_order_relaxed, memory_order_consume, memory_order_acquire,
memory_order_release, memory_order_acq_rel, memory_order_seq_cst
} memory_order;

EOF

############################################################### fence functions


cat <<EOF

#ifdef __cplusplus
extern "C" {
#endif

extern void atomic_thread_fence( memory_order );
extern void atomic_signal_fence( memory_order );

#ifdef __cplusplus
}
#endif

EOF

############################################################### kill_dependency

cat <<EOF

#ifdef __cplusplus

template <class T> T kill_dependency(T y) { return y; }

#endif

EOF

############################################################## atomic_flag type

cat <<EOF

typedef struct atomic_flag atomic_flag;

EOF

########################################################### atomic_flag externs

cat <<EOF

#ifdef __cplusplus
extern "C" {
#endif

extern bool __atomic_flag_test_and_set_explicit
( volatile atomic_flag*, memory_order );
extern void __atomic_flag_clear_explicit
( volatile atomic_flag*, memory_order );
extern void __atomic_flag_wait_explicit__
( volatile atomic_flag*, memory_order );
extern volatile atomic_flag* __atomic_flag_for_address__
( const volatile void* __z__ )
__attribute__((const));

#ifdef __cplusplus
}
#endif

EOF

############################################## atomic_flag overloaded functions

cat <<EOF

#ifdef __cplusplus

EOF

for VOLATILE in "" volatile
do

cat <<EOF

inline bool atomic_flag_test_and_set
( ${VOLATILE} atomic_flag* __a__ )
{ return __atomic_flag_test_and_set_explicit( __a__, memory_order_seq_cst ); }

inline bool atomic_flag_test_and_set_explicit
( ${VOLATILE} atomic_flag* __a__, memory_order __x__ )
{ return __atomic_flag_test_and_set_explicit( __a__, __x__ ); }

inline void atomic_flag_clear
( ${VOLATILE} atomic_flag* __a__ )
{ return __atomic_flag_clear_explicit( __a__, memory_order_seq_cst ); }

inline void atomic_flag_clear_explicit
( ${VOLATILE} atomic_flag* __a__, memory_order __x__ )
{ return __atomic_flag_clear_explicit( __a__, __x__ ); }

EOF

done

cat <<EOF

#endif

EOF

############################################### atomic_flag type generic macros

cat <<EOF

#ifndef __cplusplus

#define atomic_flag_test_and_set( __a__ ) \
__atomic_flag_test_and_set_explicit( __a__, memory_order_seq_cst )

#define atomic_flag_test_and_set_explicit( __a__, __x__ ) \
__atomic_flag_test_and_set_explicit( __a__, __x__ )

#define atomic_flag_clear( __a__ ) \
__atomic_flag_clear_explicit( __a__, memory_order_seq_cst )

#define atomic_flag_clear_explicit( __a__, __x__ ) \
__atomic_flag_clear_explicit( __a__, __x__ )

#endif

EOF

############################################################ atomic_flag struct

cat <<EOF

struct atomic_flag
{
    #ifdef __cplusplus
    CXX11_AGGR_INIT(
    atomic_flag() CXX11_DEFAULTED_EASY
    CXX11_CONSTEXPR_CTOR atomic_flag( bool __v__ ) : __f__( __v__ ) { }
    atomic_flag( const atomic_flag& ) CXX11_DELETED
    atomic_flag& operator =( const atomic_flag& ) CXX11_DELETED
    )

EOF

for VOLATILE in "" volatile
do

cat <<EOF

    bool test_and_set( memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return atomic_flag_test_and_set_explicit( this, __x__ ); }

    void clear( memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { atomic_flag_clear_explicit( this, __x__ ); }

EOF

done

cat <<EOF

    CXX11_TRIVIAL_PRIVATE
    #endif
    bool __f__;
};

#define ATOMIC_FLAG_INIT { false }

EOF

############################################################## lock free macros

cat <<EOF

#define ATOMIC_CHAR_LOCK_FREE 0
#define ATOMIC_CHAR16_T_LOCK_FREE 0
#define ATOMIC_CHAR32_T_LOCK_FREE 0
#define ATOMIC_WCHAR_T_LOCK_FREE 0
#define ATOMIC_SHORT_LOCK_FREE 0
#define ATOMIC_INT_LOCK_FREE 0
#define ATOMIC_LONG_LOCK_FREE 0
#define ATOMIC_LLONG_LOCK_FREE 0
#define ATOMIC_ADDRESS_LOCK_FREE 0

EOF

######################################################### initialization macros

cat <<EOF

#define ATOMIC_VAR_INIT( __m__ ) { __m__ }

EOF

######################################################### implementation macros

cat <<EOF

#define _ATOMIC_LOAD_( __a__, __x__ ) \
({ volatile __typeof__((__a__)->__f__)* __p__ = &((__a__)->__f__); \
   volatile atomic_flag* __g__ = __atomic_flag_for_address__( __p__ ); \
   __atomic_flag_wait_explicit__( __g__, __x__ ); \
   __typeof__((__a__)->__f__) __r__ = *__p__; \
   atomic_flag_clear_explicit( __g__, __x__ ); \
   __r__; })

#define _ATOMIC_STORE_( __a__, __m__, __x__ ) \
({ volatile __typeof__((__a__)->__f__)* __p__ = &((__a__)->__f__); \
   __typeof__(__m__) __v__ = (__m__); \
   volatile atomic_flag* __g__ = __atomic_flag_for_address__( __p__ ); \
   __atomic_flag_wait_explicit__( __g__, __x__ ); \
   *__p__ = __v__; \
   atomic_flag_clear_explicit( __g__, __x__ ); \
   __v__; })

#define _ATOMIC_MODIFY_( __a__, __o__, __m__, __x__ ) \
({ volatile __typeof__((__a__)->__f__)* __p__ = &((__a__)->__f__); \
   __typeof__(__m__) __v__ = (__m__); \
   volatile atomic_flag* __g__ = __atomic_flag_for_address__( __p__ ); \
   __atomic_flag_wait_explicit__( __g__, __x__ ); \
   __typeof__((__a__)->__f__) __r__ = *__p__; \
   *__p__ __o__ __v__; \
   atomic_flag_clear_explicit( __g__, __x__ ); \
   __r__; })

#define _ATOMIC_PTRMOD_( __a__, __o__, __m__, __x__ ) \
({ volatile __typeof__((__a__)->__f__)* __p__ = &((__a__)->__f__); \
   __typeof__(__m__) __v__ = (__m__); \
   volatile atomic_flag* __g__ = __atomic_flag_for_address__( __p__ ); \
   __atomic_flag_wait_explicit__( __g__, __x__ ); \
   __typeof__((__a__)->__f__) __r__ = *__p__; \
   *(char*)__p__ __o__ __v__; \
   atomic_flag_clear_explicit( __g__, __x__ ); \
   __r__; })

#define _ATOMIC_CMPSWP_( __a__, __e__, __m__, __x__ ) \
({ volatile __typeof__((__a__)->__f__)* __p__ = &((__a__)->__f__); \
   __typeof__(__e__) __q__ = (__e__); \
   __typeof__(__m__) __v__ = (__m__); \
   bool __r__; \
   volatile atomic_flag* __g__ = __atomic_flag_for_address__( __p__ ); \
   __atomic_flag_wait_explicit__( __g__, __x__ ); \
   __typeof__((__a__)->__f__) __t__ = *__p__; \
   if ( __t__ == *__q__ ) { *__p__ = __v__; __r__ = true; } \
   else { *__q__ = (__typeof__(*__q__))__t__; __r__ = false; } \
   atomic_flag_clear_explicit( __g__, __x__ ); \
   __r__; })

EOF

################################################################ type variables

_bool="bool"
_address="void*"

CHARS="_char _schar _uchar"
_char="char"
_schar="signed char"
_uchar="unsigned char"

INTS="_short _ushort _int _uint _long _ulong _llong _ullong"
_short="short"
_ushort="unsigned short"
_int="int"
_uint="unsigned int"
_long="long"
_ulong="unsigned long"
_llong="long long"
_ullong="unsigned long long"

INTEGERS="${CHARS} ${INTS}"

CHARACTERS="_wchar_t"
#FIX CHARACTERS="_wchar_t _char16_t _char32_t"
_char16_t="char16_t"
_char32_t="char32_t"
_wchar_t="wchar_t"

########################################################### operation variables

ADR_OPERATIONS="_add _sub"
INT_OPERATIONS="_add _sub _and _or _xor"
_add="+"
_sub="-"
_and="&"
_or="|"
_xor="^"

########################################################### gen_normal commands

gen_normal_struct_head()
{
KEY=$1
NAME=$2

cat <<EOF

typedef struct atomic${KEY}
{

EOF

}

gen_normal_struct_tail()
{
KEY=$1
NAME=$2

cat <<EOF

    CXX11_TRIVIAL_PRIVATE
    #endif
    ${NAME} __f__;
} atomic${KEY};

EOF

}

gen_normal_constructors()
{
KEY=$1
NAME=$2

cat <<EOF

    #ifdef __cplusplus
    CXX11_AGGR_INIT(
    atomic${KEY}() CXX11_DEFAULTED_EASY
    CXX11_CONSTEXPR_CTOR atomic${KEY}( ${NAME} __v__ )
    : __f__( __v__) { }
    atomic${KEY}( const atomic${KEY}& ) CXX11_DELETED
    )

EOF

}

gen_primary_methods()
{
KEY=$1
NAME=$2

for VOLATILE in "" volatile
do

cat <<EOF

    bool is_lock_free() const ${VOLATILE}
    { return false; }

    void store
    ( ${NAME} __m__, memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { _ATOMIC_STORE_( this, __m__, __x__ ); }

    ${NAME} load
    ( memory_order __x__ = memory_order_seq_cst ) const ${VOLATILE}
    { return (${NAME})_ATOMIC_LOAD_( this, __x__ ); }

    ${NAME} exchange
    ( ${NAME} __m__, memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return (${NAME})_ATOMIC_MODIFY_( this, =, __m__, __x__ ); }

    bool compare_exchange_weak
    ( ${NAME}& __e__, ${NAME} __m__,
      memory_order __x__, memory_order __y__ ) ${VOLATILE}
    { return _ATOMIC_CMPSWP_( this, &__e__, __m__, __x__ ); }

    bool compare_exchange_weak
    ( ${NAME}& __e__, ${NAME} __m__,
      memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return _ATOMIC_CMPSWP_( this, &__e__, __m__, __x__ ); }

    bool compare_exchange_strong
    ( ${NAME}& __e__, ${NAME} __m__,
      memory_order __x__, memory_order __y__ ) ${VOLATILE}
    { return _ATOMIC_CMPSWP_( this, &__e__, __m__, __x__ ); }

    bool compare_exchange_strong
    ( ${NAME}& __e__, ${NAME} __m__,
      memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return _ATOMIC_CMPSWP_( this, &__e__, __m__, __x__ ); }

EOF

done

}

gen_primary_operators()
{
KEY=$1
NAME=$2

for VOLATILE in "" volatile
do

cat <<EOF

    CXX11_AGGR_INIT(
    atomic${KEY}& operator =
    ( const atomic${KEY}& ) ${VOLATILE} CXX11_DELETED
    )

    ${NAME} operator =( ${NAME} __v__ ) ${VOLATILE}
    { store( __v__ ); return __v__; }

    operator ${NAME}() const ${VOLATILE}
    { return load(); }

EOF

done

}

gen_primary_functions()
{
KEY=$1
NAME=$2

cat <<EOF

#ifdef __cplusplus

EOF

for VOLATILE in "" volatile
do

cat <<EOF

inline bool atomic_is_lock_free( const ${VOLATILE} atomic${KEY}* __a__ )
{ return false; }

inline void atomic_init
( ${VOLATILE} atomic${KEY}* __a__, ${NAME} __m__ )
{ __a__->store( __m__, memory_order_relaxed ); }

inline ${NAME} atomic_load_explicit
( ${VOLATILE} atomic${KEY}* __a__, memory_order __x__ )
{ return __a__->load( __x__ );; }

inline ${NAME} atomic_load( ${VOLATILE} atomic${KEY}* __a__ )
{ return __a__->load( memory_order_seq_cst ); }

inline void atomic_store_explicit
( ${VOLATILE} atomic${KEY}* __a__, ${NAME} __m__, memory_order __x__ )
{ __a__->store( __m__, __x__ ); }

inline void atomic_store
( ${VOLATILE} atomic${KEY}* __a__, ${NAME} __m__ )
{ __a__->store( __m__, memory_order_seq_cst ); }

inline ${NAME} atomic_exchange_explicit
( ${VOLATILE} atomic${KEY}* __a__, ${NAME} __m__, memory_order __x__ )
{ return __a__->exchange( __m__, __x__ ); }

inline ${NAME} atomic_exchange
( ${VOLATILE} atomic${KEY}* __a__, ${NAME} __m__ )
{ return __a__->exchange( __m__, memory_order_seq_cst ); }

inline bool atomic_compare_exchange_weak_explicit
( ${VOLATILE} atomic${KEY}* __a__, ${NAME}* __e__, ${NAME} __m__,
    memory_order __x__, memory_order __y__ )
{ return __a__->compare_exchange_weak( *__e__, __m__, __x__, __y__ ); }

inline bool atomic_compare_exchange_weak
( ${VOLATILE} atomic${KEY}* __a__, ${NAME}* __e__, ${NAME} __m__ )
{ return __a__->compare_exchange_weak( *__e__, __m__,
    memory_order_seq_cst, memory_order_seq_cst ); }

inline bool atomic_compare_exchange_strong_explicit
( ${VOLATILE} atomic${KEY}* __a__, ${NAME}* __e__, ${NAME} __m__,
    memory_order __x__, memory_order __y__ )
{ return __a__->compare_exchange_strong( *__e__, __m__, __x__, __y__ ); }

inline bool atomic_compare_exchange_strong
( ${VOLATILE} atomic${KEY}* __a__, ${NAME}* __e__, ${NAME} __m__ )
{ return __a__->compare_exchange_strong( *__e__, __m__,
    memory_order_seq_cst, memory_order_seq_cst ); }

EOF

done

cat <<EOF

#endif

EOF

}

gen_secondary_methods()
{
KEY=$1
RTN=$2
ARG=$3
shift
shift
shift

for OPERKEY in $*
do
OPERSYM=${!OPERKEY}

for VOLATILE in "" volatile
do

if test "$ARG" = ptrdiff_t
then

cat <<EOF

    ${RTN} fetch${OPERKEY}
    ( ${ARG} __m__, memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return _ATOMIC_PTRMOD_( this, ${OPERSYM}=, __m__, __x__ ); }

EOF

else

cat <<EOF

    ${RTN} fetch${OPERKEY}
    ( ${ARG} __m__, memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return _ATOMIC_MODIFY_( this, ${OPERSYM}=, __m__, __x__ ); }

EOF

fi

done

done

}

gen_secondary_operators()
{
KEY=$1
RTN=$2
ARG=$3
shift
shift
shift

for OPERKEY in $*
do
OPERSYM=${!OPERKEY}

for VOLATILE in "" volatile
do

if test "$KEY" = _address
then

cat <<EOF

    ${RTN} operator ${OPERSYM}=( ${ARG} __v__ ) ${VOLATILE}
    { return ((char*)fetch${OPERKEY}( __v__ )) ${OPERSYM} __v__; }

EOF

else

cat <<EOF

    ${RTN} operator ${OPERSYM}=( ${ARG} __v__ ) ${VOLATILE}
    { return fetch${OPERKEY}( __v__ ) ${OPERSYM} __v__; }

EOF

fi

done

done

}

gen_incr_decr_operators()
{
KEY="$1"
NAME="$2"

for VOLATILE in "" volatile
do

cat <<EOF

    ${NAME} operator ++( int ) ${VOLATILE}
    { return fetch_add( 1 ); }
    ${NAME} operator --( int ) ${VOLATILE}
    { return fetch_sub( 1 ); }
    ${NAME} operator ++() ${VOLATILE}
    { return fetch_add( 1 ) + 1; }
    ${NAME} operator --() ${VOLATILE}
    { return fetch_sub( 1 ) - 1; }

EOF

done

}

gen_secondary_functions()
{
KEY=$1
RTN=$2
ARG=$3
shift
shift
shift

cat <<EOF

#ifdef __cplusplus

EOF

for OPERKEY in $*
do
OPERSYM=${!OPERKEY}

for VOLATILE in "" volatile
do

cat <<EOF

inline ${RTN} atomic_fetch${OPERKEY}_explicit
( ${VOLATILE} atomic${KEY}* __a__, ${ARG} __m__, memory_order __x__ )
{ return __a__->fetch${OPERKEY}( __m__, __x__ ); }

inline ${RTN} atomic_fetch${OPERKEY}
( ${VOLATILE} atomic${KEY}* __a__, ${ARG} __m__ )
{ return __a__->fetch${OPERKEY}( __m__, memory_order_seq_cst ); }

EOF

done

done

cat <<EOF

#endif

EOF

}

############################################################## atomic_bool type

gen_normal_struct_head _bool bool
gen_normal_constructors _bool bool
gen_primary_methods _bool bool
gen_primary_operators _bool bool
gen_normal_struct_tail _bool bool
gen_primary_functions _bool bool

########################################################### atomic_address type

gen_normal_struct_head _address "void*"
gen_normal_constructors _address "void*"
gen_primary_methods _address "void*"
gen_primary_operators _address "void*"
gen_secondary_methods _address "void*" "ptrdiff_t" ${ADR_OPERATIONS}
gen_secondary_operators _address "void*" "ptrdiff_t" ${ADR_OPERATIONS}
gen_normal_struct_tail _address "void*"
gen_primary_functions _address "void*"
gen_secondary_functions _address "void*" "ptrdiff_t" ${ADR_OPERATIONS}

######################################################### atomic integral types

for TYPEKEY in ${INTEGERS}
do
TYPENAME=${!TYPEKEY}

gen_normal_struct_head "${TYPEKEY}" "${TYPENAME}"
gen_normal_constructors "${TYPEKEY}" "${TYPENAME}"
gen_primary_methods "${TYPEKEY}" "${TYPENAME}"
gen_primary_operators "${TYPEKEY}" "${TYPENAME}"
gen_secondary_methods "${TYPEKEY}" "${TYPENAME}" "${TYPENAME}" ${INT_OPERATIONS}
gen_secondary_operators "${TYPEKEY}" "${TYPENAME}" "${TYPENAME}" ${INT_OPERATIONS}
gen_incr_decr_operators "${TYPEKEY}" "${TYPENAME}"
gen_normal_struct_tail "${TYPEKEY}" "${TYPENAME}"
gen_primary_functions "${TYPEKEY}" "${TYPENAME}"
gen_secondary_functions "${TYPEKEY}" "${TYPENAME}" "${TYPENAME}" ${INT_OPERATIONS}

done

cat <<EOF

#ifdef __cplusplus

EOF

for TYPEKEY in ${CHARACTERS}
do
TYPENAME=${!TYPEKEY}

gen_normal_struct_head "${TYPEKEY}" "${TYPENAME}"
gen_normal_constructors "${TYPEKEY}" "${TYPENAME}"
gen_primary_methods "${TYPEKEY}" "${TYPENAME}"
gen_primary_operators "${TYPEKEY}" "${TYPENAME}"
gen_secondary_methods "${TYPEKEY}" "${TYPENAME}" "${TYPENAME}" ${INT_OPERATIONS}
gen_secondary_operators "${TYPEKEY}" "${TYPENAME}" "${TYPENAME}" ${INT_OPERATIONS}
gen_incr_decr_operators "${TYPEKEY}" "${TYPENAME}"
gen_normal_struct_tail "${TYPEKEY}" "${TYPENAME}"
gen_primary_functions "${TYPEKEY}" "${TYPENAME}"
gen_secondary_functions "${TYPEKEY}" "${TYPENAME}" "${TYPENAME}" ${INT_OPERATIONS}

done

cat <<EOF

#endif

EOF

############################################################# inttypes typedefs

cat <<EOF

typedef atomic_schar atomic_int_least8_t;
typedef atomic_uchar atomic_uint_least8_t;
typedef atomic_short atomic_int_least16_t;
typedef atomic_ushort atomic_uint_least16_t;
typedef atomic_int atomic_int_least32_t;
typedef atomic_uint atomic_uint_least32_t;
typedef atomic_llong atomic_int_least64_t;
typedef atomic_ullong atomic_uint_least64_t;

typedef atomic_schar atomic_int_fast8_t;
typedef atomic_uchar atomic_uint_fast8_t;
typedef atomic_short atomic_int_fast16_t;
typedef atomic_ushort atomic_uint_fast16_t;
typedef atomic_int atomic_int_fast32_t;
typedef atomic_uint atomic_uint_fast32_t;
typedef atomic_llong atomic_int_fast64_t;
typedef atomic_ullong atomic_uint_fast64_t;

typedef atomic_long atomic_intptr_t;
typedef atomic_ulong atomic_uintptr_t;

typedef atomic_long atomic_ssize_t;
typedef atomic_ulong atomic_size_t;

typedef atomic_long atomic_ptrdiff_t;

typedef atomic_llong atomic_intmax_t;
typedef atomic_ullong atomic_uintmax_t;

#ifndef __cplusplus

typedef atomic_int_least16_t atomic_char16_t;
typedef atomic_int_least32_t atomic_char32_t;
typedef atomic_int_least32_t atomic_wchar_t;

#endif

EOF

######################################################## begin C++ only section

cat <<EOF

#ifdef __cplusplus

EOF

######################################################### gen_template commands

gen_derived_struct_tail()
{
KEY=$1
NAME=$2

cat <<EOF

};

EOF

}

gen_template_constructors()
{
KEY=$1
NAME=$2
INIT=$3

cat <<EOF

    atomic() CXX11_DEFAULTED_EASY
    CXX11_CONSTEXPR_CTOR atomic( ${NAME} __v__ )
    CXX11_AGGR_INIT( : ${INIT}( __v__ ) )
    { CXX11_NO_AGGR_INIT( __f__ = __v__; ) }
    atomic( const atomic& ) CXX11_DELETED

EOF

}

########################################################## atomic template type

cat <<EOF

template< typename T >
struct atomic
{

EOF

gen_template_constructors "" "T*" "__f__"
gen_primary_methods "" "T"
gen_primary_operators "" "T"

cat <<EOF

    CXX11_TRIVIAL_PRIVATE
    T __f__;
};

EOF

#################################### atomic pointer type partial specialization

cat <<EOF

template< typename T >
struct atomic< T* >
: atomic_address
{

EOF

gen_template_constructors "" "T*" "atomic_address"
gen_primary_methods "_address" "T*"
gen_primary_operators "_address" "T*"

for VOLATILE in "" volatile
do

cat <<EOF

    T* fetch_add
    ( ptrdiff_t __m__, memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return _ATOMIC_PTRMOD_( this, +=, __m__ * sizeof (T), __x__ ); }

    T* fetch_sub
    ( ptrdiff_t __m__, memory_order __x__ = memory_order_seq_cst ) ${VOLATILE}
    { return _ATOMIC_PTRMOD_( this, -=, __m__ * sizeof (T), __x__ ); }

EOF

done

gen_secondary_operators "" "T*" "ptrdiff_t" ${ADR_OPERATIONS}
gen_incr_decr_operators "_address" "T*"
gen_derived_struct_tail "_address" "T*"

########################################## atomic integral type specializations

for TYPEKEY in _bool _address ${INTEGERS} ${CHARACTERS}
do
TYPENAME=${!TYPEKEY}

cat <<EOF

template<>
struct atomic< ${TYPENAME} >
: atomic${TYPEKEY}
{

EOF

gen_template_constructors "${TYPEKEY}" "${TYPENAME}" "atomic${TYPEKEY}"
gen_primary_operators "${TYPEKEY}" "${TYPENAME}"
gen_derived_struct_tail "${TYPEKEY}" "${TYPENAME}"

done

########################################################## end C++ only section

cat <<EOF

#endif

EOF

###################################################### C type generic functions

cat <<EOF

#ifndef __cplusplus

#define atomic_is_lock_free( __a__ ) \
false

#define atomic_load( __a__ ) \
_ATOMIC_LOAD_( __a__, memory_order_seq_cst )

#define atomic_load_explicit( __a__, __x__ ) \
_ATOMIC_LOAD_( __a__, __x__ )

#define atomic_store( __a__, __m__ ) \
_ATOMIC_STORE_( __a__, __m__, memory_order_seq_cst )

#define atomic_store_explicit( __a__, __m__, __x__ ) \
_ATOMIC_STORE_( __a__, __m__, __x__ )

#define atomic_exchange( __a__, __m__ ) \
_ATOMIC_MODIFY_( __a__, =, __m__, memory_order_seq_cst )

#define atomic_exchange_explicit( __a__, __m__, __x__ ) \
_ATOMIC_MODIFY_( __a__, =, __m__, __x__ )

#define atomic_compare_exchange_strong( __a__, __e__, __m__ ) \
_ATOMIC_CMPSWP_( __a__, __e__, __m__, memory_order_seq_cst )

#define atomic_compare_exchange_strong_explicit( \
__a__, __e__, __m__, __x__, __y__ ) \
_ATOMIC_CMPSWP_( __a__, __e__, __m__, __x__ )

#define atomic_compare_exchange_weak( __a__, __e__, __m__ ) \
_ATOMIC_CMPSWP_( __a__, __e__, __m__, memory_order_seq_cst )

#define atomic_compare_exchange_weak_explicit( \
__a__, __e__, __m__, __x__, __y__ ) \
_ATOMIC_CMPSWP_( __a__, __e__, __m__, __x__ )

EOF

for OPERKEY in ${INT_OPERATIONS}
do
OPERSYM=${!OPERKEY}

cat <<EOF

#define atomic_fetch${OPERKEY}( __a__, __m__ ) \
_ATOMIC_MODIFY_( __a__, ${OPERSYM}=, __m__, memory_order_seq_cst )

#define atomic_fetch${OPERKEY}_explicit( __a__, __m__, __x__ ) \
_ATOMIC_MODIFY_( __a__, ${OPERSYM}=, __m__, __x__ )

EOF

done

cat <<EOF

#endif

EOF

###################################################### end language unification

cat <<EOF

#ifdef __cplusplus
} // namespace std
#endif

EOF

######################################################### end idempotent header

cat <<EOF

#endif // CXX11_ATOMIC_H

EOF
