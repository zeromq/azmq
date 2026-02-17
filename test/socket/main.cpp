/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/socket.hpp>
#include <azmq/util/scope_guard.hpp>

#include <boost/current_function.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/version.hpp>


#if BOOST_VERSION >= 107000
#include <boost/asio/use_future.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>
#if BOOST_VERSION >= 107400
#include <boost/asio/any_io_executor.hpp>
#endif
#endif

#include <array>
#include <thread>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

static boost::string_ref message_ref(azmq::message const& msg) {
    return {static_cast<const char*>(msg.data()), msg.size()};
}

std::array<boost::asio::const_buffer, 2> snd_bufs = {{
    boost::asio::buffer("A"),
    boost::asio::buffer("B")
}};

std::string subj(const char* name) {
    return std::string("inproc://") + name;
}

TEST_CASE( "Set/Get options", "[socket]" ) {
    boost::asio::io_context ioc;

    azmq::socket s(ioc, ZMQ_ROUTER);

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
    boost::asio::io_context ioc;

    azmq::socket sb(ioc, ZMQ_PAIR);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc, ZMQ_PAIR);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    auto msg = "TEST";
    auto snd_buf = boost::asio::const_buffer(msg, 5);
    auto sz1 = sc.send(snd_buf);

    std::array<char, 256> buf;
    auto sz2 = sb.receive(boost::asio::buffer(buf));

    REQUIRE(sz1 == sz2);
    REQUIRE(boost::string_ref(msg) == boost::string_ref(buf.data()));
}

TEST_CASE( "Send/Receive single message", "[socket]") {
    boost::asio::io_context ioc;

    azmq::socket sb(ioc, ZMQ_PAIR);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc, ZMQ_PAIR);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    auto msg = "TEST";
    azmq::message snd_msg(msg);
    auto sz1 = sc.send(snd_msg);
    REQUIRE(boost::string_ref(msg) == message_ref(snd_msg));

    azmq::message rec_msg;
    auto sz2 = sb.receive(rec_msg);

    REQUIRE(sz1 == sz2);
    REQUIRE(boost::string_ref(msg) == message_ref(rec_msg));
}

TEST_CASE( "Send/Receive synchronous", "[socket]" ) {
    boost::asio::io_context ioc;

    azmq::socket sb(ioc, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc, ZMQ_DEALER);
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
    boost::asio::io_context ioc_b;
    boost::asio::io_context ioc_c;

    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ioc_c.stop(); };
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
        SCOPE_EXIT { ioc_b.stop(); };
        ecb = ec;
        btb = bytes_transferred;
    });

    ioc_c.run();
    ioc_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecb == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Async send/receive copies buffer refs", "[socket_ops]" ) {
    // Similar test as above, but with temporary boost::asio::const_buffer/boost::asio::mutable_buffer objects,
    // which should be copied to be stored internally.

    class Base {
    protected:
        boost::asio::io_context ioservice;
        azmq::pair_socket sock;
        boost::system::error_code error;
        size_t bytes = 0;

    public:
        Base() : sock(ioservice) {
        }

        virtual ~Base() { }

        void bind(const std::string &addr) {
            sock.bind(addr);
        }

        void connect(const std::string &addr) {
            sock.connect(addr);
        }

        void run() {
            ioservice.run();
        }

        std::string endpoint() const {
            return sock.endpoint();
        }

        boost::system::error_code get_error() const {
            return error;
        }

        size_t get_bytes_transferred() const {
            return bytes;
        }
    };

    class Sender : public Base {
        const std::array<char, 3> data = {{'a', 'b', 'c'}};

    public:
        Sender() : Base() {
        }

        void start_send() {
            sock.async_send(boost::asio::buffer(data),
                [this](boost::system::error_code const& ec, size_t bytes_transferred) {
                    SCOPE_EXIT { ioservice.stop(); };
                    error = ec;
                    bytes = bytes_transferred;
                }
            );
        }

        const std::array<char, 3> &get_data() const {
            return data;
        }
    };

    class Receiver : public Base {
        std::array<char, 3> data = {{0, 0, 0}};

    public:
        Receiver() : Base() {
        }

        void start_receive() {
            sock.async_receive(boost::asio::buffer(data),
                [this](boost::system::error_code const& ec, size_t bytes_transferred) {
                    SCOPE_EXIT { ioservice.stop(); };
                    error = ec;
                    bytes = bytes_transferred;
                }
            );
        }

        const std::array<char, 3> &get_data() const {
            return data;
        }
    };

    std::string endpoint = "tcp://127.0.0.1:*";
    Sender sender;
    sender.bind(endpoint);
    endpoint = sender.endpoint();
    REQUIRE(!endpoint.empty());
    sender.start_send();

    Receiver receiver;
    CHECK(receiver.get_bytes_transferred() == 0);
    CHECK(receiver.get_data() != sender.get_data());

    std::thread senderthread([&sender]() {
        sender.run();
    });

    std::thread receiverthread([&receiver, &endpoint]() {
        receiver.connect(endpoint);
        receiver.start_receive();
        receiver.run();
    });

    senderthread.join();
    receiverthread.join();

    CHECK(sender.get_error() == boost::system::error_code());
    CHECK(sender.get_bytes_transferred() == sender.get_data().size());

    CHECK(receiver.get_error() == boost::system::error_code());
    CHECK(receiver.get_bytes_transferred() == sender.get_data().size());

    CHECK(receiver.get_data() == sender.get_data());
}

