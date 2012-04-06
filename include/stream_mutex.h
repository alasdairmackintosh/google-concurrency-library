// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef STREAM_MUTEX_H
#define STREAM_MUTEX_H

#include "cxx0x.h"

#include "mutex.h"

template <class Stream >
class stream_mutex;

template <class Stream>
class stream_guard
{
  public:
    struct already_locked { };
    stream_guard(stream_mutex<Stream>& mtx);
    stream_guard(stream_mutex<Stream>& mtx, already_locked);
    ~stream_guard();
    Stream& bypass() const;
  private:
    mutable stream_mutex<Stream>& mtx_;
};

template <class Stream >
class stream_mutex
{
  public:
    CXX0X_CONSTEXPR_CTOR stream_mutex(Stream& stm) : stm_(stm) { }
    void lock() { mtx_.lock(); }
    void unlock() { mtx_.unlock(); }
    bool try_lock() { return mtx_.try_lock(); }
    stream_guard<Stream> hold() { return stream_guard<Stream>(*this); }
    Stream& bypass() { return stm_; }
  private:
    Stream& stm_;
    recursive_mutex mtx_;
};

template <class Stream >
inline stream_guard<Stream>::stream_guard(stream_mutex<Stream>& mtx)
: mtx_(mtx) { mtx.lock(); }

template <class Stream >
inline stream_guard<Stream>::stream_guard(stream_mutex<Stream>& mtx,
                                          already_locked)
: mtx_(mtx) { }

template <class Stream >
inline stream_guard<Stream>::~stream_guard()
{ mtx_.unlock(); }

template <class Stream >
inline Stream& stream_guard<Stream>::bypass() const
{ return mtx_.bypass(); }

template <typename Stream, typename T>
const stream_guard<Stream>& operator<<(const stream_guard<Stream>& lck, T arg)
{
    lck.bypass() << arg;
    return lck;
}

template <typename Stream>
const stream_guard<Stream>& operator<<(const stream_guard<Stream>& lck,
                                      Stream& (*arg)(Stream&))
{
    lck.bypass() << arg;
    return lck;
}

template <typename Stream, typename T>
const stream_guard<Stream>& operator>>(const stream_guard<Stream>& lck, T& arg)
{
    lck.bypass() >> arg;
    return lck;
}

template <typename Stream, typename T>
stream_guard<Stream> operator<<(stream_mutex<Stream>& mtx, T arg)
{
    mtx.lock();
    mtx.bypass() << arg;
    return stream_guard<Stream>(mtx,
        typename stream_guard<Stream>::already_locked());
}

template <typename Stream>
stream_guard<Stream> operator<<(stream_mutex<Stream>& mtx,
                                      Stream& (*arg)(Stream&))
{
    mtx.lock();
    mtx.bypass() << arg;
    return stream_guard<Stream>(mtx,
        typename stream_guard<Stream>::already_locked());
}

template <typename Stream, typename T>
stream_guard<Stream> operator>>(stream_mutex<Stream>& mtx, T& arg)
{
    mtx.lock();
    mtx.bypass() >> arg;
    return stream_guard<Stream>(mtx,
        typename stream_guard<Stream>::already_locked());
}

#endif
