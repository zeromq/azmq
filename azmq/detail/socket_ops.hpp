/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_SOCKET_OPS_HPP__
#define AZMQ_DETAIL_SOCKET_OPS_HPP__

#include "../error.hpp"
#include "../message.hpp"
#include "context_ops.hpp"

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/socket_base.hpp>
#if ! defined BOOST_ASIO_WINDOWS
    #include <boost/asio/posix/stream_descriptor.hpp>
#else
    #include <boost/asio/ip/tcp.hpp>
#endif
#include <boost/system/error_code.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/range/metafunctions.hpp>

#include <zmq.h>

#include <cerrno>
#include <iterator>
#include <memory>
#include <string>
#include <sstream>
#include <type_traits>

namespace azmq {
namespace detail {
    struct socket_ops {
        using endpoint_type = std::string;

        struct socket_close {
            void operator()(void* socket) {
                int v = 0;
                auto rc = zmq_setsockopt(socket, ZMQ_LINGER, &v, sizeof(int));
                BOOST_ASSERT_MSG(rc == 0, "set linger=0 on shutdown"); (void)rc;
                zmq_close(socket);
            }
        };

        enum class dynamic_port : uint16_t {
            first = 0xc000,
            last = 0xffff
        };

        using raw_socket_type = void*;
        using socket_type = std::unique_ptr<void, socket_close>;

#if ! defined BOOST_ASIO_WINDOWS
        using posix_sd_type = boost::asio::posix::stream_descriptor;
        using native_handle_type = boost::asio::posix::stream_descriptor::native_handle_type;
        struct stream_descriptor_close {
            void operator()(posix_sd_type* sd) {
                sd->release();
                delete sd;
            }
        };
        using stream_descriptor = std::unique_ptr<posix_sd_type, stream_descriptor_close>;
#else
        using native_handle_type = boost::asio::ip::tcp::socket::native_handle_type;
        using stream_descriptor = std::unique_ptr<boost::asio::ip::tcp::socket>;
#endif
        using flags_type = message::flags_type;
        using more_result_type = std::pair<size_t, bool>;

        static socket_type create_socket(context_ops::context_type context,
                                         int type,
                                         boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(context, "Invalid context");
            auto res = zmq_socket(context.get(), type);
            if (!res) {
                ec = make_error_code();
                return socket_type();
            }
            return socket_type(res);
        }

        static stream_descriptor get_stream_descriptor(boost::asio::io_service & io_service,
                                                       socket_type & socket,
                                                       boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            native_handle_type handle = 0;
            auto size = sizeof(native_handle_type);
            stream_descriptor res;
            auto rc = zmq_getsockopt(socket.get(), ZMQ_FD, &handle, &size);
            if (rc < 0)
                ec = make_error_code();
            else {
#if ! defined BOOST_ASIO_WINDOWS
                res.reset(new boost::asio::posix::stream_descriptor(io_service, handle));
#else
                // Use duplicated SOCKET, because ASIO socket takes ownership over it so destroys one in dtor.
                ::WSAPROTOCOL_INFO pi;
                ::WSADuplicateSocket(handle, ::GetCurrentProcessId(), &pi);
                handle = ::WSASocket(pi.iAddressFamily/*AF_INET*/, pi.iSocketType/*SOCK_STREAM*/, pi.iProtocol/*IPPROTO_TCP*/, &pi, 0, 0);
                res.reset(new boost::asio::ip::tcp::socket(io_service, boost::asio::ip::tcp::v4(), handle));
#endif
            }
            return res;
        }

        static boost::system::error_code cancel_stream_descriptor(stream_descriptor & sd,
                                                                  boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(sd, "invalid stream_descriptor");
            return sd->cancel(ec);
        }

