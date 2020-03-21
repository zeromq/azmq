/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/socket.hpp>
#include <azmq/util/scope_guard.hpp>

#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/asio/buffer.hpp>

#include <array>
#include <thread>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

std::array<boost::asio::const_buffer, 2> snd_bufs = {{
    boost::asio::buffer("A"),
    boost::asio::buffer("B")
}};

std::string subj(const char* name) {
    return std::string("inproc://") + name;
}

TEST_CASE( "Set/Get options", "[socket]" ) {
    boost::asio::io_context ios;

    azmq::socket s(ios, ZMQ_ROUTER);

    // set/get_option are generic, works for one and all...
    azmq::socket::rcv_hwm in_hwm(42);
    s.set_option(in_hwm);

    azmq::socket::rcv_hwm out_hwm;
    s.get_option(out_hwm);
    REQUIRE(in_hwm.value() == out_hwm.value());

    azmq::socket::allow_speculative in_speculative(false);
    s.set_option(in_speculative);

    azmq::socket::allow_speculative out_speculative;
    s.get_option(out_speculative);
    REQUIRE(in_speculative.value() == out_speculative.value());
}

TEST_CASE( "Send/Receive single buffer", "[socket]") {
    boost::asio::io_context ios;

    azmq::socket sb(ios, ZMQ_PAIR);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ios, ZMQ_PAIR);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    auto msg = "TEST";
    auto snd_buf = boost::asio::const_buffer(msg, 5);
    auto sz1 = sc.send(snd_buf);

    std::array<char, 256> buf;
    auto sz2 = sb.receive(boost::asio::buffer(buf));

    REQUIRE(sz1 == sz2);
    REQUIRE(boost::string_ref(msg) == boost::string_ref(buf.data()));
}

