/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/actor.hpp>
#include <azmq/util/scope_guard.hpp>

#include <boost/asio/buffer.hpp>

#include <array>
#include <thread>
#include <iostream>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

std::array<boost::asio::const_buffer, 2> snd_bufs = {{
    boost::asio::buffer("A"),
    boost::asio::buffer("B")
}};

std::string subj(const char* name) {
    return std::string("inproc://") + name;
}

TEST_CASE( "Async Send/Receive", "[actor]" ) {
    boost::system::error_code ecc;
    size_t btc = 0;

    boost::system::error_code ecb;
    size_t btb = 0;
    {
        std::array<char, 2> a;
        std::array<char, 2> b;

        std::array<boost::asio::mutable_buffer, 2> rcv_bufs = {{
            boost::asio::buffer(a),
            boost::asio::buffer(b)
        }};

        boost::asio::io_service ios;
        auto s = azmq::actor::spawn(ios, [&](azmq::socket & ss) {
            ss.async_receive(rcv_bufs, [&](boost::system::error_code const& ec, size_t bytes_transferred) {
                ecb = ec;
                btb = bytes_transferred;
                ios.stop();
            });
            ss.get_io_service().run();
        });

        s.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
            ecc = ec;
            btc = bytes_transferred;
        });

        boost::asio::io_service::work w(ios);
        ios.run();
    }

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecb == boost::system::error_code());
    REQUIRE(btb == 4);
}
