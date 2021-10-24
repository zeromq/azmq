/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/detail/context_ops.hpp>

#include <boost/system/error_code.hpp>

#include <string>
#include <iostream>
#include <exception>

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"


TEST_CASE( "context_creation", "[context]") {
    auto ctx = azmq::detail::context_ops::get_context();
    auto ctx2 = azmq::detail::context_ops::get_context(true);
    REQUIRE(ctx != ctx2);

    auto ctx3 = azmq::detail::context_ops::get_context();
    REQUIRE(ctx == ctx3);
}

TEST_CASE( "context_options", "[context]" ) {
    auto ctx = azmq::detail::context_ops::get_context();
    using io_threads = azmq::detail::context_ops::io_threads;
    boost::system::error_code ec;
    azmq::detail::context_ops::set_option(ctx, io_threads(2), ec);
    REQUIRE(!ec);

    io_threads res = 0;
    azmq::detail::context_ops::get_option(ctx, res, ec);
    REQUIRE(!ec);
    REQUIRE(res.value() == 2);
}

TEST_CASE( "invalid option", "[context]" ) {
  auto ctx = azmq::detail::context_ops::get_context();
  using io_threads = azmq::detail::context_ops::io_threads;
  boost::system::error_code ec;
  azmq::detail::context_ops::set_option(ctx, io_threads(-1), ec);
  REQUIRE(ec);
  REQUIRE(ec.value() == EINVAL);
}
