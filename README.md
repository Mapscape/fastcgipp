# fastcgi++ #

**3.0alpha**

**Eddie Carle**

## News ##

**April 10, 2016** - Fastcgi++ is going through a dramatic rewrite now and the
master branch does not work at all. If you're here hoping for a functional
version scroll down to the releases section. If you'd like do read a bit more
about the rewrite and fastcgi++ in general, check out "[Ten years of
fastcgi++][1]".

[1]: http://eddie.isatec.ca/2016/04/10/ten-years-of-fastcgi++.html

## About ##

This library is intended as a high-efficiency C++ api for web development. It
allows your applications to communicate with web servers through the FastCGI
protocol, tabulates all your environment data, manages character encoding, and
allows requests to effectively share CPU time. If you want any further
information check the Doxygen documentation associated with the respective
release, or build it yourself.

## Releases ##

Your best bet for releases and documentation is to clone the Git repository,
checkout the tag you want and see the building section of this file. If you're
too lazy for that, however, you can take the risk and try the following links.

 - [[fastcgi++-2.1.tar.bz2][2]] [[Documentation][3]] [[Tag][10]]
 - [[fastcgi++-2.0.tar.bz2][4]] [[Documentation][5]] [[Tag][11]]
 - [[fastcgi++-1.2.tar.bz2][6]] [[Documentation][7]]
 - [[fastcgi++-1.1.tar.bz2][8]]
 - [[fastcgi++-1.0.tar.bz2][9]]

[2]: http://download.savannah.nongnu.org/releases/fastcgipp/fastcgi++-2.1.tar.bz2
[3]: http://www.nongnu.org/fastcgipp/doc/2.1
[4]: http://download.savannah.nongnu.org/releases/fastcgipp/fastcgi++-2.0.tar.bz2
[5]: http://www.nongnu.org/fastcgipp/doc/2.0
[6]: http://download.savannah.nongnu.org/releases/fastcgipp/fastcgi++-1.2.tar.bz2
[7]: http://www.nongnu.org/fastcgipp/doc/1.2
[8]: http://download.savannah.nongnu.org/releases/fastcgipp/fastcgi++-1.1.tar.bz2
[9]: http://download.savannah.nongnu.org/releases/fastcgipp/fastcgi++-1.0.tar.bz2 
[10]: https://github.com/eddic/fastcgipp/tree/2.1
[11]: https://github.com/eddic/fastcgipp/tree/2.0

## Building ##

This should provide you with all the basic stuff you need to do to get fastcgi++
built and installed. The build system is CMake and the following instructions
assume you are in Bash.

First we need to clone.

    git clone https://github.com/eddic/fastcgipp.git fastcgi++

Then we make a build directory.

    mkdir fastcgi++.build
    cd fastcgi++.build

Now we need run cmake.

    cmake ../fastcgi++ -DCMAKE_BUILD_TYPE=RELEASE

Note that that was to do a release build. That means heavily optimized and not
good for debugging. If you want to do some debugging to either fastcgi++ or an
application you are developing that uses fastcgi++, do a debug build.

    cmake ../fastcgi++ -DCMAKE_BUILD_TYPE=DEBUG

Now let's build the library itself.

    make

Then we can build the documentation if we so desire.

    make doc

Now let's install it all (doc included if it was built).

    make install

Maybe we should build the unit tests?

    make tests

And of course we should run them as well.

    make test

And that, as they say, is it.
