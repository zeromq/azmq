/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_CONTEXT_HPP_
#define AZMQ_CONTEXT_HPP_

#include "detail/socket_service.hpp"
#include "option.hpp"
#include "error.hpp"

#include <boost/asio/io_context.hpp>
#include <zmq.h>

namespace azmq {
AZMQ_V1_INLINE_NAMESPACE_BEGIN
    using io_threads = detail::context_ops::io_threads;
    using max_sockets = detail::context_ops::max_sockets;
    using ipv6 = detail::context_ops::ipv6;

    /** \brief set options on the zeromq context.
     *  \tparam Option option type
     *  \param option Option option to set
     *  \remark Must be called before any sockets are created
     */
    template<typename Option>
    boost::system::error_code set_option(boost::asio::io_context & ctxt,
                                         const Option & option,
                                         boost::system::error_code & ec) {
        return boost::asio::use_service<detail::socket_service>(ctxt).set_option(option, ec);
    }

    /** \brief set options on the zeromq context.
     *  \tparam Option option type
     *  \param option Option option to set
     *  \remark Must be called before any sockets are created
     */
    template<typename Option>
    void set_option(boost::asio::io_context & ctxt, const Option & option) {
        boost::system::error_code ec;
        if (set_option(ctxt, option, ec))
            throw boost::system::system_error(ec);
    }

    /** \brief get option from zeromq context
     *  \tparam Option option type
     *  \param option Option te get
     *  \param ec boost::system::error_code
     */
    template<typename Option>
    boost::system::error_code get_option(boost::asio::io_context & ctxt,
                                         Option & option,
                                         boost::system::error_code & ec) {
        return boost::asio::use_service<detail::socket_service>(ctxt).get_option(option, ec);
    }

    /** \brief get option from zeromq context
     *  \tparam Option option type
     *  \param option Option te get
     *  \param ec boost::system::error_code
     */
    template<typename Option>
    void get_option(boost::asio::io_context & ctxt, Option & option) {
        boost::system::error_code ec;
        if (get_option(ctxt, option))
            throw boost::system::system_error(ec);
    }
AZMQ_V1_INLINE_NAMESPACE_END
} // namespace azmq
#endif // AZMQ_CONTEXT_HPP_
