/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_SEND_OP_HPP_
#define AZMQ_DETAIL_SEND_OP_HPP_
#include "../error.hpp"
#include "../message.hpp"
#include "socket_ops.hpp"
#include "reactor_op.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/bind_allocator.hpp>

#include <zmq.h>
#include <iterator>

namespace azmq {
namespace detail {

template<typename ConstBufferSequence>
class send_buffer_op_base : public reactor_op {
public:
    send_buffer_op_base(ConstBufferSequence const& buffers, flags_type flags)
        : buffers_(buffers)
        , flags_(flags)
        { }

    virtual bool do_perform(socket_type& socket) override {
        ec_ = boost::system::error_code();
        bytes_transferred_ += socket_ops::send(buffers_, socket, flags_ | ZMQ_DONTWAIT, ec_);
        if (ec_) {
            return !try_again();
        }
        return true;
    }

private:
    ConstBufferSequence buffers_;
    flags_type flags_;
};

template<typename ConstBufferSequence,
         typename Handler>
class send_buffer_op : public send_buffer_op_base<ConstBufferSequence> {
public:
    send_buffer_op(ConstBufferSequence const& buffers,
                   Handler handler,
                   reactor_op::flags_type flags)
        : send_buffer_op_base<ConstBufferSequence>(buffers, flags)
        , handler_(std::move(handler))
        , work_guard(boost::asio::make_work_guard(handler_))
    { }

    virtual void do_complete() override {
        auto alloc = boost::asio::get_associated_allocator(
            handler_, boost::asio::recycling_allocator<void>());
        boost::asio::dispatch(work_guard.get_executor(), boost::asio::bind_allocator(alloc, [ec_ = this->ec_, handler_ = std::move(handler_), bytes_transferred_ = this->bytes_transferred_]() mutable {
            handler_(ec_, bytes_transferred_);
        }));
    }

private:
    Handler handler_;
    boost::asio::executor_work_guard<typename boost::asio::associated_executor<Handler>::type> work_guard;
};

class send_op_base : public reactor_op {
public:
    send_op_base(message msg, flags_type flags)
        : msg_(std::move(msg))
        , flags_(flags)
        { }

    virtual bool do_perform(socket_type & socket) override {
        ec_ = boost::system::error_code();
        bytes_transferred_ = socket_ops::send(msg_, socket, flags_ | ZMQ_DONTWAIT, ec_);
        if (ec_)
            return !try_again(); // some other error
        return true;
    };

private:
    message msg_;
    flags_type flags_;
};

template<typename Handler>
class send_op : public send_op_base {
public:
    send_op(message msg,
            Handler handler,
            flags_type flags)
        : send_op_base(std::move(msg), flags)
        , handler_(std::move(handler))
        , work_guard(boost::asio::make_work_guard(handler_))
    { }

    virtual void do_complete() override {
        auto alloc = boost::asio::get_associated_allocator(
            handler_, boost::asio::recycling_allocator<void>());
        boost::asio::dispatch(work_guard.get_executor(), boost::asio::bind_allocator(alloc, [ec_ = this->ec_, handler_ = std::move(handler_), bytes_transferred_ = this->bytes_transferred_]() mutable {
            handler_(ec_, bytes_transferred_);
        }));
    }

private:
    Handler handler_;
    boost::asio::executor_work_guard<typename boost::asio::associated_executor<Handler>::type> work_guard;
};

} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_SEND_OP_HPP_


