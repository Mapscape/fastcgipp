/*******************************************************************************
* Copyright (C) 2016 Eddie Carle [eddie@isatec.ca]                             *
*                                                                              *
* This file is part of fastcgi++.                                              *
*                                                                              *
* fastcgi++ is free software: you can redistribute it and/or modify it under   *
* the terms of the GNU Lesser General Public License as  published by the Free *
* Software Foundation, either version 3 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* fastcgi++ is distributed in the hope that it will be useful, but WITHOUT ANY *
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS    *
* FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with fastcgi++.  If not, see <http://www.gnu.org/licenses/>.           *
*******************************************************************************/

#include <fastcgi++/request.hpp>
#include <fastcgi++/manager.hpp>

// Let's make our request handling class. It must do at minimum:
// 1) Be derived from Fastcgipp::Request
// 2) Define the virtual response() member function from Fastcgipp::Request

// First things first let's decide on what kind of character set we will use.
// Obviously with all these different languages we can't use something like
// ISO-8859-1. Our only option is unicode and in particular utf-8. The way this
// library handles unicode might be different than some are used to. All
// internal characters are wide.  This way we don't have to mess around with
// variable size characters in our program.  A string with 10 wchar_ts is ten
// characters long.  Not up in the air as it is with utf-8. Anyway, moving
// right along, the streams will code convert all the wide characters to utf-8
// before it is sent out to the client.  This way we get the best of both
// worlds.

// So, whenever we are going to use utf-8, our template parameter for
// Fastcgipp::Request<charT> should be wchar_t. Keep in mind that this suddenly
// makes everything wide character and unicode compatible. Including HTTP header
// data (cookies, urls, yada-yada).

class HelloWorld: public Fastcgipp::Request<wchar_t>
{
    bool response()
    {
        // Let's make our header, note the charset=utf-8. Remember that HTTP
        // headers must be terminated with \r\n\r\n. NOT just \n\n.
        out << L"Content-Type: text/html; charset=utf-8\r\n\r\n";

        // Now it's all stuff you should be familiar with
        out <<
L"<!DOCTYPE html>\n"
L"<html>"
    L"<head>"
        L"<meta http-equiv='Content-Type' content='text/html; charset=utf-8' />"
        L"<meta charset='utf-8' />"
        L"<title>fastcgi++: Hello World</title>"
    L"</head>"
    L"<body>"
        L"<p>"
            L"English: Hello World<br>"
            L"Russian: Привет мир<br>"
            L"Greek: Γεια σας κόσμο<br>"
            L"Chinese: 世界您好<br>"
            L"Japanese: 今日は世界<br>"
            L"Runic English?: ᚺᛖᛚᛟ ᚹᛟᛉᛚᛞ<br>"
        L"</p>"
    L"</body>"
L"</html>";

        // There is also a stream setup for error output. Anything sent here
        // will go to your we server error log. Personally I prefer using the
        // internal fastcgi++ logging mechanisms but we'll send something here
        // for fun.
        err << L"Hello apache error log";

        // Always return true if you are done. This will let fastcgi++ know we
        // are done and the manager will destroy the request and free it's
        // resources. Return false if you are not finished but want to
        // relinquish control and give other requests some CPU time. You might
        // do this after an SQL query while waiting for a reply. Passing
        // messages to requests through the manager is possible but beyond the
        // scope of this example.
        return true;
    }
};

// The main function is easy to set up
int main()
{
    // First we make a Fastcgipp::Manager object, with our request handling
    // class as a template parameter.
    Fastcgipp::Manager<HelloWorld> manager;

    // We need to do this to ensure the manager is tied to signals that the web
    // server may send.
    manager.setupSignals();

    // The default listen() call makes the manager listen for incoming
    // connections on the default FastCGI socket
    manager.listen();

    // This tells the manager to start it's worker threads.
    manager.start();

    // Now we just pause execution until the manager threads have completed.
    manager.join();

    return 0;
}