TEST_CASE( "Send/Receive async is_speculative", "[socket_ops]" ) {
    boost::asio::io_context ioc_b;
    boost::asio::io_context ioc_c;

    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.set_option(azmq::socket::allow_speculative(true));
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.set_option(azmq::socket::allow_speculative(true));
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ioc_c.stop(); };
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
        SCOPE_EXIT { ioc_b.stop(); };
        ecb = ec;
        btb = bytes_transferred;
    });

    ioc_c.run();
    ioc_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecb == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Send/Receive async threads", "[socket]" ) {
    boost::asio::io_context ioc_b;
    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    boost::asio::io_context ioc_c;
    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    std::thread tc([&] {
        sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
            SCOPE_EXIT { ioc_c.stop(); };
            ecc = ec;
            btc = bytes_transferred;
        });
        ioc_c.run();
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
            SCOPE_EXIT { ioc_b.stop(); };
            ecb = ec;
            btb = bytes_transferred;
        });
        ioc_b.run();
    });

    tc.join();
    tb.join();
    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Send/Receive message async", "[socket]" ) {
    boost::asio::io_context ioc_b;
    boost::asio::io_context ioc_c;

    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ioc_c.stop(); };
        ecc = ec;
        btc = bytes_transferred;
    });

    std::array<char, 5> ident;
    std::array<char, 2> a;
    std::array<char, 2> b;

    boost::system::error_code ecb;
    size_t btb = 0;
    sb.async_receive([&](boost::system::error_code const& ec, azmq::message & msg, size_t bytes_transferred) {
        SCOPE_EXIT { ioc_b.stop(); };
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

    ioc_c.run();
    ioc_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE( "Send/Receive message more async", "[socket]" ) {
    boost::asio::io_context ioc_b;
    boost::asio::io_context ioc_c;

    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;
    sc.async_send(snd_bufs, [&] (boost::system::error_code const& ec, size_t bytes_transferred) {
        SCOPE_EXIT { ioc_c.stop(); };
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
        SCOPE_EXIT { ioc_b.stop(); };
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

    ioc_c.run();
    ioc_b.run();

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

    monitor_handler(boost::asio::io_context & ioc, azmq::socket& s, std::string role)
        : socket_(s.monitor(ioc, ZMQ_EVENT_ALL))
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
                if (event.e != ZMQ_EVENT_DISCONNECTED)
                {
                    events_.push_back(event);
                }
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
    boost::asio::io_context ioc;
    boost::asio::io_context ioc_m;

    using socket_ptr = std::unique_ptr<azmq::socket>;
    socket_ptr client(new azmq::socket(ioc, ZMQ_DEALER));

    socket_ptr server(new azmq::socket(ioc, ZMQ_DEALER));

    monitor_handler client_monitor(ioc_m, *client, "client");
    monitor_handler server_monitor(ioc_m, *server, "server");

    client_monitor.start();
    server_monitor.start();

    std::thread t([&] {
        ioc_m.run();
    });

    server->bind("tcp://127.0.0.1:*");
    auto endpoint = server->endpoint();
    REQUIRE(!endpoint.empty());
    client->connect(endpoint);

    bounce(*client, *server);

    // On Windows monitored sockets must be closed before their monitors,
    // otherwise ZMQ crashes or deadlocks during the context termination.
    // ZMQ's bug?
    client.reset();
    server.reset();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    ioc_m.stop();
    t.join();

    {
        auto has_event = [&client_monitor](uint16_t wanted) {
                for (auto const& ev : client_monitor.events_) {
                        if (ev.e == wanted)
                                return true;
                }
                return false;
        };

        CHECK(has_event(ZMQ_EVENT_CONNECT_DELAYED));
        CHECK(has_event(ZMQ_EVENT_CONNECTED));
        #ifdef ZMQ_EVENT_HANDSHAKE_SUCCEEDED
            CHECK(has_event(ZMQ_EVENT_HANDSHAKE_SUCCEEDED));
        #endif
        CHECK(has_event(ZMQ_EVENT_MONITOR_STOPPED));
    }

    {
        auto has_event = [&server_monitor](uint16_t wanted) {
                for (auto const& ev : server_monitor.events_) {
                        if (ev.e == wanted)
                                return true;
                }
                return false;
        };

        CHECK(has_event(ZMQ_EVENT_LISTENING));
        CHECK(has_event(ZMQ_EVENT_ACCEPTED));
        #ifdef ZMQ_EVENT_HANDSHAKE_SUCCEEDED
            CHECK(has_event(ZMQ_EVENT_HANDSHAKE_SUCCEEDED));
        #endif
        CHECK(has_event(ZMQ_EVENT_CLOSED));
        CHECK(has_event(ZMQ_EVENT_MONITOR_STOPPED));
    }
}

TEST_CASE( "Attach Method", "[socket]" ) {
    using namespace boost::algorithm;
    boost::asio::io_context ioc;
    azmq::dealer_socket s(ioc);

    std::vector<std::string> elems;

    azmq::attach(s, split(elems, "@inproc://myendpoint,tcp://127.0.0.1:*,inproc://others", is_any_of(",")), true);
    REQUIRE(s.endpoint() == "inproc://others");
}

TEST_CASE( "Pub/Sub", "[socket]" ) {
    boost::asio::io_context ioc;
    azmq::sub_socket subscriber(ioc);
    subscriber.set_option(azmq::socket::subscribe("FOO"));

    azmq::pub_socket publisher(ioc);
    publisher.bind("tcp://127.0.0.1:*");
    auto endpoint = publisher.endpoint();
    REQUIRE(!endpoint.empty());
    subscriber.connect(endpoint);

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
    boost::asio::io_context ioc_b;
    boost::asio::io_context ioc_c;

    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.bind("tcp://127.0.0.1:*");
    auto endpoint = sb.endpoint();
    REQUIRE(!endpoint.empty());

    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.connect(endpoint);

    size_t ct = 100000;
    state s(ct);
    std::thread t([&] {
        state::run(s, sb);
        ioc_b.run();
    });

    for (auto i = 0u; i < ct; ++i) {
        sc.send(boost::asio::buffer(&i, sizeof(i)));
    }

    t.join();

    REQUIRE(s.ec == boost::system::error_code());
    REQUIRE(s.ct == ct);
}

TEST_CASE( "socket_service does not call pending completion handlers when destroyed", "[socket]" ) {
    boost::asio::io_context ioservice;
    azmq::socket socket(ioservice, ZMQ_ROUTER);
    socket.bind(subj(BOOST_CURRENT_FUNCTION));
    socket.async_receive([](boost::system::error_code const& ec, azmq::message & msg, size_t bytes_transferred) {
        FAIL();
    });
}

#if BOOST_VERSION >= 107000

TEST_CASE("Async Operation Send/Receive with callback", "[socket_ops]") {
    boost::asio::io_context ioc_b;
    boost::asio::io_context ioc_c;

    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::system::error_code ecc;
    size_t btc = 0;

    azmq::async_send(sc, snd_bufs, [&](boost::system::error_code const &ec, size_t bytes_transferred) {
      SCOPE_EXIT
      { ioc_c.stop(); };
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
    azmq::async_receive(sb, rcv_bufs, [&](boost::system::error_code const &ec, size_t bytes_transferred) {
      SCOPE_EXIT
      { ioc_b.stop(); };
      ecb = ec;
      btb = bytes_transferred;
    });

    ioc_c.run();
    ioc_b.run();

    REQUIRE(ecc == boost::system::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecb == boost::system::error_code());
    REQUIRE(btb == 9);
}

TEST_CASE("Async Operation Send/Receive with future ", "[socket_ops]") {
    boost::asio::io_context ioc_b;
    boost::asio::io_context ioc_c;

    azmq::socket sb(ioc_b, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc_c, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    std::future<size_t> btc = azmq::async_send(sc, snd_bufs, boost::asio::use_future);

    std::array<char, 5> ident;
    std::array<char, 2> a;
    std::array<char, 2> b;

    std::array<boost::asio::mutable_buffer, 3> rcv_bufs = {{
                                                               boost::asio::buffer(ident),
                                                               boost::asio::buffer(a),
                                                               boost::asio::buffer(b)
                                                           }};

    std::future<size_t> btb = azmq::async_receive(sb, rcv_bufs, boost::asio::use_future);

    ioc_c.run();
    ioc_b.run();

    REQUIRE(btc.get() == 4);
    REQUIRE(btb.get() == 9);
}

// If boost >= 1.80 use completion token boost::asio::use_future
TEST_CASE("Async Operation Send/Receive with stackful coroutine", "[socket_ops]") {
    boost::asio::io_context ioc;

    azmq::socket sb(ioc, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    boost::optional<size_t> btc{};
    boost::optional<size_t> btb{};

    //send coroutine task
    # if BOOST_VERSION >= 108000
    auto future_send =
    # endif
    boost::asio::spawn(ioc, [&](boost::asio::yield_context yield) {
      btc = azmq::async_send(sc, snd_bufs, yield);
    }
    #if BOOST_VERSION >= 108000
    , boost::asio::use_future
    #endif
    );

    //receive coroutine task
    # if BOOST_VERSION >= 108000
    auto future_recv =
    # endif
    boost::asio::spawn(ioc, [&](boost::asio::yield_context yield) {
      std::array<char, 5> ident;
      std::array<char, 2> a;
      std::array<char, 2> b;

      std::array<boost::asio::mutable_buffer, 3> rcv_bufs = {{
                                                                 boost::asio::buffer(ident),
                                                                 boost::asio::buffer(a),
                                                                 boost::asio::buffer(b)
                                                             }};

      btb = azmq::async_receive(sb, rcv_bufs, yield);
    }
    #if BOOST_VERSION >= 108000
    , boost::asio::use_future
    #endif
    );

    ioc.run();
    
    // Make sure coroutines have executed
    # if BOOST_VERSION >= 108000
    REQUIRE(future_send.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    REQUIRE(future_recv.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    # endif

    REQUIRE(btc.has_value());
    REQUIRE(btc.value() == 4);

    REQUIRE(btb.has_value());
    REQUIRE(btb.value() == 9);

}

// If boost >= 1.80 use completion token boost::asio::use_future
TEST_CASE("Async Operation Send/Receive single message, stackful coroutine, one message at a time", "[socket_ops]") {
    boost::asio::io_context ioc;

    azmq::socket sb(ioc, ZMQ_ROUTER);
    sb.bind(subj(BOOST_CURRENT_FUNCTION));

    azmq::socket sc(ioc, ZMQ_DEALER);
    sc.connect(subj(BOOST_CURRENT_FUNCTION));

    //send coroutine task
    #if BOOST_VERSION >= 108000
    auto future_send = 
    #endif
    boost::asio::spawn(ioc, [&](boost::asio::yield_context yield) {
      auto const btc = azmq::async_send(sc, snd_bufs, yield);
      REQUIRE(btc == 4);
    }
    #if BOOST_VERSION >= 108000
    , boost::asio::use_future
    #endif
    );

    //receive coroutine task
    #if BOOST_VERSION >= 108000
    auto future_recv = 
    #endif
    boost::asio::spawn(ioc, [&](boost::asio::yield_context yield) {
      auto frame1 = azmq::message{};
      auto const btb1 = azmq::async_receive(sb, frame1, yield);
      REQUIRE(btb1 == 5);
      REQUIRE(frame1.more());

      auto frame2 = azmq::message{};
      auto const btb2 = azmq::async_receive(sb, frame2, yield);
      REQUIRE(btb2 == 2);
      REQUIRE(frame2.more());
      REQUIRE(message_ref(snd_bufs.at(0)) == message_ref(frame2));

      auto frame3 = azmq::message{};
      auto const btb3 = azmq::async_receive(sb, frame3, yield);
      REQUIRE(btb3 == 2);
      REQUIRE(!frame3.more());
      REQUIRE(message_ref(snd_bufs.at(1)) == message_ref(frame3));
    }
    #if BOOST_VERSION >= 108000
    , boost::asio::use_future
    #endif
    );

    ioc.run();
    // Make sure coroutines have executed
    #if BOOST_VERSION >= 108000
    REQUIRE(future_send.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    REQUIRE(future_recv.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    #endif
}


TEST_CASE("Async Operation Send/Receive single message, check thread safety", "[socket_ops]") {
	boost::asio::io_context ioc;
#if BOOST_VERSION >= 107400
	boost::asio::strand<boost::asio::any_io_executor> strand{ioc.get_executor()};
#else
	boost::asio::strand<boost::asio::executor> strand{ioc.get_executor()};
#endif

	azmq::socket sb(ioc, ZMQ_ROUTER);
	sb.bind(subj(BOOST_CURRENT_FUNCTION));

	azmq::socket sc(ioc, ZMQ_DEALER);
	sc.connect(subj(BOOST_CURRENT_FUNCTION));

	//send coroutine task
	boost::asio::spawn(strand, [&](boost::asio::yield_context yield) {
		REQUIRE(strand.running_in_this_thread());
        boost::system::error_code ecc;
		auto const btc = azmq::async_send(sc, snd_bufs, yield[ecc]);
		REQUIRE(strand.running_in_this_thread());
        REQUIRE(!ecc);
		REQUIRE(btc == 4);
	});

	//receive coroutine task
	boost::asio::spawn(strand, [&](boost::asio::yield_context yield) {
		std::array<char, 5> ident;
		std::array<char, 2> a;
		std::array<char, 2> b;

		std::array<boost::asio::mutable_buffer, 3> rcv_bufs = { {boost::asio::buffer(ident),
						boost::asio::buffer(a),
						boost::asio::buffer(b)}};

		boost::system::error_code ecc;

		REQUIRE(strand.running_in_this_thread());
		auto const btb = azmq::async_receive(sb, rcv_bufs, yield[ecc]);
		REQUIRE(strand.running_in_this_thread());
		REQUIRE(!ecc);
		REQUIRE(btb == 9);

		REQUIRE(message_ref(snd_bufs.at(0)) == boost::string_ref(a.data(), 2));
		REQUIRE(message_ref(snd_bufs.at(1)) == boost::string_ref(b.data(), 2));
	});

	ioc.run();
}

#endif // BOOST_VERSION >= 107000
