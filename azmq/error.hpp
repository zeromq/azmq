/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_ERROR_HPP_
#define AZMQ_ERROR_HPP_
#include "detail/config.hpp"

#include <boost/system/error_code.hpp>
#include <boost/version.hpp>
#include <zmq.h>
#include <string>


#if BOOST_VERSION < 105400
    #ifndef BOOST_NOEXCEPT
        #ifdef BOOST_NO_CXX11_NOEXCEPT
            #define BOOST_NOEXCEPT
        #else
            #define BOOST_NOEXCEPT noexcept
        #endif
    #endif
    #define BOOST_SYSTEM_NOEXCEPT BOOST_NOEXCEPT
#endif

namespace azmq {
AZMQ_V1_INLINE_NAMESPACE_BEGIN
    /** \brief custom error_category to map zeromq errors */
    class error_category : public boost::system::error_category {
    public:
        const char* name() const BOOST_SYSTEM_NOEXCEPT override {
            return "ZeroMQ";
        }

        std::string message(int ev) const override {
            return std::string(zmq_strerror(ev));
        }
    };

    inline boost::system::error_code make_error_code(int ev = errno) {
        static error_category cat;

        return boost::system::error_code(ev, cat);
    }
AZMQ_V1_INLINE_NAMESPACE_END
} // namespace azmq
#endif // AZMQ_ERROR_HPP_

