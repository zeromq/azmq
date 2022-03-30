/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_SOCKET_HPP_
#define AZMQ_SOCKET_HPP_

#include "error.hpp"
#include "option.hpp"
#include "context.hpp"
#include "message.hpp"
#include "detail/basic_io_object.hpp"
#include "detail/send_op.hpp"
#include "detail/receive_op.hpp"

#include <boost/asio/basic_io_object.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/system/error_code.hpp>

#include <type_traits>
#include <utility>

namespace azmq {
AZMQ_V1_INLINE_NAMESPACE_BEGIN

/** \brief Implement an asio-like socket over a zeromq socket
 *  \remark sockets are movable, but not copyable
 */
class socket :
    public azmq::detail::basic_io_object<detail::socket_service> {

public:
    using native_handle_type = detail::socket_service::native_handle_type;
    using endpoint_type = detail::socket_service::endpoint_type;
    using flags_type = detail::socket_service::flags_type;
    using more_result_type = detail::socket_service::more_result_type;
    using shutdown_type = detail::socket_service::shutdown_type;

    // socket options
    using allow_speculative = detail::socket_service::allow_speculative;
    using type = opt::integer<ZMQ_TYPE>;
    using rcv_more = opt::integer<ZMQ_RCVMORE>;
    using rcv_hwm = opt::integer<ZMQ_RCVHWM>;
    using snd_hwm = opt::integer<ZMQ_SNDHWM>;
    using affinity = opt::ulong_integer<ZMQ_AFFINITY>;
    using subscribe = opt::binary<ZMQ_SUBSCRIBE>;
    using unsubscribe = opt::binary<ZMQ_UNSUBSCRIBE>;
    using identity = opt::binary<ZMQ_IDENTITY>;
    using rate = opt::integer<ZMQ_RATE>;
    using recovery_ivl = opt::integer<ZMQ_RECOVERY_IVL>;
    using snd_buf = opt::integer<ZMQ_SNDBUF>;
    using rcv_buf = opt::integer<ZMQ_RCVBUF>;
    using linger = opt::integer<ZMQ_LINGER>;
    using reconnect_ivl = opt::integer<ZMQ_RECONNECT_IVL>;
    using reconnect_ivl_max = opt::integer<ZMQ_RECONNECT_IVL_MAX>;
    using backlog = opt::integer<ZMQ_BACKLOG>;
    using max_msgsize = opt::integer<ZMQ_MAXMSGSIZE>;
    using multicast_hops = opt::integer<ZMQ_MULTICAST_HOPS>;
    using rcv_timeo = opt::integer<ZMQ_RCVTIMEO>;
    using snd_timeo = opt::integer<ZMQ_SNDTIMEO>;
    using ipv6 = opt::boolean<ZMQ_IPV6>;
    using immediate = opt::boolean<ZMQ_IMMEDIATE>;
    using router_mandatory = opt::boolean<ZMQ_ROUTER_MANDATORY>;
    using router_raw = opt::boolean<ZMQ_ROUTER_RAW>;
    using router_handover = opt::boolean<ZMQ_ROUTER_HANDOVER>;
    using probe_router = opt::boolean<ZMQ_PROBE_ROUTER>;
    using xpub_verbose = opt::boolean<ZMQ_XPUB_VERBOSE>;
    using xpub_nodrop = opt::integer<ZMQ_XPUB_NODROP>;
    using req_correlate = opt::boolean<ZMQ_REQ_CORRELATE>;
    using req_relaxed = opt::boolean<ZMQ_REQ_RELAXED>;
    using last_endpoint = opt::binary<ZMQ_LAST_ENDPOINT>;
    using tcp_keepalive = opt::integer<ZMQ_TCP_KEEPALIVE>;
    using tcp_keepalive_idle = opt::integer<ZMQ_TCP_KEEPALIVE_IDLE>;
    using tcp_keepalive_cnt = opt::integer<ZMQ_TCP_KEEPALIVE_CNT>;
    using tcp_keepalive_intvl = opt::integer<ZMQ_TCP_KEEPALIVE_INTVL>;
    using tcp_accept_filter = opt::binary<ZMQ_TCP_ACCEPT_FILTER>;
    using plain_server = opt::integer<ZMQ_PLAIN_SERVER>;
    using plain_username = opt::binary<ZMQ_PLAIN_USERNAME>;
    using plain_password = opt::binary<ZMQ_PLAIN_PASSWORD>;
    using curve_server = opt::boolean<ZMQ_CURVE_SERVER>;
    using curve_publickey = opt::binary<ZMQ_CURVE_PUBLICKEY>;
    using curve_privatekey = opt::binary<ZMQ_CURVE_SECRETKEY>;
    using curve_serverkey = opt::binary<ZMQ_CURVE_SERVERKEY>;
    using zap_domain = opt::binary<ZMQ_ZAP_DOMAIN>;
    using conflate = opt::boolean<ZMQ_CONFLATE>;

    /** \brief socket constructor
     *  \param ios reference to an asio::io_service
     *  \param s_type int socket type
     *      For socket types see the zeromq documentation
     *  \param optimize_single_threaded bool
     *      Defaults to false - socket is not optimized for a single
     *      threaded io_service
     *  \remarks
     *      ZeroMQ's socket types are not thread safe. Because there is no
     *      guarantee that the supplied io_service is running in a single
     *      thread, Aziomq by default wraps all calls to ZeroMQ APIs with
     *      a mutex. If you can guarantee that a single thread has called
     *      io_service.run() you may bypass the mutex by passing true for
     *      optimize_single_threaded.
     */
    explicit socket(boost::asio::io_service& ios,
                    int type,
                    bool optimize_single_threaded = false)
            : azmq::detail::basic_io_object<detail::socket_service>(ios) {
        boost::system::error_code ec;
        if (get_service().do_open(get_implementation(), type, optimize_single_threaded, ec))
            throw boost::system::system_error(ec);
    }

    socket(socket&& other)
        : azmq::detail::basic_io_object<detail::socket_service>(other.get_io_service()) {
        get_service().move_construct(get_implementation(),
                                     other.get_service(),
                                     other.get_implementation());
    }

    socket& operator=(socket&& rhs) {
        get_service().move_assign(get_implementation(),
                                  rhs.get_service(),
                                  rhs.get_implementation());
        return *this;
    }

    socket(const socket &) = delete;
    socket & operator=(const socket &) = delete;

    /** \brief Accept incoming connections on this socket
     *  \param addr std::string zeromq URI to bind
     *  \param ec error_code to capture error
     *  \see http://api.zeromq.org/4-1:zmq-bind
     *  \remarks
     *  For TCP endpoints, supports binding to IANA defined
     *  ephemeral ports. The default range is 49152-65535.
     *  To override this range, follow the '*' with "[first-last]".
     *  To bind to a random port, follow the '!' with "[first-last]".
     *
     *  Examples:
     *
     *  tcp://127.0.0.1:*                bind to first free port from 49152 up
     *  tcp://126.0.0.1:!                bind to random port from 49152 to 65535
     *  tcp://127.0.0.1:*[60000-]        bind to first free port from 60000 up
     *  tcp://127.0.0.1:![-60000]        bind to random port from 49152 to 60000
     *  tcp://127.0.0.1:![55000-55999]   bind to random port from 55000-55999
     *
     *  endpoint() will return the actual endpoint bound, not the binding spec
     *  supplied here.
     */
    boost::system::error_code bind(std::string addr,
                                   boost::system::error_code & ec) {
        return get_service().bind(get_implementation(), std::move(addr), ec);
    }

    /** \brief Accept incoming connections on this socket
     *  \param addr std::string zeromq URI to bind
     *  \throw boost::system::system_error
     *  \see http://api.zeromq.org/4-1:zmq-bind
     */
    void bind(std::string addr) {
        boost::system::error_code ec;
        if (bind(std::move(addr), ec))
            throw boost::system::system_error(ec);
    }

    /** \brief Stop accepting connection on this socket
     *  \param addr std::string const& zeromq URI to unbind
     *  \param ec error_code to capture error
     *  \see http://api.zeromq.org/4-0:zmq-unbind
     */
    boost::system::error_code unbind(std::string const& addr,
                                     boost::system::error_code & ec) {
        return get_service().unbind(get_implementation(), addr, ec);
    }

    /** \brief Stop accepting connection on this socket
     *  \param addr std::string const& zeromq URI to unbind
     *  \throw boost::system::system_error
     *  \see http://api.zeromq.org/4-0:zmq-unbind
     */
    void unbind(std::string const& addr) {
        boost::system::error_code ec;
        if (unbind(addr, ec))
            throw boost::system::system_error(ec);
    }

    /** \brief Create outgoing connection from this socket
     *  \param addr std::string zeromq URI of endpoint
     *  \param ec error_code to capture error
     *  \see http://api.zeromq.org/4-1:zmq-connect
     */
    boost::system::error_code connect(std::string addr,
                                      boost::system::error_code & ec) {
        return get_service().connect(get_implementation(), std::move(addr), ec);
    }

    /** \brief Create outgoing connection from this socket
     *  \param addr std::string zeromq URI of endpoint
     *  \throw boost::system::system_error
     *  \see http://api.zeromq.org/4-1:zmq-connect
     */
    void connect(std::string addr) {
        boost::system::error_code ec;
        if (connect(addr, ec))
            throw boost::system::system_error(ec);
    }

    /** \brief Disconnect this socket
     *  \param addr std::string const& zeromq URI of endpoint
     *  \param ec error_code to capture error
     *  \see http://api.zeromq.org/4-1:zmq-disconnect
     */
    boost::system::error_code disconnect(std::string const& addr,
                                         boost::system::error_code & ec) {
        return get_service().disconnect(get_implementation(), addr, ec);
    }

    /** \brief Disconnect this socket
     *  \param addr std::string const& zeromq URI of endpoint
     *  \throw boost::system::system_error
     *  \see http://api.zeromq.org/4-1:zmq-disconnect
     */
    void disconnect(std::string const& addr) {
        boost::system::error_code ec;
        if (disconnect(addr, ec))
            throw boost::system::system_error(ec);
    }

    /** \brief return endpoint addr supplied to bind or connect
     *  \returns std::string
     *  \remarks Return value will be empty if bind or connect has
     *  not yet been called/succeeded.  If multiple calls to connect
     *  or bind have occured, this call wil return only the most recent
     */
    endpoint_type endpoint() const {
        return get_service().endpoint(get_implementation());
    }

    /** \brief Set an option on a socket
     *  \tparam Option type which must conform the asio SettableSocketOption concept
     *  \param ec error_code to capture error
     *  \param opt T option to set
     */
    template<typename Option>
    boost::system::error_code set_option(Option const& opt,
                                         boost::system::error_code & ec) {
        return get_service().set_option(get_implementation(), opt, ec);
    }

    /** \brief Set an option on a socket
     *  \tparam T type which must conform the asio SettableSocketOption concept
     *  \param opt T option to set
     *  \throw boost::system::system_error
     */
    template<typename Option>
    void set_option(Option const& opt) {
        boost::system::error_code ec;
        if (set_option(opt, ec))
            throw boost::system::system_error(ec);
    }

    /** \brief Get an option from a socket
     *  \tparam T must conform to the asio GettableSocketOption concept
     *  \param opt T option to get
     *  \param ec error_code to capture error
     */
    template<typename Option>
    boost::system::error_code get_option(Option & opt,
                                         boost::system::error_code & ec) {
        return get_service().get_option(get_implementation(), opt, ec);
    }

    /** \brief Get an option from a socket
     *  \tparam T must conform to the asio GettableSocketOption concept
     *  \param opt T option to get
     *  \throw boost::system::system_error
     */
    template<typename Option>
    void get_option(Option & opt) {
        boost::system::error_code ec;
        if (get_option(opt, ec))
            throw boost::system::system_error(ec);
    }

    /** \brief Receive some data from the socket
     *  \tparam MutableBufferSequence
     *  \param buffers buffer(s) to fill on receive
     *  \param flags specifying how the receive call is to be made
     *  \param ec set to indicate what error, if any, occurred
     *  \remark
     *  If buffers is a sequence of buffers this call will fill the supplied
     *  sequence with message parts from a multipart message. It is possible
     *  that there are more message parts than supplied buffers, or that an
     *  individual message part's size may exceed an individual buffer in the
     *  sequence. In either case, the call will return with ec set to
     *  no_buffer_space. The caller may distinguish between the case where
     *  there were simply an insufficient number of buffers to collect all
     *  message parts and the case where an individual buffer was insufficiently
     *  sized by examining the returned size. In the case where an insufficient
     *  number of buffers were presented, the size will be the number of bytes
     *  received so far. In the case where an individual buffer was too small,
     *  the returned size will be zero. It is the callers responsibility to issue
     *  additional receive calls to collect the remaining message parts or
     *  flush to discard them.
     */
    template<typename MutableBufferSequence>
    std::size_t receive(MutableBufferSequence const& buffers,
                        flags_type flags,
                        boost::system::error_code & ec) {
        return get_service().receive(get_implementation(), buffers, flags, ec);
    }

    /** \brief Receive some data from the socket
     *  \tparam MutableBufferSequence
     *  \param buffers buffer(s) to fill on receive
     *  \param flags flags specifying how the receive call is to be made
     *  \throw boost::system::system_error
     *  \remark
     *  If buffers is a sequence of buffers this call will fill the supplied
     *  sequence with message parts from a multipart message. It is possible
     *  that there are more message parts than supplied buffers, or that an
     *  individual message part's size may exceed an individual buffer in the
     *  sequence. In either case, the call will return with ec set to
     *  no_buffer_space. The caller may distinguish between the case where
     *  there were simply an insufficient number of buffers to collect all
     *  message parts and the case where an individual buffer was insufficiently
     *  sized by examining the returned size. In the case where an insufficient
     *  number of buffers were presented, the size will be the number of bytes
     *  received so far. In the case where an individual buffer was too small,
     *  the returned size will be zero. It is the callers responsibility to issue
     *  additional receive calls to collect the remaining message parts or
     *  flush to discard them.
     */
    template<typename MutableBufferSequence>
    std::size_t receive(const MutableBufferSequence & buffers,
                        flags_type flags = 0) {
        boost::system::error_code ec;
        auto res = receive(buffers, flags, ec);
        if (ec)
            throw boost::system::system_error(ec);
        return res;
    }

    /** \brief Receive some data from the socket
     *  \param msg raw_message to fill on receive
     *  \param flags specifying how the receive call is to be made
     *  \returns byte's received
     *  \remarks
     *      This variant provides access to a type that thinly wraps the underlying
     *      libzmq message type.  The rebuild_message flag indicates whether the
     *      message provided should be closed and rebuilt.  This is useful when
     *      reusing the same message instance across multiple receive operations.
     */
    std::size_t receive(message & msg,
                        flags_type flags,
                        boost::system::error_code & ec) {
        return get_service().receive(get_implementation(), msg, flags, ec);
    }

    /** \brief Receive some data from the socket
     *  \param msg message to fill on receive
     *  \param flags specifying how the receive call is to be made
     *  \param ec set to indicate what error, if any, occurred
     *  \param rebuild_message bool
     *  \remarks
     *      This variant provides access to a type that thinly wraps the underlying
     *      libzmq message type.  The rebuild_message flag indicates whether the
     *      message provided should be closed and rebuilt.  This is useful when
     *      reusing the same message instance across multiple receive operations.
     */
    std::size_t receive(message & msg,
                        flags_type flags = 0) {
        boost::system::error_code ec;
        auto res = receive(msg, flags, ec);
        if (ec)
            throw boost::system::system_error(ec);
        return res;
    }

    /** \brief Receive all parts of a multipart message from the socket
     *  \param vec message_vector to fill on receive
     *  \flags specifying how the receive call is to be made
     *  \param ec set to indicate what error, if any, occurred
     *  \return size_t bytes transferred
     */
    size_t receive_more(message_vector & vec,
                        flags_type flags,
                        boost::system::error_code & ec) {
        return get_service().receive_more(get_implementation(), vec, flags, ec);
    }

    /** \brief Receive all parts of a multipart message from the socket
     *  \param vec messave_vector to fill on receive
     *  \flags specifying how the receive call is to be made
     *  \return size_t bytes transferred
     *  \throw boost::system::system_error
     */
    size_t receive_more(message_vector & vec,
                        flags_type flags) {
        boost::system::error_code ec;
        auto res = receive_more(vec, flags, ec);
        if (ec)
            throw boost::system::system_error(ec);
        return res;
    }

    /** \brief Send some data from the socket
     *  \tparam ConstBufferSequence
     *  \param buffers buffer(s) to send
     *  \param flags specifying how the send call is to be made
     *  \param ec set to indicate what, if any, error occurred
     *  \remark
     *  If buffers is a sequence of buffers this call will send a multipart
     *  message from the supplied buffer sequence.
     */
    template<typename ConstBufferSequence>
    std::size_t send(ConstBufferSequence const& buffers,
                     flags_type flags,
                     boost::system::error_code & ec) {
        return get_service().send(get_implementation(), buffers, flags, ec);
    }

    /** \brief Send some data to the socket
     *  \tparam ConstBufferSequence
     *  \param buffers buffer(s) to send
     *  \param flags specifying how the send call is to be made
     *  \throw boost::system::system_error
     *  \remark
     *  If buffers is a sequence of buffers this call will send a multipart
     *  message from the supplied buffer sequence.
     */
    template<typename ConstBufferSequence>
    std::size_t send(ConstBufferSequence const& buffers,
                     flags_type flags = 0) {
        boost::system::error_code ec;
        auto res = send(buffers, flags, ec);
        if (ec)
            throw boost::system::system_error(ec);
        return res;
    }

    /** \brief Send some data from the socket
     *  \param msg raw_message to send
     *  \param flags specifying how the send call is to be made
     *  \param ec set to indicate what, if any, error occurred
     */
    std::size_t send(message msg,
                     flags_type flags,
                     boost::system::error_code & ec) {
        return get_service().send(get_implementation(), std::move(msg), flags, ec);
    }

    /** \brief Send some data from the socket
     *  \param msg raw_message to send
     *  \param flags specifying how the send call is to be made
     *  \return bytes transferred
     */
    std::size_t send(message msg,
                     flags_type flags = 0) {
        boost::system::error_code ec;
        auto res = get_service().send(get_implementation(), std::move(msg), flags, ec);
        if (ec)
            throw boost::system::system_error(ec);
        return res;
    }

    /* \brief Purge remaining message parts from prior receive()
     * \param ec boost::system::error_code &
     * \return size_t number of bytes discarded
     */
    std::size_t flush(boost::system::error_code & ec) {
        return get_service().flush(get_implementation(), ec);
    }

    /* \brief Flush remaining message parts from prior receive()
     * \return size_t number of bytes discarded
     * \throw boost::system::system_error
     */
    std::size_t flush() {
        boost::system::error_code ec;
        auto res = flush(ec);
        if (ec)
            throw boost::system::system_error(ec);
        return res;
    }

    /** \brief Initiate an async receive operation.
     *  \tparam MutableBufferSequence
     *  \tparam ReadHandler must conform to the asio ReadHandler concept
     *  \param buffers buffer(s) to fill on receive
     *  \param handler ReadHandler
     *  \remark
     *  If buffers is a sequence of buffers, and flags has ZMQ_RCVMORE
     *  set, this call will fill the supplied sequence with message
     *  parts from a multipart message. It is possible that there are
     *  more message parts than supplied buffers, or that an individual
     *  message part's size may exceed an individual buffer in the
     *  sequence. In either case, the handler will be called with ec set
     *  to no_buffer_space. It is the callers responsibility to issue
     *  additional receive calls to collect the remaining message parts or
     *  call flush to discard them.
     */
    template<typename MutableBufferSequence,
             typename ReadHandler>
    void async_receive(MutableBufferSequence const& buffers,
                       ReadHandler && handler,
                       flags_type flags = 0) {
        using type = detail::receive_buffer_op<MutableBufferSequence, ReadHandler>;
        get_service().enqueue<type>(get_implementation(), detail::socket_service::op_type::read_op,
                                    buffers, std::forward<ReadHandler>(handler), flags);
    }

    /** \brief Initiate an async receive operation.
     *  \tparam MutableBufferSequence
     *  \tparam ReadMoreHandler must conform to the ReadMoreHandler concept
     *  \param buffers buffer(s) to fill on receive
     *  \param handler ReadMoreHandler
     *  \remark
     *  The ReadMoreHandler concept has the following interface
     *      struct ReadMoreHandler {
     *          void operator()(const boost::system::error_code & ec,
     *                          more_result result);
     *      }
     *  \remark
     *  Works as for async_receive() but does not error if more parts remain
     *  than buffers supplied.  The completion handler will be called with
     *  a more_result indicating the number of bytes transferred thus far,
     *  and flag indicating whether more message parts remain. The handler
     *  may then make synchronous receive_more() calls to collect the remaining
     *  message parts.
     */
    template<typename MutableBufferSequence,
             typename ReadMoreHandler>
    void async_receive_more(MutableBufferSequence const& buffers,
                            ReadMoreHandler && handler,
                            flags_type flags = 0) {
        using type = detail::receive_more_buffer_op<MutableBufferSequence, ReadMoreHandler>;
        get_service().enqueue<type>(get_implementation(), detail::socket_service::op_type::read_op,
                                    buffers, std::forward<ReadMoreHandler>(handler), flags);
    }

    /** \brief Initate an async receive operation
     *  \tparam MessageReadHandler must conform to the MessageReadHandler concept
     *  \param handler ReadHandler
     *  \param flags int flags
     *  \remark
     *  The MessageReadHandler concept has the following interface
     *  struct MessageReadHandler {
     *      void operator()(const boost::system::error_code & ec,
     *                      message & msg,
     *                      size_t bytes_transferred);
     *  }
     *  \remark
     *  Multipart messages can be handled by checking the status of more() on the
     *  supplied message, and calling synchronous receive() to retrieve subsequent
     *  message parts. If a handler wishes to retain the supplied message after the
     *  MessageReadHandler returns, it must make an explicit copy or move of
     *  the message.
     */
    template<typename MessageReadHandler>
    void async_receive(MessageReadHandler && handler,
                       flags_type flags = 0) {
        using type = detail::receive_op<MessageReadHandler>;
        get_service().enqueue<type>(get_implementation(), detail::socket_service::op_type::read_op,
                                    std::forward<MessageReadHandler>(handler), flags);
    }

    /** \brief Initiate an async send operation
     *  \tparam ConstBufferSequence must conform to the asio
     *          ConstBufferSequence concept
     *  \tparam WriteHandler must conform to the asio
     *          WriteHandler concept
     *  \param flags specifying how the send call is to be made
     *  \remark
     *  If buffers is a sequence of buffers, this call will send a multipart
     *  message from the supplied buffer sequence.
     */
    template<typename ConstBufferSequence,
             typename WriteHandler>
    void async_send(ConstBufferSequence const& buffers,
                    WriteHandler && handler,
                    flags_type flags = 0) {
        using type = detail::send_buffer_op<ConstBufferSequence, WriteHandler>;
        get_service().enqueue<type>(get_implementation(), detail::socket_service::op_type::write_op,
                                    buffers, std::forward<WriteHandler>(handler), flags);
    }

    /** \brief Initate an async send operation
     *  \tparam WriteHandler must conform to the asio WriteHandler concept
     *  \param msg message reference
     *  \param handler WriteHandler
     *  \param flags int flags
     */
    template<typename WriteHandler>
    void async_send(message const& msg,
                    WriteHandler && handler,
                    flags_type flags = 0) {
        using type = detail::send_op<WriteHandler>;
        get_service().enqueue<type>(get_implementation(), detail::socket_service::op_type::write_op,
                                    msg, std::forward<WriteHandler>(handler), flags);
    }

    /** \brief Initiate shutdown of socket
     *  \param what shutdown_type
     *  \param ec set to indicate what, if any, error occurred
     */
    boost::system::error_code shutdown(shutdown_type what,
                                       boost::system::error_code & ec) {
        return get_service().descriptor_shutdown(get_implementation(), what, ec);
    }

    /** \brief Initiate shutdown of socket
     *  \param what shutdown_type
     *  \throw boost::system::system_error
     */
    void shutdown(shutdown_type what) {
        boost::system::error_code ec;
        if (shutdown(what, ec))
            throw boost::system::system_error(ec);
    }

    /** \brief Cancel all outstanding asynchronous operations
     *  \param ec set to indicate what, if any, error occurred
     */
    boost::system::error_code cancel(boost::system::error_code & ec) {
        return get_service().cancel(get_implementation(), ec);
    }

    /** \brief Cancel all outstanding asynchronous operations
     *  \throw boost::system::system_error
     */
    void cancel() {
        boost::system::error_code ec;
        if (get_service().cancel(get_implementation(), ec))
            throw boost::system::system_error(ec);
    }

    /** \brief Allows access to the underlying ZeroMQ socket
     *  \remark With great power, comes great responsibility
     */
    native_handle_type native_handle() {
        return get_service().native_handle(get_implementation());
    }

    /** \brief monitor events on a socket
        *  \param ios io_service on which to bind the returned monitor socket
        *  \param events int mask of events to publish to returned socket
        *  \param ec error_code to set on error
        *  \returns socket
    **/
    socket monitor(boost::asio::io_service & ios,
                   int events,
                   boost::system::error_code & ec) {
        auto uri = get_service().monitor(get_implementation(), events, ec);
        socket res(ios, ZMQ_PAIR);
        if (ec)
            return res;

        if (res.connect(uri, ec))
            return res;
        return res;
    }

    /** \brief monitor events on a socket
        *  \param ios io_service on which to bind the returned monitor socket
        *  \param events int mask of events to publish to returned socket
        *  \returns socket
    **/
    socket monitor(boost::asio::io_service & ios,
                   int events) {
        boost::system::error_code ec;
        auto res = monitor(ios, events, ec);
        if (ec)
            throw boost::system::system_error(ec);
        return res;
    }

    friend std::ostream& operator<<(std::ostream& stm, const socket& that) {
        auto& s = const_cast<socket&>(that);
        s.get_service().format(s.get_implementation(), stm);
        return stm;
    }
};
AZMQ_V1_INLINE_NAMESPACE_END

namespace detail {
    template<int Type>
    class specialized_socket: public socket
    {
        typedef socket Base;

