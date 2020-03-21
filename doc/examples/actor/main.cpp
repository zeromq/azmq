#include <azmq/actor.hpp>

#include <boost/utility/string_ref.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <memory>
#include <array>
#include <atomic>
#include <iostream>

namespace asio = boost::asio;
namespace pt = boost::posix_time;

class server_t {
public:
    server_t(asio::io_context& ios)
        : pimpl_(std::make_shared<impl>())
        , frontend_(azmq::actor::spawn(ios, run, pimpl_))
    { }

    void ping() {
        frontend_.send(asio::buffer("PING"));
        frontend_.async_receive(asio::buffer(buf_), [this](boost::system::error_code const& ec, size_t bytes_transferred) {
            if (ec)
                return;
            if (boost::string_ref(buf_.data(), bytes_transferred - 1) == "PONG")
                pimpl_->pongs_++;
        });
    }

    friend std::ostream & operator<<(std::ostream & stm, server_t const & that) {
        return stm << "pings=" << that.pimpl_->pings_
                   << ", pongs=" << that.pimpl_->pongs_;
    }

private:
    // for such a simple example, this is overkill, but is a useful pattern for
    // real servers that need to maintain state
    struct impl {
        std::atomic<unsigned long> pings_;
        std::atomic<unsigned long> pongs_;
        std::array<char, 256> buf_;

        impl()
            : pings_(0)
            , pongs_(0)
        { }
    };
    using ptr = std::shared_ptr<impl>;
    ptr pimpl_;

    // we schedule async receives for the backend socket here
    static void do_receive(azmq::socket & backend, std::weak_ptr<impl> pimpl) {
        if (auto p = pimpl.lock()) {
            backend.async_receive(asio::buffer(p->buf_), [&backend, pimpl](boost::system::error_code const& ec, size_t bytes_transferred) {
                if (ec)
                    return; // exit on error

                if (auto p = pimpl.lock()) {
                    if (boost::string_ref(p->buf_.data(), bytes_transferred - 1) != "PING")
                        return; // exit if not PING
                    p->pings_++;
                    backend.send(asio::buffer("PONG"));

                    // schedule another receive
                    do_receive(backend, pimpl);
                }
            });
        }
    }

    // This is the function run by the background thread
    static void run(azmq::socket & backend, ptr pimpl) {
        do_receive(backend, pimpl);
        backend.get_executor().context().run();
    }

    azmq::socket frontend_;
    std::array<char, 256> buf_;
};


// ping every 250ms
void schedule_ping(asio::deadline_timer & timer, server_t & server) {
    server.ping();

    timer.expires_from_now(pt::milliseconds(250));
    timer.async_wait([&](boost::system::error_code const& ec) {
        if (ec)
            return;
        schedule_ping(timer, server);
    });
};

int main(int argc, char** argv) {
    asio::io_context ios;

    std::cout << "Running...";
    std::cout.flush();

    // halt on SIGINT or SIGTERM
    asio::signal_set signals(ios, SIGTERM, SIGINT);
    signals.async_wait([&](boost::system::error_code const&, int) {
        ios.stop();
    });

    server_t server(ios);

    asio::deadline_timer timer(ios);
    schedule_ping(timer, server);

    // run for 5 secods
    asio::deadline_timer deadline(ios, pt::seconds(5));
    deadline.async_wait([&](boost::system::error_code const&) {
        ios.stop();
    });

    ios.run();

    std::cout << "Done. Results - " << server << std::endl;

    return 0;
}
