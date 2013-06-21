#include <iostream>
#include <algorithm>
#include <memory>
#include "dynarray.h"

std::dynarray<int> a_global(32);

void dump( const std::dynarray< int > & source )
{
    std::dynarray< int >::const_iterator src = source.begin();
    for ( ; src != source.end(); src++ )
        std::cout << " " << *src;
    std::cout << std::endl;
}
void lowrap(       std::dynarray< int > & target,
             const std::dynarray< int > & source )
{
    dump( source );

    std::dynarray< int > sorted( source );
    dump( sorted );

    std::sort( sorted.begin(), sorted.end() );
    dump( sorted );

    const int* srt = &sorted.front();
    std::dynarray< int >::iterator tgt( target.begin() );
    for ( ; tgt != target.end(); tgt++ ) {
        *tgt = *srt;
        if ( srt == &sorted.back() )
            srt = &sorted.front();
        else
            srt++;
    }
    dump( target );
}
int main() {
    std::dynarray< int > alpha(8);
    std::dynarray< int > gamma(3);
    for ( std::dynarray< int >::size_type i = 0; i < gamma.size(); i++ )
	gamma[i] = 4 - i;
    lowrap( alpha, gamma );
    int sum = 0;
    for ( std::dynarray< int >::size_type i = 0; i < alpha.size(); i++ )
	sum += alpha.at(i);

    if ( sum != 23 )
      return 1;
    return 0;
}
