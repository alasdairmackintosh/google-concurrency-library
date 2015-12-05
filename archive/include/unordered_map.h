#ifndef CXX11_UNORDERED_MAP_H
#define CXX11_UNORDERED_MAP_H

#if defined(__GNUC__) && defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ == 4

#include <unordered_map>

#else

#include <tr1/unordered_map>
namespace std {
using tr1::unordered_map;
}

#endif

#endif