        static boost::system::error_code bind(socket_type & socket,
                                              endpoint_type & ep,
                                              boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            const boost::regex simple_tcp("^tcp://.*:(\\d+)$");
            const boost::regex dynamic_tcp("^(tcp://.*):([*!])(\\[(\\d+)?-(\\d+)?\\])?$");
            boost::smatch mres;
            int rc = -1;
            if (boost::regex_match(ep, mres, simple_tcp)) {
                if (zmq_bind(socket.get(), ep.c_str()) == 0)
                    rc = boost::lexical_cast<uint16_t>(mres.str(1));
            } else if (boost::regex_match(ep, mres, dynamic_tcp)) {
                auto const& hostname = mres.str(1);
                auto const& opcode = mres.str(2);
                auto const& first_str = mres.str(4);
                auto const& last_str = mres.str(5);
                auto first = first_str.empty() ? static_cast<uint16_t>(dynamic_port::first)
                                               : boost::lexical_cast<uint16_t>(first_str);
                auto last = last_str.empty() ? static_cast<uint16_t>(dynamic_port::last)
                                             : boost::lexical_cast<uint16_t>(last_str);
                uint16_t port = first;
                if (opcode[0] == '!') {
                    static boost::random::mt19937 gen;
                    boost::random::uniform_int_distribution<> port_range(port, last);
                    port = port_range(gen);
                }
                auto attempts = last - first;
                auto fmt = boost::format("%s:%d");
                while (rc < 0 && attempts--) {
                    ep = boost::str(fmt % hostname % port);
                    if (zmq_bind(socket.get(), ep.c_str()) == 0)
                        rc = port;
                    if (++port > last)
                        port = first;
                }
            } else {
                rc = zmq_bind(socket.get(), ep.c_str());
            }
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static boost::system::error_code unbind(socket_type & socket,
                                                endpoint_type const& ep,
                                                boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            auto rc = zmq_unbind(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static boost::system::error_code connect(socket_type & socket,
                                                 endpoint_type const& ep,
                                                 boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            auto rc = zmq_connect(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static boost::system::error_code disconnect(socket_type & socket,
                                                    endpoint_type const& ep,
                                                    boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            auto rc = zmq_disconnect(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        template<typename Option>
        static boost::system::error_code set_option(socket_type & socket,
                                                    Option const& opt,
                                                    boost::system::error_code & ec) {
            auto rc = zmq_setsockopt(socket.get(), opt.name(), opt.data(), opt.size());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        template<typename Option>
        static boost::system::error_code get_option(socket_type & socket,
                                                    Option & opt,
                                                    boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            size_t size = opt.size();
            auto rc = zmq_getsockopt(socket.get(), opt.name(), opt.data(), &size);
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static int get_events(socket_type & socket,
                              boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            int evs = 0;
            size_t size = sizeof(evs);
            for(;;) {
                auto rc = zmq_getsockopt(socket.get(), ZMQ_EVENTS, &evs, &size);
                if (rc < 0) {
                    if (errno == EINTR)
                        continue;
                    ec = make_error_code();
                    return 0;
                }
                break;
            }
            return evs;
        }

        static int get_socket_kind(socket_type & socket,
                                   boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            int kind = 0;
            size_t size = sizeof(kind);
            auto rc = zmq_getsockopt(socket.get(), ZMQ_TYPE, &kind, &size);
            if (rc < 0)
                ec = make_error_code();
            return kind;
        }

        static bool get_socket_rcvmore(socket_type & socket) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            int more = 0;
            size_t size = sizeof(more);
            auto rc = zmq_getsockopt(socket.get(), ZMQ_RCVMORE, &more, &size);
            if (rc == 0)
                return more == 1;
            return false;
        }

        static size_t send(message const& msg,
                           socket_type & socket,
                           flags_type flags,
                           boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "Invalid socket");
            auto rc = zmq_msg_send(const_cast<zmq_msg_t*>(&msg.msg_), socket.get(), flags);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return rc;
        }

        template<typename ConstBufferSequence>
        static auto send(ConstBufferSequence const& buffers,
                         socket_type & socket,
                         flags_type flags,
                         boost::system::error_code & ec) ->
            typename boost::enable_if<boost::has_range_const_iterator<ConstBufferSequence>, size_t>::type
        {
            size_t res = 0;
            auto last = std::distance(std::begin(buffers), std::end(buffers)) - 1;
            auto index = 0u;
            for (auto it = std::begin(buffers); it != std::end(buffers); ++it, ++index) {
                auto f = index == last ? flags
                                       : flags | ZMQ_SNDMORE;
                res += send(message(*it), socket, f, ec);
                if (ec) return 0u;
            }
            return res;
        }

        static size_t receive(message & msg,
                              socket_type & socket,
                              flags_type flags,
                              boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "Invalid socket");
            auto rc = zmq_msg_recv(const_cast<zmq_msg_t*>(&msg.msg_), socket.get(), flags);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return rc;
        }

        template<typename MutableBufferSequence>
        static auto receive(MutableBufferSequence const& buffers,
                            socket_type & socket,
                            flags_type flags,
                            boost::system::error_code & ec) ->
            typename boost::enable_if<boost::has_range_const_iterator<MutableBufferSequence>, size_t>::type
        {
            size_t res = 0;
            message msg;
            auto it = std::begin(buffers);
            do {
                auto sz = receive(msg, socket, flags, ec);
                if (ec)
                    return 0;

                if (msg.buffer_copy(*it++) < sz) {
                    ec = make_error_code(boost::system::errc::no_buffer_space);
                    return 0;
                }

                res += sz;
                flags |= ZMQ_RCVMORE;
            } while ((it != std::end(buffers)) && msg.more());

            if (msg.more())
                ec = make_error_code(boost::system::errc::no_buffer_space);
            return res;
        }

        static size_t receive_more(message_vector & vec,
                                   socket_type & socket,
                                   flags_type flags,
                                   boost::system::error_code & ec) {
            size_t res = 0;
            message msg;
            bool more = false;
            do {
                auto sz = receive(msg, socket, flags, ec);
                if (ec)
                    return 0;
                more = msg.more();
                vec.emplace_back(std::move(msg));
                res += sz;
                flags |= ZMQ_RCVMORE;
            } while (more);
            return res;
        }

        static size_t flush(socket_type & socket,
                            boost::system::error_code & ec) {
            size_t res = 0;
            message msg;
            while (get_socket_rcvmore(socket)) {
                auto sz = receive(msg, socket, ZMQ_RCVMORE, ec);
                if (ec)
                    return 0;
                res += sz;
            };
            return res;
        }

        static std::string monitor(socket_type & socket,
                                   int events,
                                   boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "Invalid socket");
            std::ostringstream stm;
            stm << "inproc://monitor-" << socket.get();
            auto addr = stm.str();
            auto rc = zmq_socket_monitor(socket.get(), addr.c_str(), events);
            if (rc < 0)
                ec = make_error_code();
            return addr;
        }
    };
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_SOCKET_OPS_HPP__

