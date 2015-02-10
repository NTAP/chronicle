// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef TVHELPER_H
#define TVHELPER_H

bool
less(const timeval &a, const timeval &b)
{
    if (a.tv_sec < b.tv_sec) {
        return true;
    } else if (a.tv_sec == b.tv_sec) {
        if (a.tv_usec < b.tv_usec) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool
operator<(const timeval &a, const timeval &b)
{
    return less(a, b);
}

bool
operator==(const timeval &a, const timeval &b)
{
    return (a.tv_sec == b.tv_sec &&
            a.tv_usec == b.tv_usec);
}

bool
operator!=(const timeval &a, const timeval &b)
{
    return !(a == b);
}

timeval
operator+(const timeval &a, const timeval &b)
{
    timeval res = a;
    res.tv_usec += b.tv_usec;
    res.tv_sec += b.tv_sec;
    if (res.tv_usec > 1000000) {
        res.tv_usec -= 1000000;
        ++res.tv_sec;
    }
    return res;
}

timeval
operator-(const timeval &a, const timeval &b)
{
    timeval res = a;
    if (res.tv_usec < b.tv_usec) {
        res.tv_usec += 1000000;
        --res.tv_sec;
    }
    res.tv_usec -= b.tv_usec;
    res.tv_sec -= b.tv_sec;
    return res;
}

#endif // TVHELPER_H
