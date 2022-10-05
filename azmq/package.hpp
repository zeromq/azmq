/*
    Copyright (c) 2022 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_PACKAGE_HPP__
#define AZMQ_PACKAGE_HPP__

#include "error.hpp"
#include "message.hpp"
#include "util/scope_guard.hpp"
#include "util/util.hpp"

#include <zmq.h>

#include <boost/assert.hpp>
#include <boost/version.hpp>

#include <memory>
#include <type_traits>
#include <vector>

namespace azmq
{

class package
{
  public:
    auto begin () { return messages_.begin (); }
    auto end () { return messages_.end (); }

    package () = default;
    ~package () = default;

    package (const package &other) : messages_{other.messages_} {}

    package &operator= (const package &other)
    {
        messages_ = other.messages_;

        return *this;
    }

    package (package &&other) : messages_{std::move (other.messages_)} {}

    package &operator= (package &&other)
    {
        messages_ = std::move (other.messages_);

        return *this;
    }

    size_t size () const { return messages_.size (); }

    // Copy operators will take copies of any data with a given size
    template <typename T> void add_raw (T const *data, size_t const data_size)
    {
        messages_.emplace_back (boost::asio::buffer (data, data_size));
    }

    size_t size ()
    {
        size_t size = 0;
        for (const auto &message : messages_) {
            size += message.size ();
        }

        return size;
    }

    size_t messages_count () { return messages_.size (); }

    const message &operator[] (size_t index) const { return messages_[index]; }

    template <typename T> package &operator<< (T const &value)
    {
        if constexpr (std::is_same_v<T, std::string>) {
            add_raw (reinterpret_cast<void const *> (value.data ()),
                     value.size ());
        } else if constexpr (std::is_same_v<T, message>) {
            add_raw (value.data (), value.size ());
        } else if constexpr (std::is_convertible_v<T, char const *>) {
            add_raw (reinterpret_cast<void const *> (value), strlen (value));
        } else if constexpr (std::is_arithmetic_v<T>) {
            add_raw (reinterpret_cast<void const *> (&value), sizeof (T));
        } else {
            static_assert (util::always_false_v<T>, "unsupported type");
        }

        return *this;
    }

    void reset () { messages_.clear (); }

  private:
    friend detail::socket_ops;
    std::vector<message> messages_;
};

AZMQ_V1_INLINE_NAMESPACE_END
} // namespace azmq
#endif // AZMQ_MESSAGE_HPP__
