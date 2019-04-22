/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_ACTOR_SERVICE_HPP_
#define AZMQ_DETAIL_ACTOR_SERVICE_HPP_

#include "../error.hpp"
#include "../socket.hpp"
#include "../option.hpp"
#include "service_base.hpp"
#include "socket_service.hpp"
#include "config/thread.hpp"
#include "config/mutex.hpp"
#include "config/unique_lock.hpp"
#include "config/condition_variable.hpp"

#include <boost/version.hpp>
#include <boost/assert.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/container/flat_map.hpp>

#if BOOST_VERSION < 10700
#   define AZMQ_DETAIL_USE_IO_SERVICE 1
#endif

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <sstream>
#include <exception>

namespace azmq {
namespace detail {
    class actor_service
        : public azmq::detail::service_base<actor_service> {
    public:
        inline static std::string get_uri(const char* pfx);

#ifdef AZMQ_DETAIL_USE_IO_SERVICE
        actor_service(boost::asio::io_service & ios)
#else
        actor_service(boost::asio::io_context & ios)	  
#endif
            : azmq::detail::service_base<actor_service>(ios)
        { }

        void shutdown_service() override { }

        using is_alive = opt::boolean<static_cast<int>(opt::limits::lib_actor_min)>;
        using detached = opt::boolean<static_cast<int>(opt::limits::lib_actor_min) + 1>;
        using start = opt::boolean<static_cast<int>(opt::limits::lib_actor_min) + 2>;
        using last_error = opt::exception_ptr<static_cast<int>(opt::limits::lib_actor_min) + 3>;

        template<typename T>
        socket make_pipe(bool defer_start, T&& data) {
#ifdef AZMQ_DETAIL_USE_IO_SERVICE
            return make_pipe(get_io_service(), defer_start, std::forward<T>(data));
#else
            return make_pipe(get_io_context(), defer_start, std::forward<T>(data));	    
#endif
        }

        template<typename T>
#ifdef AZMQ_DETAIL_USE_IO_SERVICE
        static socket make_pipe(boost::asio::io_service & ios, bool defer_start, T&& data) {
#else
        static socket make_pipe(boost::asio::io_context & ios, bool defer_start, T&& data) {
#endif	  
            auto p = std::make_shared<model<T>>(std::forward<T>(data));
            auto res = p->peer_socket(ios);
            associate_ext(res, handler(std::move(p), defer_start));
            return std::move(res);
        }

    private:
        struct concept {
            using ptr = std::shared_ptr<concept>;

            boost::asio::io_service io_service_;
            boost::asio::signal_set signals_;
            pair_socket socket_;
            thread_t thread_;

            using lock_type = unique_lock_t<mutex_t>;
            mutable lock_type::mutex_type mutex_;
            mutable condition_variable_t cv_;
            bool ready_;
            bool stopped_;
            std::exception_ptr last_error_;

            concept()
                : signals_(io_service_, SIGINT, SIGTERM)
                , socket_(io_service_)
                , ready_(false)
                , stopped_(true)
            {
                socket_.bind(get_uri("pipe"));
            }

            virtual ~concept() = default;

#ifdef AZMQ_DETAIL_USE_IO_SERVICE
            pair_socket peer_socket(boost::asio::io_service & peer) {
#else	      
            pair_socket peer_socket(boost::asio::io_context & peer) {
#endif
                pair_socket res(peer);
                auto uri = socket_.endpoint();
                BOOST_ASSERT_MSG(!uri.empty(), "uri empty");
                res.connect(uri);
                return res;
            }

            bool joinable() const { return thread_.joinable(); }

            void stop() {
                if (!joinable()) return;
                io_service_.stop();
                thread_.join();
            }

            void stopped() {
                lock_type l{ mutex_ };
                stopped_ = true;
            }

            bool is_stopped() const {
                lock_type l{ mutex_ };
                return stopped_;
            }

            void ready() {
                {
                    lock_type l{ mutex_ };
                    ready_ = true;
                }
                cv_.notify_all();
            }

            void detach() {
                if (!joinable()) return; // already detached
                lock_type l { mutex_ };
                cv_.wait(l, [this] { return ready_; });
                thread_.detach();
            }

            void set_last_error(std::exception_ptr last_error) {
                lock_type l { mutex_ };
                last_error_ = last_error;
            }

            std::exception_ptr last_error() const {
                lock_type l { mutex_ };
                return last_error_;
            }

            virtual void run() = 0;

            static void run(ptr p) {
                lock_type l { p->mutex_ };
                p->signals_.async_wait([p](boost::system::error_code const&, int) {
                    p->io_service_.stop();
                });
                p->stopped_ = false;
                p->thread_ = thread_t([p] {
                    p->ready();
                    try {
                        p->run();
                    } catch (...) {
                        p->set_last_error(std::current_exception());
                    }
                    p->stopped();
                });
            }
        };

        template<typename Function>
        struct model : concept {
            Function data_;

            model(Function data)
                : data_(std::move(data))
            { }

            void run() override { data_(socket_); }
        };

        struct handler {
            concept::ptr p_;
            bool defer_start_;

            handler(concept::ptr p, bool defer_start)
                : p_(std::move(p))
                , defer_start_(defer_start)
            { }

            void on_install(boost::asio::io_service&, void*) {
                if (defer_start_) return;
                defer_start_ = false;
                concept::run(p_);
            }

            void on_remove() {
                if (defer_start_) return;
                p_->stop();
            }

            template<typename Option>
            boost::system::error_code set_option(Option const& opt,
                                                 boost::system::error_code & ec) {
                switch (opt.name()) {
                case is_alive::static_name::value :
                    ec = make_error_code(boost::system::errc::no_protocol_option);
                    break;
                case detached::static_name::value :
                    {
                        if (*static_cast<detached::value_t const*>(opt.data()))
                            p_->detach();
                    }
                    break;
                case start::static_name::value :
                    {
                        if (*static_cast<start::value_t const*>(opt.data()) && defer_start_) {
                            defer_start_ = false;
                            concept::run(p_);
                        }
                    }
                    break;
                case last_error::static_name::value :
                    ec = make_error_code(boost::system::errc::no_protocol_option);
                    break;
                default:
                    ec = make_error_code(boost::system::errc::not_supported);
                    break;
                }
                return ec;
            }

            template<typename Option>
            boost::system::error_code get_option(Option & opt,
                                                 boost::system::error_code & ec) {
                switch (opt.name()) {
                case is_alive::static_name::value :
                    {
                        auto v = static_cast<is_alive::value_t*>(opt.data());
                        *v = !p_->is_stopped();
                    }
                    break;
                case detached::static_name::value :
                    {
                        auto v = static_cast<detached::value_t*>(opt.data());
                        *v = !p_->joinable();
                    }
                    break;
                case start::static_name::value :
                    ec = make_error_code(boost::system::errc::no_protocol_option);
                    break;
                case last_error::static_name::value :
                    {
                        auto v = static_cast<last_error::value_t*>(opt.data());
                        *v = p_->last_error();
                    }
                    break;
                default:
                    ec = make_error_code(boost::system::errc::not_supported);
                    break;
                }
                return ec;
            }
        };
    };

    std::string actor_service::get_uri(const char* pfx) {
        static std::atomic_ulong id{ 0 };
        std::ostringstream stm;
        stm << "inproc://azmq-" << pfx << "-" << id++;
        return stm.str();
    }

} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_ACTOR_SERVICE_HPP_

