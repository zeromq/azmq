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

#include <boost/version.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor_work_guard.hpp>
#if BOOST_VERSION >= 107900
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/bind_allocator.hpp>
#endif

#include <zmq.h>

#include <iterator>

namespace azmq {
namespace detail {
template<typename MutableBufferSequence>
class receive_buffer_op_base : public reactor_op {
public:
    receive_buffer_op_base(MutableBufferSequence const& buffers, flags_type flags)
        : buffers_(buffers)
        , flags_(flags)
        { }

    virtual bool do_perform(socket_type& socket) override {
        ec_ = boost::system::error_code();
        bytes_transferred_ += socket_ops::receive(buffers_, socket, flags_ | ZMQ_DONTWAIT, ec_);
        if (ec_)
            return !try_again();
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
        : receive_buffer_op_base<MutableBufferSequence>(buffers, flags)
        , handler_(std::move(handler))
        , work_guard(boost::asio::make_work_guard(handler_))
        { }

    virtual void do_complete() override {
#if BOOST_VERSION >= 107900
        auto alloc = boost::asio::get_associated_allocator(
            handler_, boost::asio::recycling_allocator<void>());
#endif
        boost::asio::dispatch(work_guard.get_executor(),
#if BOOST_VERSION >= 107900
            boost::asio::bind_allocator(alloc,
#endif
                [ec_ = this->ec_, handler_ = std::move(handler_), bytes_transferred_ = this->bytes_transferred_]() mutable {
            handler_(ec_, bytes_transferred_);
        })
#if BOOST_VERSION >= 107900
        )
#endif
            ;
    }

private:
    Handler handler_;
    boost::asio::executor_work_guard<typename boost::asio::associated_executor<Handler>::type> work_guard;
};

template<typename MutableBufferSequence,
         typename Handler>
class receive_more_buffer_op : public receive_buffer_op_base<MutableBufferSequence> {
public:
    receive_more_buffer_op(MutableBufferSequence const& buffers,
                           Handler handler,
                           socket_ops::flags_type flags)
        : receive_buffer_op_base<MutableBufferSequence>(buffers, flags)
        , handler_(std::move(handler))
        , work_guard(boost::asio::make_work_guard(handler_))
        { }

    virtual void do_complete() override {
#if BOOST_VERSION >= 107900
        auto alloc = boost::asio::get_associated_allocator(
            handler_, boost::asio::recycling_allocator<void>());
#endif
        boost::asio::dispatch(work_guard.get_executor(),
#if BOOST_VERSION >= 107900
            boost::asio::bind_allocator(alloc,
#endif
                [ec_ = this->ec_, handler_ = std::move(handler_), bytes_transferred_ = this->bytes_transferred_, more = this->more()]() mutable {
            handler_(ec_, std::make_pair(bytes_transferred_, more));
        })
#if BOOST_VERSION >= 107900
        )
#endif
            ;
    }

private:
    Handler handler_;
    boost::asio::executor_work_guard<typename boost::asio::associated_executor<Handler>::type> work_guard;
};

class receive_op_base : public reactor_op {
public:
    receive_op_base(socket_ops::flags_type flags)
        : flags_(flags)
        { }

    virtual bool do_perform(socket_type& socket) override {
        ec_ = boost::system::error_code();
        bytes_transferred_ = socket_ops::receive(msg_, socket, flags_ | ZMQ_DONTWAIT, ec_);
        if (ec_)
            return !try_again();
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
        : receive_op_base(flags)
        , handler_(std::move(handler))
        , work_guard(boost::asio::make_work_guard(handler_))
        { }

    virtual void do_complete() override {
#if BOOST_VERSION >= 107900
        auto alloc = boost::asio::get_associated_allocator(
            handler_, boost::asio::recycling_allocator<void>());
#endif
        boost::asio::dispatch(work_guard.get_executor(),
#if BOOST_VERSION >= 107900
            boost::asio::bind_allocator(alloc,
#endif
                [ec_ = this->ec_, handler_ = std::move(handler_), msg_ = std::move(msg_), bytes_transferred_ = this->bytes_transferred_]() mutable {
            handler_(ec_, msg_, bytes_transferred_);
        })
#if BOOST_VERSION >= 107900
        )
#endif
            ;
    }

private:
    Handler handler_;
    boost::asio::executor_work_guard<typename boost::asio::associated_executor<Handler>::type> work_guard;
};
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_RECEIVE_OP_HPP_


