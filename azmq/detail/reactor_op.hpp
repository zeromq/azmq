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
#include <boost/asio/error.hpp>
#include <boost/intrusive/list.hpp>

namespace azmq {
namespace detail {
class reactor_op {
public:
    using socket_type = socket_ops::socket_type;
    using flags_type = socket_ops::flags_type;
    boost::intrusive::list_member_hook<> member_hook_;
    boost::system::error_code ec_;
    size_t bytes_transferred_;

    bool do_perform(socket_type & socket) { return perform_func_(this, socket); }
    static void do_complete(reactor_op * op) {
        op->complete_func_(op, op->ec_, op->bytes_transferred_);
    }

    static boost::system::error_code canceled() {
        static boost::system::error_code ec = make_error_code(boost::asio::error::operation_aborted);
        return ec;
    }

protected:
    typedef bool (*perform_func_type)(reactor_op*, socket_type &);
    typedef void (*complete_func_type)(reactor_op* op, boost::system::error_code const&, size_t);

    perform_func_type perform_func_;
    complete_func_type complete_func_;

    bool try_again() const {
        return ec_.value() == boost::asio::error::try_again;
    }

    bool is_canceled() const { return ec_.value() == boost::asio::error::operation_aborted; }

    reactor_op(perform_func_type perform_func,
               complete_func_type complete_func)
        : bytes_transferred_(0)
        , perform_func_(perform_func)
        , complete_func_(complete_func)
    { }

};

} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_REACTOR_OP_HPP_

