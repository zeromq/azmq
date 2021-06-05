/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_REACTOR_OP_HPP_
#define AZMQ_DETAIL_REACTOR_OP_HPP_

#include "../message.hpp"
#include "socket_ops.hpp"

#include <boost/optional.hpp>
#include <boost/asio/io_service.hpp>

namespace azmq {
namespace detail {
class reactor_op {
public:
    using socket_type = socket_ops::socket_type;
    using flags_type = socket_ops::flags_type;
    boost::system::error_code ec_;
    size_t bytes_transferred_ = 0;

    virtual ~reactor_op() = default;
    virtual bool do_perform(socket_type& socket) = 0;
    virtual void do_complete() = 0;

    static boost::system::error_code canceled() { return boost::asio::error::operation_aborted; }

protected:
    bool try_again() const {
        return ec_.value() == boost::system::errc::resource_unavailable_try_again;
    }

    bool is_canceled() const { return ec_ == canceled(); }
};

} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_REACTOR_OP_HPP_

