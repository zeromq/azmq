/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/message.hpp>

#include <boost/asio/buffer.hpp>

#include <string>
#include <algorithm>
#include <array>
#include <iterator>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>


TEST_CASE( "message_constructors", "[message]" ) {
    // default init range has 0 size
    azmq::message m;
    REQUIRE(m.size() == 0);

    // pre-sized message construction
    azmq::message mm(42);
    REQUIRE(mm.size() == 42);

    // implicit construction from asio::const_buffer
    std::string s("This is a test");
    azmq::message mstr(boost::asio::buffer(s));
    REQUIRE(s.size() == mstr.size());
    REQUIRE(s ==  mstr.string());

    // construction from string
    azmq::message mmstr(s);
    REQUIRE(s == mmstr.string());
}

char global_buf[1024];
int global_hint;
int global_ctr;


void free_fn1(void *buf)
{
    REQUIRE(buf == &global_buf);
    ++global_ctr;
}

void free_fn2(void *buf, void *hint)
{
    REQUIRE(buf == &global_buf);
    REQUIRE(hint == &global_hint);
    ++global_ctr;
}


TEST_CASE( "deleter", "[message]" ) {
    global_ctr = 0;

    {
        azmq::message m1(azmq::nocopy, boost::asio::buffer("static const buf"));
        REQUIRE(17 == m1.size());
    }

    {
        azmq::message m2(azmq::nocopy, boost::asio::buffer(global_buf), [](void *buf){
            REQUIRE(buf == &global_buf);
            ++global_ctr;
        });
        REQUIRE(sizeof(global_buf) == m2.size());
    }
    REQUIRE(1 == global_ctr);

    {
        azmq::message m3(azmq::nocopy, boost::asio::buffer(global_buf), &global_hint, [](void *buf, void *hint){
            REQUIRE(buf == global_buf);
            REQUIRE(hint == &global_hint);
            ++global_ctr;
        });
        REQUIRE(sizeof(global_buf) == m3.size());
    }
    REQUIRE(2 == global_ctr);

    {
        char buf2[16];
        int x = 42;
        azmq::message m4(azmq::nocopy, boost::asio::buffer(buf2), [x, &buf2](void *buf){
            REQUIRE(buf == buf2);
            REQUIRE(x == 42);
            ++global_ctr;
        });
        REQUIRE(sizeof(buf2) == m4.size());
    }
    REQUIRE(3 == global_ctr);

    {
        azmq::message m5(azmq::nocopy, boost::asio::buffer(global_buf), &global_hint, free_fn2);
        REQUIRE(sizeof(global_buf) == m5.size());
    }
    REQUIRE(4 == global_ctr);

    {
        azmq::message m6(azmq::nocopy, boost::asio::buffer(global_buf), &free_fn1);
        REQUIRE(sizeof(global_buf) == m6.size());
    }
    REQUIRE(5 == global_ctr);


    {
        azmq::message m7;
        {
            azmq::message m8(azmq::nocopy, boost::asio::buffer(global_buf), free_fn1);
            REQUIRE(sizeof(global_buf) == m8.size());
            m7 = m8;
            REQUIRE(sizeof(global_buf) == m7.size());

        }
        REQUIRE(5 == global_ctr); // msg is not deleted yet
    }
    REQUIRE(6 == global_ctr);
}

TEST_CASE( "message_buffer_operations", "[message]" ) {
    azmq::message mm(42);
    // implicit cast to const_buffer
    boost::asio::const_buffer b = mm.cbuffer();
    REQUIRE(boost::asio::buffer_size(b) == mm.size());

    // implicit cast to mutable_buffer
    boost::asio::mutable_buffer bb = mm.buffer();
    REQUIRE(boost::asio::buffer_size(bb) == mm.size());
}

TEST_CASE( "message_copy_operations", "[message]" ) {
    azmq::message m(42);
    azmq::message mm(m);
    REQUIRE(m.size() == 42);
    REQUIRE(mm.size() == 42);

    azmq::message mmm = m;
    REQUIRE(m.size() == 42);
    REQUIRE(mmm.size() == 42);
}

TEST_CASE( "message_move_operations", "[message]" ) {
    azmq::message m;
    azmq::message mm(42);

    // move assignment
    m = std::move(mm);
    REQUIRE(m.size() == 42);
    REQUIRE(mm.size() == 0);

    // move construction
    azmq::message mmm(std::move(m));
    REQUIRE(m.size() == 0);
    REQUIRE(mmm.size() == 42);
}

TEST_CASE( "write_through_mutable_buffer", "[message]" ) {
    azmq::message m("This is a test");

    azmq::message mm(m);
    boost::asio::mutable_buffer bb = mm.buffer();
    auto pstr = boost::asio::buffer_cast<char*>(bb);
    pstr[0] = 't';

    auto s = mm.string();
    REQUIRE(std::string("this is a test") == s);

    auto ss = m.string();
    REQUIRE(s != ss);
}

TEST_CASE( "comparison", "[message]" ) {
    using boost::asio::buffer;

    REQUIRE(azmq::message(buffer("bla-bla", 7)) == azmq::message(buffer("bla-bla", 7)));
    REQUIRE_FALSE(azmq::message(buffer("bla-bla", 7)) != azmq::message(buffer("bla-bla", 7)));

    REQUIRE(azmq::message(buffer("bla-bla", 7)) != azmq::message(buffer("bla-bla", 6)));
    REQUIRE_FALSE(azmq::message(buffer("bla-bla", 7)) == azmq::message(buffer("bla-bla", 6)));

    REQUIRE(azmq::message(buffer("bla-bla", 6)) != azmq::message(buffer("bla-bla", 7)));
    REQUIRE_FALSE(azmq::message(buffer("bla-bla", 6)) == azmq::message(buffer("bla-bla", 7)));

    REQUIRE_FALSE(azmq::message(buffer("bla-bla", 7)) == azmq::message(buffer("bla-BLB", 7)));
    REQUIRE(azmq::message(buffer("bla-bla", 7)) != azmq::message(buffer("bla-BLB", 7)));

    REQUIRE_FALSE(azmq::message(buffer("bla-BLB", 7)) == azmq::message(buffer("bla-bla", 7)));
    REQUIRE(azmq::message(buffer("bla-BLB", 7)) != azmq::message(buffer("bla-bla", 7)));
}


TEST_CASE( "message_data", "[message]" ) {
    azmq::message m("bla-bla");

    REQUIRE(m.size() == 7);
    REQUIRE(0 == memcmp(m.data(), "bla-bla", 7));
}

TEST_CASE( "message_sequence", "[message]" ) {
    std::string foo("foo");
    std::string bar("bar");

    std::array<boost::asio::const_buffer, 2> bufs {{
        boost::asio::buffer(foo),
        boost::asio::buffer(bar)
    }};

    // make a message_vector from a range
    auto res = azmq::to_message_vector(bufs);
    REQUIRE(res.size() == bufs.size());
    REQUIRE(foo == res[0].string());
    REQUIRE(bar == res[1].string());

    // implicit conversion
    res.push_back(boost::asio::buffer("BAZ"));
    REQUIRE(res.size() == bufs.size() + 1);

    // range of const_buffer -> range of message
    auto range = azmq::const_message_range(bufs);
    REQUIRE(std::distance(std::begin(bufs), std::end(bufs)) ==
            std::distance(std::begin(range), std::end(range)));

    auto it = std::begin(range);
    for(auto& buf : bufs) {
        REQUIRE(azmq::message(buf) == *it++);
    }
}
