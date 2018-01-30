/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_SOCKET_SERVICE_HPP__
#define AZMQ_DETAIL_SOCKET_SERVICE_HPP__
#include "../error.hpp"
#include "../message.hpp"
#include "../option.hpp"
#include "../util/scope_guard.hpp"
#include "config/mutex.hpp"
#include "config/lock_guard.hpp"

#include "basic_io_object.hpp"
#include "service_base.hpp"
#include "context_ops.hpp"
#include "socket_ops.hpp"
#include "socket_ext.hpp"
#include "reactor_op.hpp"
#include "send_op.hpp"
#include "receive_op.hpp"

#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/system/system_error.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#include <memory>
#include <typeindex>
#include <string>
#include <vector>
#include <tuple>
#include <ostream>

namespace azmq {
namespace detail {
    class socket_service
        : public azmq::detail::service_base<socket_service> {
    public:
        using socket_type = socket_ops::socket_type;
        using native_handle_type= socket_ops::raw_socket_type;
        using stream_descriptor = socket_ops::stream_descriptor;
        using endpoint_type = socket_ops::endpoint_type;
        using flags_type = socket_ops::flags_type;
        using more_result_type = socket_ops::more_result_type;
        using context_type = context_ops::context_type;
        using op_queue_type = boost::intrusive::list<reactor_op,
                                    boost::intrusive::member_hook<
                                        reactor_op,
                                        boost::intrusive::list_member_hook<>,
                                        &reactor_op::member_hook_
                                    >>;
        using exts_type = boost::container::flat_map<std::type_index, socket_ext>;
        using allow_speculative = opt::boolean<static_cast<int>(opt::limits::lib_socket_min)>;

        enum class shutdown_type {
            none = 0,
            send,
            receive
        };

        enum op_type : unsigned {
            read_op = 0,
            write_op = 1,
            max_ops = 2
        };

        struct per_descriptor_data {
            bool optimize_single_threaded_ = false;
            socket_type socket_;
            stream_descriptor sd_;
            mutable boost::mutex mutex_;
            bool in_speculative_completion_ = false;
            bool scheduled_ = false;
            bool missed_events_found_ = false;
            bool allow_speculative_ = true;
            shutdown_type shutdown_ = shutdown_type::none;
            exts_type exts_;
            endpoint_type endpoint_;
            bool serverish_ = false;
            std::array<op_queue_type, max_ops> op_queue_;

            void do_open(boost::asio::io_service & ios,
                         context_type & ctx,
                         int type,
                         bool optimize_single_threaded,
                         boost::system::error_code & ec) {
                BOOST_ASSERT_MSG(!socket_, "socket already open");
                socket_ = socket_ops::create_socket(ctx, type, ec);
                if (ec) return;

                sd_ = socket_ops::get_stream_descriptor(ios, socket_, ec);
                if (ec) return;

                optimize_single_threaded_ = optimize_single_threaded;
            }

            int events_mask() const
            {
                static_assert(2 == max_ops, "2 == max_ops");
                return (!op_queue_[read_op].empty() ? ZMQ_POLLIN : 0)
                     | (!op_queue_[write_op].empty() ? ZMQ_POLLOUT : 0);
            }

            bool perform_ops(op_queue_type & ops, boost::system::error_code& ec) {
                const int filter[max_ops] = { ZMQ_POLLIN, ZMQ_POLLOUT };
                while(int evs = socket_ops::get_events(socket_, ec)& events_mask()) {
                    if (ec)
                        break;

                    for (size_t i = 0; i != max_ops; ++i) {
                        if ((evs & filter[i]) && op_queue_[i].front().do_perform(socket_)) {
                            op_queue_[i].pop_front_and_dispose([&ops](reactor_op * op) {
                                ops.push_back(*op);
                            });
                        }
                    }
                }

                return 0 != events_mask(); // true if more operations scheduled
            }

            boost::system::error_code cancel_stream_descriptor(boost::system::error_code & ec) {
                return socket_ops::cancel_stream_descriptor(sd_, ec);
            }

            void cancel_ops(boost::system::error_code const& ec, op_queue_type & ops) {
                for (size_t i = 0; i != max_ops; ++i) {
                    while (!op_queue_[i].empty()) {
                        op_queue_[i].front().ec_ = ec;
                        op_queue_[i].pop_front_and_dispose([&ops](reactor_op * op) {
                            ops.push_back(*op);
                        });
                    }
                }
            }

            void set_endpoint(socket_ops::endpoint_type endpoint, bool serverish) {
                endpoint_ = std::move(endpoint);
                serverish_ = serverish;
            }

            void clear_endpoint() {
                endpoint_.clear();
                serverish_ = false;
            }

            void format(std::ostream & stm) {
                char const* kinds[] = {"PAIR", "PUB", "SUB", "REQ", "REP",
                                        "DEALER", "ROUTER", "PULL", "PUSH",
                                        "XPUB", "XSUB", "STREAM"
                                      };
                static_assert(ZMQ_PAIR == 0, "ZMQ_PAIR");
                boost::system::error_code ec;
                auto kind = socket_ops::get_socket_kind(socket_, ec);
                if (ec)
                    throw boost::system::system_error(ec);
                BOOST_ASSERT_MSG(kind >= 0 && kind <= ZMQ_STREAM, "kind not in [ZMQ_PAIR, ZMQ_STREAM]");
                stm << "socket[" << kinds[kind] << "]{ ";
                if (!endpoint_.empty())
                    stm << (serverish_ ? '@' : '>') << endpoint_ << ' ';
                stm << "}";
            }

            void lock() const {
                if (optimize_single_threaded_) return;
                mutex_.lock();
            }

            void try_lock() const {
                if (optimize_single_threaded_) return;
                mutex_.try_lock();
            }

            void unlock() const {
                if (optimize_single_threaded_) return;
                mutex_.unlock();
            }
        };
        using unique_lock = boost::unique_lock<per_descriptor_data>;
        using implementation_type = std::shared_ptr<per_descriptor_data>;

        using core_access = azmq::detail::core_access<socket_service>;

        explicit socket_service(boost::asio::io_service & ios)
            : azmq::detail::service_base<socket_service>(ios)
            , ctx_(context_ops::get_context())
        { }

        void shutdown_service() override {
            ctx_.reset();
        }

        context_type context() const { return ctx_; }

        void construct(implementation_type & impl) {
            impl = std::make_shared<per_descriptor_data>();
        }

        void move_construct(implementation_type & impl,
                            socket_service &,
                            implementation_type & other) {
            impl = std::move(other);
        }

        void move_assign(implementation_type & impl,
                         socket_service &,
                         implementation_type & other) {
            impl = std::move(other);
        }

        boost::system::error_code do_open(implementation_type & impl,
                                          int type,
                                          bool optimize_single_threaded,
                                          boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(impl, "impl");
            impl->do_open(get_io_service(), ctx_, type, optimize_single_threaded, ec);
            if (ec)
                impl.reset();
            return ec;
        }

        void destroy(implementation_type & impl) {
            impl.reset();
        }

        native_handle_type native_handle(implementation_type & impl) {
            BOOST_ASSERT_MSG(impl, "impl");
            unique_lock l{ *impl };
            return impl->socket_.get();
        }

        template<typename Extension>
        bool associate_ext(implementation_type & impl, Extension&& ext) {
            BOOST_ASSERT_MSG(impl, "impl");
            unique_lock l{ *impl };
            exts_type::iterator it;
            bool res;
            std::tie(it, res) = impl->exts_.emplace(std::type_index(typeid(Extension)),
                                                    socket_ext(std::forward<Extension>(ext)));
            if (res)
                it->second.on_install(get_io_service(), impl->socket_.get());
            return res;
        }

        template<typename Extension>
        bool remove_ext(implementation_type & impl) {
            BOOST_ASSERT_MSG(impl, "impl");
            unique_lock l{ *impl };
            auto it = impl->exts_.find(std::type_index(typeid(Extension)));
            if (it != std::end(impl->exts_)) {
                it->second.on_remove();
                impl->exts_.erase(it);
                return true;
            }
            return false;
        }

        template<typename Option>
        boost::system::error_code set_option(Option const& option,
                                             boost::system::error_code & ec) {
            return context_ops::set_option(ctx_, option, ec);
        }

        template<typename Option>
        boost::system::error_code get_option(Option & option,
                                             boost::system::error_code & ec) {
            return context_ops::get_option(ctx_, option, ec);
        }

        template<typename Option>
        boost::system::error_code set_option(implementation_type & impl,
                                             Option const& option,
                                             boost::system::error_code & ec) {
            unique_lock l{ *impl };
            switch (option.name()) {
            case allow_speculative::static_name::value :
                    ec = boost::system::error_code();
                    impl->allow_speculative_ = option.data() ? *static_cast<bool const*>(option.data())
                                                             : false;
                break;
            default:
                for (auto& ext : impl->exts_) {
                    if (ext.second.set_option(option, ec)) {
                        if (ec.value() == boost::system::errc::not_supported) continue;
                        return ec;
                    }
                }
                ec = boost::system::error_code();
                socket_ops::set_option(impl->socket_, option, ec);
            }
            return ec;
        }

        template<typename Option>
        boost::system::error_code get_option(implementation_type & impl,
                                             Option & option,
                                             boost::system::error_code & ec) {
            unique_lock l{ *impl };
            switch (option.name()) {
            case allow_speculative::static_name::value :
                    if (option.size() < sizeof(bool)) {
                        ec = make_error_code(boost::system::errc::invalid_argument);
                    } else {
                        ec = boost::system::error_code();
                        *static_cast<bool*>(option.data()) = impl->allow_speculative_;
                    }
                break;
            default:
                for (auto& ext : impl->exts_) {
                    if (ext.second.get_option(option, ec)) {
                        if (ec.value() == boost::system::errc::not_supported) continue;
                        return ec;
                    }
                }
                ec = boost::system::error_code();
                socket_ops::get_option(impl->socket_, option, ec);
            }
            return ec;
        }

        boost::system::error_code descriptor_shutdown(implementation_type & impl,
                                                      shutdown_type what,
                                                      boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (impl->shutdown_ < what)
                impl->shutdown_ = what;
            else
                ec = make_error_code(boost::system::errc::operation_not_permitted);
            return ec;
        }

        endpoint_type endpoint(implementation_type const& impl) const {
            unique_lock l{ *impl };
            return impl->endpoint_;
        }

        boost::system::error_code bind(implementation_type & impl,
                                       socket_ops::endpoint_type endpoint,
                                       boost::system::error_code & ec) {
            unique_lock l{ *impl };
            // Note - socket_ops::bind() may modify the local copy of endpoint
            if (socket_ops::bind(impl->socket_, endpoint, ec))
                return ec;
            impl->set_endpoint(std::move(endpoint), true);
            return ec;
        }

        boost::system::error_code unbind(implementation_type & impl,
                                         socket_ops::endpoint_type const& endpoint,
                                         boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (socket_ops::unbind(impl->socket_, endpoint, ec))
                return ec;
            impl->clear_endpoint();
            return ec;
        }

        boost::system::error_code connect(implementation_type & impl,
                                          socket_ops::endpoint_type endpoint,
                                          boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (socket_ops::connect(impl->socket_, endpoint, ec))
                return ec;
            impl->set_endpoint(std::move(endpoint), false);
            return ec;
        }

        boost::system::error_code disconnect(implementation_type & impl,
                                             socket_ops::endpoint_type const& endpoint,
                                             boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (socket_ops::disconnect(impl->socket_, endpoint, ec))
                return ec;
            impl->clear_endpoint();
            return ec;
        }

        template<typename ConstBufferSequence>
        size_t send(implementation_type & impl,
                    ConstBufferSequence const& buffers,
                    flags_type flags,
                    boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (is_shutdown(impl, op_type::write_op, ec))
                return 0;
            auto r = socket_ops::send(buffers, impl->socket_, flags, ec);
            check_missed_events(impl);
            return r;
        }

        size_t send(implementation_type & impl,
                    message const& msg,
                    flags_type flags,
                    boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (is_shutdown(impl, op_type::write_op, ec))
                return 0;
            auto r = socket_ops::send(msg, impl->socket_, flags, ec);
            check_missed_events(impl);
            return r;
        }

        template<typename MutableBufferSequence>
        size_t receive(implementation_type & impl,
                       MutableBufferSequence const& buffers,
                       flags_type flags,
                       boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (is_shutdown(impl, op_type::read_op, ec))
                return 0;
            auto r = socket_ops::receive(buffers, impl->socket_, flags, ec);
            check_missed_events(impl);
            return r;
        }

        size_t receive(implementation_type & impl,
                       message & msg,
                       flags_type flags,
                       boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (is_shutdown(impl, op_type::read_op, ec))
                return 0;
            auto r = socket_ops::receive(msg, impl->socket_, flags, ec);
            check_missed_events(impl);
            return r;
        }

        size_t receive_more(implementation_type & impl,
                            message_vector & vec,
                            flags_type flags,
                            boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (is_shutdown(impl, op_type::read_op, ec))
                return 0;
            auto r = socket_ops::receive_more(vec, impl->socket_, flags, ec);
            check_missed_events(impl);
            return r;
        }

        size_t flush(implementation_type & impl,
                     boost::system::error_code & ec) {
            unique_lock l{ *impl };
            if (is_shutdown(impl, op_type::read_op, ec))
                return 0;
            auto r = socket_ops::flush(impl->socket_, ec);
            check_missed_events(impl);
            return r;
        }

        using reactor_op_ptr = std::unique_ptr<reactor_op>;
        template<typename T, typename... Args>
        void enqueue(implementation_type & impl, op_type o, Args&&... args) {
            reactor_op_ptr p{ new T(std::forward<Args>(args)...) };
            boost::system::error_code ec = enqueue(impl, o, p);
            if (ec) {
                BOOST_ASSERT_MSG(p, "op ptr");
                p->ec_ = ec;
                reactor_op::do_complete(p.release());
            }
        }

        boost::system::error_code cancel(implementation_type & impl,
                                         boost::system::error_code & ec) {
            unique_lock l{ *impl };
            descriptors_.unregister_descriptor(impl);
            cancel_ops(impl);
            return impl->cancel_stream_descriptor(ec);
        }

        std::string monitor(implementation_type & impl, int events,
                            boost::system::error_code & ec) {
            return socket_ops::monitor(impl->socket_, events, ec);
        }

        void format(implementation_type const& impl,
                    std::ostream & stm) {
            unique_lock l{ *impl };
            impl->format(stm);
        }

    private:
        context_type ctx_;

        bool is_shutdown(implementation_type & impl, op_type o, boost::system::error_code & ec) {
            if (is_shutdown(o, impl->shutdown_)) {
                ec = make_error_code(boost::system::errc::operation_not_permitted);
                return true;
            }
            return false;
        }

        static bool is_shutdown(op_type o, shutdown_type what) {
            return (o == op_type::write_op) ? what >= shutdown_type::send
                                            : what >= shutdown_type::receive;
        }

        static void cancel_ops(implementation_type & impl) {
            op_queue_type ops;
            impl->cancel_ops(reactor_op::canceled(), ops);
            while (!ops.empty())
                ops.pop_front_and_dispose(reactor_op::do_complete);
        }

        using weak_descriptor_ptr = std::weak_ptr<per_descriptor_data>;

        static void handle_missed_events(weak_descriptor_ptr const& weak_impl, boost::system::error_code ec) {
            auto impl = weak_impl.lock();
            if (!impl)
                return;

            op_queue_type ops;
            {
                unique_lock l{ *impl };

                impl->missed_events_found_ = false;

                if (!ec)
                    impl->perform_ops(ops, ec);
                if (ec)
                    impl->cancel_ops(ec, ops);
            }
            while (!ops.empty())
                ops.pop_front_and_dispose(reactor_op::do_complete);
        }

        void check_missed_events(implementation_type & impl)
        {
            if (!impl->scheduled_ || impl->missed_events_found_)
                return;

            boost::system::error_code ec;
            auto evs = socket_ops::get_events(impl->socket_, ec) & impl->events_mask();

            if (evs || ec)
            {
                impl->missed_events_found_ = true;
                weak_descriptor_ptr weak_impl(impl);
                impl->sd_->get_io_service().post([weak_impl, ec]() { handle_missed_events(weak_impl, ec); });
            }
        }

        struct descriptor_map {
            ~descriptor_map() {
                lock_type l{ mutex_ };
                for(auto&& descriptor : map_)
                    if (auto impl = descriptor.second.lock()) {
                        cancel_ops(impl);
                    }
            }

            void register_descriptor(implementation_type & impl) {
                lock_type l{ mutex_ };
                auto handle = impl->sd_->native_handle();
                map_.emplace(handle, impl);
            }

            void unregister_descriptor(implementation_type & impl) {
                lock_type l{ mutex_ };
                auto handle = impl->sd_->native_handle();
                map_.erase(handle);
            }

        private:
            mutable boost::mutex mutex_;
            using lock_type = boost::unique_lock<boost::mutex>;
            using key_type = socket_ops::native_handle_type;
            boost::container::flat_map<key_type, weak_descriptor_ptr> map_;
        };

        struct reactor_handler {
            descriptor_map & descriptors_;
            weak_descriptor_ptr per_descriptor_data_;

            reactor_handler(descriptor_map & descriptors,
                            implementation_type const& per_descriptor_data)
                : descriptors_(descriptors)
                , per_descriptor_data_(per_descriptor_data)
            { }

            void operator()(boost::system::error_code ec, size_t) const {
                auto p = per_descriptor_data_.lock();
                if (!p)
                    return;

                op_queue_type ops;
                {
                    unique_lock l{ *p };

                    if (!ec)
                        p->scheduled_ = p->perform_ops(ops, ec);
                    if (ec) {
                        p->scheduled_ = false;
                        p->cancel_ops(ec, ops);
                    }

                    if (p->scheduled_)
                        p->sd_->async_read_some(boost::asio::null_buffers(), *this);
                    else
                        descriptors_.unregister_descriptor(p);
                }
                while (!ops.empty())
                    ops.pop_front_and_dispose(reactor_op::do_complete);
            }

            static void schedule(descriptor_map & descriptors, implementation_type & impl) {
                reactor_handler handler(descriptors, impl);
                descriptors.register_descriptor(impl);

                boost::system::error_code ec;
                auto evs = socket_ops::get_events(impl->socket_, ec) & impl->events_mask();

                if (evs || ec) {
                    impl->sd_->get_io_service().post([handler, ec] { handler(ec, 0); });
                } else {
                    impl->sd_->async_read_some(boost::asio::null_buffers(),
                                                std::move(handler));
                }
            }
        };

        struct deferred_completion {
            weak_descriptor_ptr owner_;
            reactor_op *op_;

            deferred_completion(implementation_type const& owner,
                                reactor_op_ptr op)
                : owner_(owner)
                , op_(op.release())
            { }

            void operator()() {
                reactor_op::do_complete(op_);
                if (auto p = owner_.lock()) {
                    unique_lock l{ *p };
                    p->in_speculative_completion_ = false;
                }
            }

            friend
            bool asio_handler_is_continuation(deferred_completion* /*handler*/) { return true; }
        };

        descriptor_map descriptors_;

        boost::system::error_code enqueue(implementation_type & impl,
                                        op_type o, reactor_op_ptr & op) {
            unique_lock l{ *impl };
            boost::system::error_code ec;
            if (is_shutdown(impl, o, ec))
                return ec;

            // we have at most one speculative completion in flight at any time
            if (impl->allow_speculative_ && !impl->in_speculative_completion_) {
                // attempt to execute speculatively when the op_queue is empty
                if (impl->op_queue_[o].empty()) {
                    if (op->do_perform(impl->socket_)) {
                        impl->in_speculative_completion_ = true;
                        l.unlock();
                        get_io_service().post(deferred_completion(impl, std::move(op)));
                        return ec;
                    }
                }
            }
            impl->op_queue_[o].push_back(*op.release());

            if (!impl->scheduled_) {
                impl->scheduled_ = true;
                reactor_handler::schedule(descriptors_, impl);
            } else {
                check_missed_events(impl);
            }
            return ec;
        }
    };

    template<typename T, typename Extension>
    static bool associate_ext(T & that, Extension&& ext) {
        socket_service::core_access access{ that };
        return access.service().associate_ext(access.implementation(), std::forward<Extension>(ext));
    }

    template<typename T, typename Extension>
    static bool remove_ext(T & that) {
        socket_service::core_access access{ that };
        return access.service().remove_ext<Extension>(access.implementation());
    }
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_SOCKET_SERVICE_HPP__

