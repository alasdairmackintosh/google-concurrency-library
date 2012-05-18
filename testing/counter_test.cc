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

#include <iostream>

#include "counter.h"

gcl::serial_counter<int> sc( 1 );
gcl::atomic_counter<int> ac( 1 );
gcl::serial_counter_buffer<int> scb( ac );
gcl::atomic_counter_buffer<int> acb( ac );
gcl::weak_counter<int> wc( 1 );
gcl::weak_counter_buffer<int> wcb( wc );
gcl::duplex_counter<int> dc( 1 );
gcl::duplex_counter_buffer<int> dcb1( dc );
gcl::atomic_counter_buffer<int> dcb1b( dcb1 );
gcl::atomic_counter_buffer<int> dcb2( dc );
gcl::atomic_counter_buffer<int> dcb2b(
		static_cast<gcl::counter_bumper<int>&>(dcb2) );

int main()
{
    ++sc;
    assert( sc.load() == 2 );
    assert( sc.exchange( 0 ) == 2 );
    assert( sc.load() == 0 );
    ++ac;
    ++scb;
    ++scb;
    --scb;
    scb.push();
    ++acb;
    acb.push();
    assert( ac.load() == 4 );
    assert( ac.exchange( 0 ) == 4 );
    assert( ac.load() == 0 );
    ++wc;
    ++wcb;
    assert( wc.load() == 3 );
    ++dc;
    ++dcb1;
    assert( dc.load() == 3 );
    assert( dc.exchange( 0 ) == 3 );
    assert( dc.load() == 0 );
    ++dcb1b;
    ++dcb2b;
    assert( dc.load() == 0 );
    dcb1b.push();
    dcb2b.push();
    dcb2.push();
    assert( dc.load() == 2 );
    return 0;
}
