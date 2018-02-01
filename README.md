# AZMQ Boost Asio + ZeroMQ

## Welcome
The azmq library provides Boost Asio style bindings for ZeroMQ

This library is built on top of ZeroMQ's standard C interface and is
intended to work well with C++ applications which use the Boost libraries
in general, and Asio in particular.

The main abstraction exposed by the library is azmq::socket which
provides an Asio style socket interface to the underlying zeromq socket
and interfaces with Asio's io_service().  The socket implementation
participates in the io_service's reactor for asynchronous IO and
may be freely mixed with other Asio socket types (raw TCP/UDP/Serial/etc.).

## Building and installation

Building requires a recent version of CMake (2.8.12 or later for Visual Studio, 2.8 or later for the rest), and a C++ compiler
which supports C++11. Currently this has been tested with -
* Xcode 5.1 on OS X 10.8
* Xcode 6 on OS X 10.9
* Xcode 6.4 on OS X 10.10
* Xcode 7.1 on OS X 10.11
* GCC 4.8 + Boost 1.48 on CentOS 6
* GCC 4.8 + Boost 1.53 on CentOS 7
* GCC 4.8 on Arch Linux and Ubuntu
* GCC 4.8 on Ubuntu
* GCC 5.3 + Boost 1.60 on Ubuntu
* Microsoft Visual Studio 2013 on Windows Server 2008 R2

Library dependencies are -
* Boost 1.48 or later
* ZeroMQ 4.0.x

Tests and example code require -
* Boost 1.54 or later

To build on Linux / OS X -
```
$ mkdir build && cd build
$ cmake ..
$ make
$ make test
$ make install
```

To build on Windows -
```
> mkdir build
> cd build
> cmake ..
> cmake --build . --config Release
> ctest . -C Release
```
You can also open Visual Studio solution from `build` directory after invoking CMake.

To change the default install location use `-DCMAKE_INSTALL_PREFIX` when invoking CMake.

To change where the build looks for Boost and ZeroMQ use `-DBOOST_ROOT=<my custom Boost install>` and `-DZMQ_ROOT=<my custom ZeroMQ install>` when invoking CMake. Or set `BOOST_ROOT` and `ZMQ_ROOT` environment variables.

## Example Code
This is an azmq version of the code presented in the ZeroMQ guide at
http://zeromq.org/intro:read-the-manual

```cpp
#include <azmq/socket.hpp>
#include <boost/asio.hpp>
#include <array>

namespace asio = boost::asio;

int main(int argc, char** argv) {
    asio::io_service ios;
    azmq::sub_socket subscriber(ios);
    subscriber.connect("tcp://192.168.55.112:5556");
    subscriber.connect("tcp://192.168.55.201:7721");
    subscriber.set_option(azmq::socket::subscribe("NASDAQ"));

    azmq::pub_socket publisher(ios);
    publisher.bind("ipc://nasdaq-feed");

    std::array<char, 256> buf;
    for (;;) {
        auto size = subscriber.receive(asio::buffer(buf));
        publisher.send(asio::buffer(buf));
    }
    return 0;
}
```

Further examples may be found in doc/examples

## Build status

[AZMQ build status](https://136.243.151.173:4433/project.html?projectId=Azmq&guest=1)

## Copying

Use of this software is granted under the the BOOST 1.0 license
(same as Boost Asio).  For details see the file `LICENSE-BOOST_1_0
included with the distribution.

## Contributing

AZMQ uses the [C4.1 (Collective Code Construction Contract)](http://rfc.zeromq.org/spec:22) process for contributions.
See the accompanying CONTRIBUTING file for more information.
