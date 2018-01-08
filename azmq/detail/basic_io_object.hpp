/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_BASIC_IO_OBJECT_HPP__
#define AZMQ_DETAIL_BASIC_IO_OBJECT_HPP__

#include <boost/asio/io_service.hpp>
#include <boost/asio/basic_io_object.hpp>

namespace azmq {
namespace detail {
    template<typename Service>
    class core_access {
    public:
        using service_type = Service;
        using implementation_type = typename service_type::implementation_type;

        template<typename T>
        core_access(T & that)
            : service_(that.get_service())
            , impl_(that.get_implementation())
        { }

        service_type & service() { return service_; }
        implementation_type & implementation() { return impl_; }

    private:
        service_type & service_;
        implementation_type & impl_;
    };

    template<typename Service>
    class basic_io_object
        : public boost::asio::basic_io_object<Service> {

        friend class core_access<Service>;

    public:
        basic_io_object(boost::asio::io_service& ios)
            : boost::asio::basic_io_object<Service>(ios)
        { }
    };
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_BASIC_IO_OBJECT_HPP__

