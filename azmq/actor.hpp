/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_ACTOR_HPP_
#define AZMQ_ACTOR_HPP_

#include "socket.hpp"
#include "detail/actor_service.hpp"

#include <boost/asio/io_context.hpp>

#include <functional>

namespace azmq { namespace actor {
AZMQ_V1_INLINE_NAMESPACE_BEGIN

    using is_alive = detail::actor_service::is_alive;
    using detached = detail::actor_service::detached;
    using start = detail::actor_service::start;
    using last_error = detail::actor_service::last_error;

    /** \brief create an actor bound to one end of a pipe (pair of inproc sockets)
     *  \param peer io_context to associate the peer (caller) end of the pipe
     *  \param f Function accepting socket& as the first parameter and a
     *           number of additional args
     *  \returns peer socket
     *
     *  \remark The newly created actor will run in a boost::thread, and will
     *  receive the 'server' end of the pipe as it's first argument.  The actor
     *  will be attached to the lifetime of the returned socket and will run
     *  until it is destroyed.
     *
     *  \remark Each actor has an associated io_context and the supplied socket
     *  will be created on this io_context. The actor may access this by calling
     *  get_executor().context() on the supplied socket.
     *
     *  \remark The associated io_context is configured to stop the spawned actor
     *  on SIG_KILL and SIG_TERM.
     *
     *  \remark Termination:
     *      well behaved actors should ultimately call run() on the io_context
     *      associated with the supplied socket. This allows the 'client' end of
     *      the socket's lifetime to cleanly signal termination. If for some
     *      reason, this is not possible, the caller should set the 'detached'
     *      option on the 'client' socket. This detaches the actor's associated
     *      thread from the client socket so that it will not be joined at
     *      destruction time. It is then up to the caller to work out the termination
     *      signal for the background thread; for instance by sending a termination
     *      message.
     *
     *      Also note, the default signal handling for the background thread is
     *      designed to call stop() on the associated io_context, so not calling
     *      run() in your handler means you are responsible for catching these
     *      signals in some other way.
     */
    template<typename Function, typename... Args>
    socket spawn(boost::asio::io_context& peer, bool defer_start, Function && f, Args&&... args) {
        auto& t = boost::asio::use_service<detail::actor_service>(peer);
        return t.make_pipe(defer_start, std::bind(std::forward<Function>(f),
                                                  std::placeholders::_1,
                                                  std::forward<Args>(args)...));
    }

    template<typename Function, typename... Args>
    socket spawn(boost::asio::io_context& peer, Function && f, Args&&... args) {
        auto& t = boost::asio::use_service<detail::actor_service>(peer);
        return t.make_pipe(false, std::bind(std::forward<Function>(f),
                                            std::placeholders::_1,
                                            std::forward<Args>(args)...));
    }

AZMQ_V1_INLINE_NAMESPACE_END
} // namespace actor
} // namespace azmq
#endif // AZMQ_ACTOR_HPP_