TEST_CASE( "Send/Receive synchronous", "[socket]" ) {
    boost::asio::io_context ios;

    azmq::socket sb(ios, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ios, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    sc.send(snd_bufs);

    azmq::message msg;
    auto size = sb.receive(msg);
    REQUIRE(msg.more() == true);

    size = sb.receive(msg, ZMQ_RCVMORE);
    REQUIRE(size == boost::asio::buffer_size(snd_bufs[0]));
    REQUIRE(msg.more() == true);

    size = sb.receive(msg);
    REQUIRE(size == boost::asio::buffer_size(snd_bufs[1]));
    REQUIRE(msg.more() == false);

    sc.send(snd_bufs);

    std::array<char, 5> ident;
    std::array<char, 2> a;
    std::array<char, 2> b;

    std::array<boost::asio::mutable_buffer, 3> rcv_bufs = {{
        boost::asio::buffer(ident),
        boost::asio::buffer(a),
        boost::asio::buffer(b)
    }};

    size = sb.receive(rcv_bufs);
    REQUIRE(size == 9);
}

TEST_CASE( "Send/Receive async", "[socket_ops]" ) {
    boost::asio::io_context ios_b;
    boost::asio::io_context ios_c;

    azmq::socket sb(ios_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ios_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ios_c.stop(); };
        ecc = ec;
        btc = bytes_transferred;
    });

    std::array<char, 5> ident;
    std::array<char, 2> a;
    std::array<char, 2> b;

    std::array<boost::asio::mutable_buffer, 3> rcv_bufs = {{
        boost::asio::buffer(ident),
        boost::asio::buffer(a),
        boost::asio::buffer(b)
    }};

    boost::system::error_code ecb;
    size_t btb = 0;
    sb.async_receive(rcv_bufs, [&](boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ios_b.stop(); };
        ecb = ec;
        btb = bytes_transferred;
    });

    ios_c.run();
    ios_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecb == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Send/Receive async is_speculative", "[socket_ops]" ) {
    boost::asio::io_context ios_b;
    boost::asio::io_context ios_c;

    azmq::socket sb(ios_b, ZMQ_ROUTER);
    sb.set_option(azmq::socket::allow_speculative(true));
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ios_c, ZMQ_DEALER);
    sc.set_option(azmq::socket::allow_speculative(true));
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ios_c.stop(); };
        ecc = ec;
        btc = bytes_transferred;
    });

    std::array<char, 5> ident;
    std::array<char, 2> a;
    std::array<char, 2> b;

    std::array<boost::asio::mutable_buffer, 3> rcv_bufs = {{
        boost::asio::buffer(ident),
        boost::asio::buffer(a),
        boost::asio::buffer(b)
    }};

    boost::system::error_code ecb;
    size_t btb = 0;
    sb.async_receive(rcv_bufs, [&](boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ios_b.stop(); };
        ecb = ec;
        btb = bytes_transferred;
    });

    ios_c.run();
    ios_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecb == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Send/Receive async threads", "[socket]" ) {
    boost::asio::io_context ios_b;
    azmq::socket sb(ios_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    boost::asio::io_context ios_c;
    azmq::socket sc(ios_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    std::thread tc([&] {
        sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
            SCOPE_EXIT { ios_c.stop(); };
            ecc = ec;
            btc = bytes_transferred;
        });
        ios_c.run();
    });

    boost::system::error_code ecb;
    size_t btb = 0;
    std::thread tb([&] {
        std::array<char, 5> ident;
        std::array<char, 2> a;
        std::array<char, 2> b;

        std::array<boost::asio::mutable_buffer, 3> rcv_bufs = {{
            boost::asio::buffer(ident),
            boost::asio::buffer(a),
            boost::asio::buffer(b)
        }};

        sb.async_receive(rcv_bufs, [&](boost::system::error_code const& ec, size_t bytes_transferred) {
            SCOPE_EXIT { ios_b.stop(); };
            ecb = ec;
            btb = bytes_transferred;
        });
        ios_b.run();
    });

    tc.join();
    tb.join();
    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Send/Receive message async", "[socket]" ) {
    boost::asio::io_context ios_b;
    boost::asio::io_context ios_c;

    azmq::socket sb(ios_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ios_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ios_c.stop(); };
        ecc = ec;
        btc = bytes_transferred;
    });

    std::array<char, 5> ident;
    std::array<char, 2> a;
    std::array<char, 2> b;

    boost::system::error_code ecb;
    size_t btb = 0;
    sb.async_receive([&](boost::system::error_code const& ec, azmq::message & msg, size_t bytes_transferred) {
        SCOPE_EXIT { ios_b.stop(); };
        ecb = ec;
        if (ecb)
            return;
        btb += bytes_transferred;
        msg.buffer_copy(boost::asio::buffer(ident));

        if (msg.more()) {
            btb += sb.receive(msg, ZMQ_RCVMORE, ecb);
            if (ecb)
                return;
            msg.buffer_copy(boost::asio::buffer(a));
        }

        if (msg.more()) {
            btb += sb.receive(msg, 0, ecb);
            if (ecb)
                return;
            msg.buffer_copy(boost::asio::buffer(b));
        }
    });

    ios_c.run();
    ios_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Send/Receive message more async", "[socket]" ) {
    boost::asio::io_context ios_b;
    boost::asio::io_context ios_c;

    azmq::socket sb(ios_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ios_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ios_c.stop(); };
        ecc = ec;
        btc = bytes_transferred;
    });

    std::array<char, 5> ident;
    std::array<char, 2> a;
    std::array<char, 2> b;

    std::array<boost::asio::mutable_buffer, 2> rcv_bufs = {{
        boost::asio::buffer(a),
        boost::asio::buffer(b)
    }};

    boost::system::error_code ecb;
    size_t btb = 0;
    sb.async_receive([&](boost::system::error_code const& ec, azmq::message & msg, size_t bytes_transferred) {
        SCOPE_EXIT { ios_b.stop(); };
        ecb = ec;
        if (ecb)
            return;
        btb += bytes_transferred;
        msg.buffer_copy(boost::asio::buffer(ident));

        if (!msg.more())
            return;

        azmq::message_vector v;
        btb += sb.receive_more(v, 0, ecb);
        if (ecb)
            return;
        auto it = std::begin(v);
        for (auto&& buf : rcv_bufs)
            (*it++).buffer_copy(buf);
    });

    ios_c.run();
    ios_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btb == 9);
}

struct monitor_handler {

#if defined BOOST_MSVC
#pragma pack(push, 1)
    struct event_t
    {
        uint16_t e;
        uint32_t i;
    };
#pragma pack(pop)
#else
    struct event_t
    {
        uint16_t e;
        uint32_t i;
    } __attribute__((packed));
#endif

    azmq::socket socket_;
    std::string role_;
    std::vector<event_t> events_;

    monitor_handler(boost::asio::io_context & ios, azmq::socket& s, std::string role)
        : socket_(s.monitor(ios, ZMQ_EVENT_ALL))
        , role_(std::move(role))
    { }

    void start()
    {
        socket_.async_receive([this](boost::system::error_code const& ec,
                                     azmq::message & msg, size_t) {
                if (ec)
                    return;
                event_t event;
                msg.buffer_copy(boost::asio::buffer(&event, sizeof(event)));
                events_.push_back(event);
                socket_.flush();
                start();
            });
    }

    void cancel()
    {
        socket_.cancel();
    }
};

void bounce(azmq::socket & server, azmq::socket & client) {
    const char *content = "12345678ABCDEFGH12345678abcdefgh";
    std::array<boost::asio::const_buffer, 2> snd_bufs = {{
        boost::asio::buffer(content, 32),
        boost::asio::buffer(content, 32)
    }};

    std::array<char, 32> buf0;
    std::array<char, 32> buf1;

    std::array<boost::asio::mutable_buffer, 2> rcv_bufs = {{
        boost::asio::buffer(buf0),
        boost::asio::buffer(buf1)
    }};
    client.send(snd_bufs);
    server.receive(rcv_bufs);
    server.send(snd_bufs);
    client.receive(rcv_bufs);
}

