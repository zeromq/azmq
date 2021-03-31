/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_SOCKET_EXT_HPP__
#define AZMQ_DETAIL_SOCKET_EXT_HPP__
#include "../error.hpp"

#include <boost/assert.hpp>
#include <boost/asio/io_service.hpp>

#include <memory>
#include <typeindex>

namespace azmq {
namespace detail {
    struct socket_ext {
        template<typename T>
        socket_ext(T data)
            : ptr_(new model<T>(std::move(data)))
        { }

        ~socket_ext() { on_remove(); }

        // MSVS 2013 can not generate move ctor/assignment

        socket_ext(socket_ext&& op): ptr_(std::move(op.ptr_)) { }

        socket_ext& operator= (socket_ext&& op) {
            ptr_ = std::move(op.ptr_);
            return *this;
        }

        void on_install(boost::asio::io_service& ios, void * socket) const {
            BOOST_ASSERT_MSG(ptr_, "reusing moved instance of socket_ext");
            ptr_->on_install(ios, socket);
        }

        void on_remove() {
            if (ptr_)
                ptr_->on_remove();
            ptr_.reset();
        }

        template<typename Option>
        boost::system::error_code set_option(Option const& opt, boost::system::error_code & ec) const {
            BOOST_ASSERT_MSG(ptr_, "reusing (re)moved instance of socket_ext");
            return ptr_->set_option(opt_model<Option>(const_cast<Option&>(opt)), ec);
        }

        template<typename Option>
        boost::system::error_code get_option(Option & opt, boost::system::error_code & ec) const {
            BOOST_ASSERT_MSG(ptr_, "reusing (re)moved instance of socket_ext");
            return ptr_->set_option(opt_model<Option>(opt), ec);
        }

    private :
        struct opt_concept {
            virtual ~opt_concept() = default;

            virtual int name() const = 0;
            virtual void const* data() const = 0;
            virtual void* data() = 0;
            virtual size_t size() = 0;
        };

        template<typename Option>
        struct opt_model : opt_concept {
            Option & data_;

            opt_model(Option & data) : data_(data) { }

            int name() const override { return data_.name(); }
            void const* data() const override { return data_.data(); }
            void* data() override { return data_.data(); }
            size_t size() override { return data_.size(); }
        };

        struct concept_ {
            virtual ~concept_() = default;

            virtual void on_install(boost::asio::io_service &, void *) = 0;
            virtual void on_remove() = 0;
            virtual boost::system::error_code set_option(opt_concept const&, boost::system::error_code &) = 0;
            virtual boost::system::error_code get_option(opt_concept &, boost::system::error_code &) = 0;
        };
        std::unique_ptr<concept_> ptr_;

        template<typename T>
        struct model : concept_ {
            T data_;

            model(T data): data_(std::move(data)) { }

            void on_install(boost::asio::io_service & ios, void * socket) override { data_.on_install(ios, socket); }
            void on_remove() override { data_.on_remove(); }
            boost::system::error_code set_option(opt_concept const& opt, boost::system::error_code & ec) override {
                return data_.set_option(opt, ec);
            }

            boost::system::error_code get_option(opt_concept & opt, boost::system::error_code & ec) override {
                return data_.get_option(opt, ec);
            }
        };
    };
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_SOCKET_EXT_HPP__