    public:
        specialized_socket(boost::asio::io_service & ios,
                           bool optimize_single_threaded = false)
            : Base(ios, Type, optimize_single_threaded)
        {
            // Note that we expect these to get sliced to socket, so DO NOT add any data members
            static_assert(sizeof(*this) == sizeof(socket), "Specialized socket must not have any specific data members");
        }

        specialized_socket(specialized_socket&& op)
            : Base(std::move(op))
        {}

        specialized_socket& operator= (specialized_socket&& rhs)
        {
            Base::operator=(std::move(rhs));
            return *this;
        }
    };
}

AZMQ_V1_INLINE_NAMESPACE_BEGIN
using pair_socket = detail::specialized_socket<ZMQ_PAIR>;
using req_socket = detail::specialized_socket<ZMQ_REQ>;
using rep_socket = detail::specialized_socket<ZMQ_REP>;
using dealer_socket = detail::specialized_socket<ZMQ_DEALER>;
using router_socket = detail::specialized_socket<ZMQ_ROUTER>;
using pub_socket = detail::specialized_socket<ZMQ_PUB>;
using sub_socket = detail::specialized_socket<ZMQ_SUB>;
using xpub_socket = detail::specialized_socket<ZMQ_XPUB>;
using xsub_socket = detail::specialized_socket<ZMQ_XSUB>;
using push_socket = detail::specialized_socket<ZMQ_PUSH>;
using pull_socket = detail::specialized_socket<ZMQ_PULL>;
using stream_socket = detail::specialized_socket<ZMQ_STREAM>;

/** \brief attach a socket to a range of endpoints
 *  \tparam Iterator iterator to a sequence of endpoints
 *  \param s socket& to attach to supplied endpoints
 *  \param begin of endpoint sequence
 *  \param end of endpoint sequence
 *  \param ec boost::system::error_code&
 *  \param serverish bool (defaults to true) - See remarks
 *  \return boost::system::error_code
 *  \remarks
 *  If each endpoint is prefixed with '@' or '>', then this
 *  routine will bind or connect the socket (respectively) to
 *  the endpoint. If the endpoint does not start with '@' or '>'
 *  then the value of serverish determines whether the socket
 *  will bound (the default) or connected (serverish == false).
 */
template<typename Iterator>
boost::system::error_code attach(socket & s, Iterator begin, Iterator end,
                                 boost::system::error_code & ec,
                                 bool serverish = true) {
    for (auto it = begin; it != end; ++it) {
        if (it->empty()) continue;
        if (it->at(0) == '@') {
            if (s.bind(it->substr(1), ec))
                return ec;
        } else if (it->at(0) == '>') {
            if (s.connect(it->substr(1), ec))
                return ec;
        } else {
            if (serverish)
                s.bind(*it, ec);
            else
                s.connect(*it, ec);
            if (ec)
                return ec;
        }
    }
    return ec;
}

/** \brief attach a socket to a range of endpoints
 *  \tparam Iterator iterator to a sequence of endpoints
 *  \param s socket& to attach to supplied endpoints
 *  \param begin of endpoint sequence
 *  \param end of endpoint sequence
 *  \param serverish bool (defaults to true) - See remarks
 *  \throw boost::system::system_error
 *  \remarks
 *  If each endpoint is prefixed with '@' or '>', then this
 *  routine will bind or connect the socket (respectively) to
 *  the endpoint. If the endpoint does not start with '@' or '>'
 *  then the value of serverish determines whether the socket
 *  will bound (the default) or connected (serverish == false).
 */
template<typename Iterator>
void attach(socket & s, Iterator begin, Iterator end, bool serverish = true) {
    boost::system::error_code ec;
    if (attach(s, begin, end, ec, serverish))
        throw boost::system::system_error(ec);
}

/** \brief attach a socket to a range of endpoints
 *  \tparam Range a type implementing begin() and end()
 *          for a sequence of endpoints
 *  \param s socket& to attach to supplied endpoints
 *  \param r Range of endpoints
 *  \param ec boost::system::error_code&
 *  \param serverish bool (defaults to true) - See remarks
 *  \return boost::system::error_code
 *  \remarks
 *  If each endpoint is prefixed with '@' or '>', then this
 *  routine will bind or connect the socket (respectively) to
 *  the endpoint. If the endpoint does not start with '@' or '>'
 *  then the value of serverish determines whether the socket
 *  will bound (the default) or connected (serverish == false).
 */
template<typename Range>
boost::system::error_code attach(socket & s, Range r,
                                 boost::system::error_code & ec,
                                 bool serverish = true) {
    return attach(s, std::begin(r), std::end(r), ec, serverish);
}

/** \brief attach a socket to a range of endpoints
 *  \tparam Range a type implementing begin() and end()
 *          for a sequence of endpoints
 *  \param s socket& to attach to supplied endpoints
 *  \param r Range of endpoints
 *  \param serverish bool (defaults to true) - See remarks
 *  \throw boost::system::system_error
 *  \remarks
 *  If each endpoint is prefixed with '@' or '>', then this
 *  routine will bind or connect the socket (respectively) to
 *  the endpoint. If the endpoint does not start with '@' or '>'
 *  then the value of serverish determines whether the socket
 *  will bound (the default) or connected (serverish == false).
 */
template<typename Range>
void attach(socket & s, Range r, bool serverish = true) {
    boost::system::error_code ec;
    if (attach(s, std::begin(r), std::end(r), ec, serverish))
        throw boost::system::system_error(ec);
}
AZMQ_V1_INLINE_NAMESPACE_END
} // namespace azmq
#endif // AZMQ_SOCKET_HPP_