TEST_CASE( "Socket Monitor", "[socket]" ) {
    boost::asio::io_context ios;
    boost::asio::io_context ios_m;

    using socket_ptr = std::unique_ptr<azmq::socket>;
    socket_ptr client(new azmq::socket(ios, ZMQ_DEALER));
    socket_ptr server(new azmq::socket(ios, ZMQ_DEALER));

    monitor_handler client_monitor(ios_m, *client, "client");
    monitor_handler server_monitor(ios_m, *server, "server");

    client_monitor.start();
    server_monitor.start();

    std::thread t([&] {
        ios_m.run();
    });

    server->bind("tcp://127.0.0.1:9998");
    client->connect("tcp://127.0.0.1:9998");

    bounce(*client, *server);

    // On Windows monitored sockets must be closed before their monitors,
    // otherwise ZMQ crashes or deadlocks during the context termination.
    // ZMQ's bug?
    client.reset();
    server.reset();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    ios_m.stop();
    t.join();

    // ZeroMQ added ZMQ_EVENT_HANDSHAKE_SUCCEEDED event after CONNECTED / ACCEPTED event.
    REQUIRE(client_monitor.events_.size() == 4);
    CHECK(client_monitor.events_[0].e == ZMQ_EVENT_CONNECT_DELAYED);
    CHECK(client_monitor.events_[1].e == ZMQ_EVENT_CONNECTED);
    CHECK(client_monitor.events_[2].e == ZMQ_EVENT_HANDSHAKE_SUCCEEDED);
    CHECK(client_monitor.events_[3].e == ZMQ_EVENT_MONITOR_STOPPED);

    REQUIRE(server_monitor.events_.size() == 5);
    CHECK(server_monitor.events_[0].e == ZMQ_EVENT_LISTENING);
    CHECK(server_monitor.events_[1].e == ZMQ_EVENT_ACCEPTED);
    CHECK(server_monitor.events_[2].e == ZMQ_EVENT_HANDSHAKE_SUCCEEDED);
    CHECK(server_monitor.events_[3].e == ZMQ_EVENT_CLOSED);
    CHECK(server_monitor.events_[4].e == ZMQ_EVENT_MONITOR_STOPPED);
}

TEST_CASE( "Attach Method", "[socket]" ) {
    using namespace boost::algorithm;
    boost::asio::io_context ios;
    azmq::dealer_socket s(ios);

    std::vector<std::string> elems;

    azmq::attach(s, split(elems, "@inproc://myendpoint,tcp://127.0.0.1:5556,inproc://others", is_any_of(",")), true);
    REQUIRE(s.endpoint() == "inproc://others");
}

TEST_CASE( "Pub/Sub", "[socket]" ) {
    boost::asio::io_context ios;
    azmq::sub_socket subscriber(ios);
    subscriber.connect("tcp://127.0.0.1:5556");
    subscriber.set_option(azmq::socket::subscribe("FOO"));

    azmq::pub_socket publisher(ios);
    publisher.bind("tcp://127.0.0.1:5556");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    publisher.send(boost::asio::buffer(std::string("FOOBAR")));
    std::array<char, 256> buf;
    auto size = subscriber.receive(boost::asio::buffer(buf));

    REQUIRE(size == 6);
}

struct state {
    size_t max;
    size_t ct;
    std::array<char, 5> ident;
    std::array<char, 256> buf;
    boost::system::error_code ec;

    state(size_t m) : max(m), ct(0) { }

    static void run(state & s, azmq::socket & sb) {
        std::array<boost::asio::mutable_buffer, 2> rcv_bufs = {{
            boost::asio::buffer(s.ident),
            boost::asio::buffer(s.buf)
        }};

        sb.async_receive(rcv_bufs, [&] (boost::system::error_code const& ec, size_t) {
            if (ec) {
                s.ec = ec;
                sb.get_io_context().stop();
                return;
            }

            if (++s.ct >= s.max)
                sb.get_io_context().stop();
            run(s, sb);
        });
    }
};

TEST_CASE( "Loopback", "[socket]" ) {
    boost::asio::io_context ios_b;
    boost::asio::io_context ios_c;

    azmq::socket sb(ios_b, ZMQ_ROUTER);
    sb.bind("tcp://127.0.0.1:5560");

    azmq::socket sc(ios_c, ZMQ_DEALER);
    sc.connect("tcp://127.0.0.1:5560");

    size_t ct = 100000;
    state s(ct);
    std::thread t([&] {
        state::run(s, sb);
        ios_b.run();
    });

    for (auto i = 0u; i < ct; ++i) {
        sc.send(boost::asio::buffer(&i, sizeof(i)));
    }

    t.join();

    REQUIRE(s.ec == boost::system::error_code());
    REQUIRE(s.ct == ct);
}
