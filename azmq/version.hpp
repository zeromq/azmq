/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef AZMQ_VERSION_HPP_
#define AZMQ_VERSION_HPP_

/*  Version macros for compile-time API version detection                     */
#define AZMQ_VERSION_MAJOR 1
#define AZMQ_VERSION_MINOR 0
#define AZMQ_VERSION_PATCH 2

#define AZMQ_MAKE_VERSION(major, minor, patch)                                  \
    ((major) *10000 + (minor) *100 + (patch))
#define AZMQ_VERSION                                                            \
AZMQ_MAKE_VERSION (AZMQ_VERSION_MAJOR, AZMQ_VERSION_MINOR, AZMQ_VERSION_PATCH)

#endif // AZMQ_VERSION_HPP_
