#include <iostream>

#include "counter.h"

std::serial_counter<int> sc( 1 );
std::atomic_counter<int> ac( 1 );
std::serial_counter_buffer<int> scb( ac );
std::atomic_counter_buffer<int> acb( ac );
std::weak_counter<int> wc( 1 );
std::weak_counter_buffer<int> wcb( wc );
std::duplex_counter<int> dc( 1 );
std::duplex_counter_buffer<int> dcb1( dc );
std::atomic_counter_buffer<int> dcb1b( dcb1 );
std::atomic_counter_buffer<int> dcb2( dc );
std::atomic_counter_buffer<int> dcb2b(
		static_cast<std::counter_bumper<int>&>(dcb2) );

int main()
{
    sc.inc( 1 );
    assert( sc.load() == 2 );
    assert( sc.exchange( 0 ) == 2 );
    assert( sc.load() == 0 );
    ac.inc( 1 );
    scb.inc( 1 );
    scb.inc( 1 );
    scb.dec( 1 );
    scb.push();
    acb.inc( 1 );
    acb.push();
    assert( ac.load() == 4 );
    assert( ac.exchange( 0 ) == 4 );
    assert( ac.load() == 0 );
    wc.inc( 1 );
    wcb.inc( 1 );
    assert( wc.load() == 3 );
    dc.inc( 1 );
    dcb1.inc( 1 );
    assert( dc.load() == 3 );
    assert( dc.exchange( 0 ) == 3 );
    assert( dc.load() == 0 );
    dcb1b.inc( 1 );
    dcb2b.inc( 1 );
    assert( dc.load() == 0 );
    dcb1b.push();
    dcb2b.push();
    dcb2.push();
    assert( dc.load() == 2 );
    return 0;
}
