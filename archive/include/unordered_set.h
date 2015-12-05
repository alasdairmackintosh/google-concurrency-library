#ifndef CXX11_UNORDERED_SET_H
#define CXX11_UNORDERED_SET_H

#if defined(__GNUC__) && defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ == 4

#include <unordered_set>

#else

#include <tr1/unordered_set>
namespace std {
using tr1::unordered_set;
}

#endif

#endif
