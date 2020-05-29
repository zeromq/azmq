/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_RECEIVE_OP_HPP_
#define AZMQ_DETAIL_RECEIVE_OP_HPP_

#include "../error.hpp"
#include "../message.hpp"
#include "socket_ops.hpp"
#include "reactor_op.hpp"

#include <boost/asio/io_service.hpp>

#include <zmq.h>

#include <iterator>

namespace azmq {
namespace detail {
template<typename MutableBufferSequence>
class receive_buffer_op_base : public reactor_op {
public:
    receive_buffer_op_base(MutableBufferSequence const& buffers,
                           flags_type flags,
                           complete_func_type complete_func)
        : reactor_op(&receive_buffer_op_base::do_perform, complete_func)
        , buffers_(buffers)
        , flags_(flags)
        { }

    static bool do_perform(reactor_op* base, socket_type & socket) {
        auto o = static_cast<receive_buffer_op_base*>(base);
        o->ec_ = boost::system::error_code();

        o->bytes_transferred_ += socket_ops::receive(o->buffers_, socket, o->flags_ | ZMQ_DONTWAIT, o->ec_);
        if (o->ec_)
            return !o->try_again();
        return true;
    }

protected:
    bool more() const {
        return ec_ == boost::system::errc::no_buffer_space && bytes_transferred_;
    }

private:
    MutableBufferSequence buffers_;
    flags_type flags_;
};

template<typename MutableBufferSequence,
         typename Handler>
class receive_buffer_op : public receive_buffer_op_base<MutableBufferSequence> {
public:
    receive_buffer_op(MutableBufferSequence const& buffers,
                      Handler handler,
                      socket_ops::flags_type flags)
        : receive_buffer_op_base<MutableBufferSequence>(buffers, flags,
                                                        &receive_buffer_op::do_complete)
        , handler_(std::move(handler))
        { }

    static void do_complete(reactor_op* base) {
        auto o = static_cast<receive_buffer_op*>(base);
        o->handler_(o->ec_, o->bytes_transferred_);
    }

private:
    Handler handler_;
};

template<typename MutableBufferSequence,
         typename Handler>
class receive_more_buffer_op : public receive_buffer_op_base<MutableBufferSequence> {
public:
    receive_more_buffer_op(MutableBufferSequence const& buffers,
                           Handler handler,
                           socket_ops::flags_type flags)
        : receive_buffer_op_base<MutableBufferSequence>(buffers, flags,
                                                        &receive_more_buffer_op::do_complete)
        , handler_(std::move(handler))
        { }

    static void do_complete(reactor_op* base) {
        auto o = static_cast<receive_more_buffer_op*>(base);
        o->handler_(o->ec_, std::make_pair(o->bytes_transferred_, o->more()));
    }

private:
    Handler handler_;
};

class receive_op_base : public reactor_op {
public:
    receive_op_base(socket_ops::flags_type flags,
                    complete_func_type complete_func)
        : reactor_op(&receive_op_base::do_perform, complete_func)
        , flags_(flags)
        { }

    static bool do_perform(reactor_op* base, socket_type & socket) {
        auto o = static_cast<receive_op_base*>(base);
        o->ec_ = boost::system::error_code();

        o->bytes_transferred_ = socket_ops::receive(o->msg_, socket, o->flags_ | ZMQ_DONTWAIT, o->ec_);
        if (o->ec_)
            return !o->try_again();
        return true;
    }

protected:
    message msg_;
    flags_type flags_;
};

template<typename Handler>
class receive_op : public receive_op_base {
public:
    receive_op(Handler handler,
               socket_ops::flags_type flags)
        : receive_op_base(flags, &receive_op::do_complete)
        , handler_(std::move(handler))
        { }

    static void do_complete(reactor_op* base) {
        auto o = static_cast<receive_op*>(base);
        o->handler_(o->ec_, o->msg_, o->bytes_transferred_);
    }

private:
    Handler handler_;
};
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_RECEIVE_OP_HPP_


