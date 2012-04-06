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

#include "cxx0x.h"

// The STATIC_ASSERT macro expands either to the C++0x static_assert
// or to an illegal definition.  The message must be an identifier.
// The macro may be used at namespace or block scope.  At class
// scope, use CLASS_STATIC_ASSERT.

CXX0X_STATIC_ASSERT( sizeof(long) >= sizeof(int), long_too_small )

struct asserting {
    int field;
    CXX0X_CLASS_STATIC_ASSERT( sizeof(long) >= sizeof(int), long_too_small )
};

// The DEFAULTED macros expand to =default, or to definitions that
// emulate the default behavior.
// DEFAULTED_EASY is for bodies where the default behavior is
// specified with {}, typically destruction and default construction.
// DEFAULTED_HARD is for when the default behavior requires code,
// typically copy construction and assignment.

class defaulted {
    int val;
public:
    defaulted() CXX0X_DEFAULTED_EASY
    defaulted( const defaulted &rhs ) CXX0X_DEFAULTED_HARD(
        : val( rhs.val ) { } )
    defaulted& operator=( const defaulted &rhs ) CXX0X_DEFAULTED_HARD(
        { val = rhs.val; return *this; } )
    ~defaulted() CXX0X_DEFAULTED_EASY
};

// The DELETED macros expand to =delete, or to a simple declaration
// that we expect to not be found at link time.

struct deleted {
    int val;
public:
    deleted() CXX0X_DELETED
    deleted( const deleted &rhs ) CXX0X_DELETED
    deleted& operator=( const deleted &rhs ) CXX0X_DELETED
    ~deleted() CXX0X_DELETED
};

// The CONSTEXPR macros identify constexpr types and functions.
// The literal properties do not work before C++0x, but at least
// one can mark intent.
// CONSTEXPR_VAR marks variables.
// CONSTEXPR_FUN marks functions.
// CONSTEXPR_CTOR marks constructors.

CXX0X_CONSTEXPR_VAR int constant = 32;

CXX0X_CONSTEXPR_FUN bool identity( bool b ) { return b; }

// CONSTEXPR_CTOR is related to aggregate initialization and the
// use of uniform initialization.
// The AGGR_INIT macro contains the methods that can be specified with
// aggregate initialization with C++0x, but not with C++03.
// These are constructors, assignment, and destructor.

// Note, sadly, that the member initializer list can contain only one member.
// Otherwise, the macro is parsed incorrectly as needing two arguments.

struct aggrinit {
    int val;
    CXX0X_AGGR_INIT(
    aggrinit() CXX0X_DEFAULTED_EASY
    CXX0X_CONSTEXPR_CTOR aggrinit( int v ) : val(v) { }
    aggrinit( const aggrinit& ) CXX0X_DELETED
    aggrinit& operator =( const aggrinit& ) CXX0X_DELETED
    )
};

aggrinit initialized_var = { 3 };

// C++0x permits standard layout structs to have private members,
// so long as all members are private.  C++03 does not.
// The TRIVIAL_PRIVATE macro adds the private label in C++0x.

struct trivial {
CXX0X_TRIVIAL_PRIVATE
    int a;
    int b;
};

// C++0x repurposes the auto keyword to enable infering the type of
// a variable from its initializer.  The AUTO_VAR macro makes use of
// either that feature or the typeof facility of gcc.

int autovar( int arg ) {
    CXX0X_AUTO_VAR( res, arg );
    return res;
}

// The NULLPTR macro emulates the new nullptr keyword.
// Use it in place of NULL.
// Type inference may not be 100% compatible,
// so prefer to use it only in relatively obvious cases.

int* take_overload( int );
int* take_overload( int* );

int* nullpointer() {
    return take_overload( CXX0X_NULLPTR );
}

// EXPLICIT_CONV adds the explicit keyword for conversions under C++0x.
// Under C++98, this macro is empty, and so do not rely on it.

class very_explicit {
    int val;
public:
    explicit very_explicit( int arg ) : val( arg ) { }
    CXX0X_EXPLICIT_CONV operator int() { return val; }
};

// C++0x adds move semantics in the form of rvalue references.
// RVREF adds the move signatures under C++0x.

class movable {
    int* val;
public:
    CXX0X_RVREF( movable( movable&& old )
        { val = old.val; old.val = CXX0X_NULLPTR; } )
};

// THREAD_LOCAL expands to thread_local (C++0x)
// or __thread (common C++98 extension).
// Use it for namespace-scope variables, static class variables,
// or static function-local variables.
// Do not use it with any variable that has a non-trivial constructor
// or a non-trivial destructor; they are not yet supported.

CXX0X_THREAD_LOCAL int thread_local_var;


int main()
{
    return 0;
}
