#ifndef FUNCTIONAL_H
#define FUNCTIONAL_H

#if defined(__GXX_EXPERIMENTAL_CXX0X__)

#include <functional>

#else

#include <tr1/functional>
namespace std {
using tr1::function;
using tr1::bind;
using tr1::ref;
namespace placeholders = tr1::placeholders;
using tr1::hash;
}

#endif

#endif
