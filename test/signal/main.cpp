/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/signal.hpp>

#include <boost/asio/io_service.hpp>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE( "Send/Receive a signal", "[signal]" ) {
    boost::asio::io_service ios;
    azmq::pair_socket sb(ios);
    azmq::pair_socket sc(ios);

    sb.bind("inproc://test");
    sc.connect("inproc://test");

    azmq::signal::send(sb, 123);
    REQUIRE( azmq::signal::wait(sc) == 123);
}
