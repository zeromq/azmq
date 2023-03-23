/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/socket.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <coroutine>
#include <array>
#include <string>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

std::string subj(const char *name) {
    return std::string("inproc://") + name;
}

TEST_CASE("coroutine send/receive message", "[socket_cpp20]") {
    boost::asio::io_context ioc;

    azmq::socket sb(ioc, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::optional<size_t> btc{};
    boost::optional<size_t> btb{};

    //sending coroutine
    co_spawn(ioc, [&]() -> boost::asio::awaitable<void> {
      std::array<boost::asio::const_buffer, 2> snd_bufs = {{
                                                               boost::asio::buffer("A"),
                                                               boost::asio::buffer("B")
                                                           }};

      btc = co_await azmq::async_send(sc, snd_bufs, boost::asio::use_awaitable);
      co_return;
    }, boost::asio::detached);

    //receiving coroutine
    co_spawn(ioc, [&]() -> boost::asio::awaitable<void> {
      std::array<char, 5> ident;
      std::array<char, 2> a;
      std::array<char, 2> b;

      std::array<boost::asio::mutable_buffer, 3> rcv_bufs = {{
                                                                 boost::asio::buffer(ident),
                                                                 boost::asio::buffer(a),
                                                                 boost::asio::buffer(b)
                                                             }};

      btb = co_await azmq::async_receive(sb, rcv_bufs, boost::asio::use_awaitable);
      co_return;
    }, boost::asio::detached);

    ioc.run();

    REQUIRE(btb.has_value());
    REQUIRE(btb.value() == 9);

    REQUIRE(btc.has_value());
    REQUIRE(btc.value() == 4);
}

