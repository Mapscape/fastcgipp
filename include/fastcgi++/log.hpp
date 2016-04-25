/*!
 * @file       log.hpp
 * @brief      Declares the Fastcgipp debugging/logging facilities
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       April 25, 2016
 * @copyright  Copyright &copy; 2016 Eddie Carle. This project is released under
 *             the GNU Lesser General Public License Version 3.
 */

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

#ifndef FASTCGIPP_LOG_HPP
#define FASTCGIPP_LOG_HPP

#include "fastcgi++/config.h"

#include <ostream>
#include <mutex>
#include <cstdlib>

//! Topmost namespace for the fastcgi++ library
namespace Fastcgipp
{
    //! Contains the Fastcgipp debugging/logging mechanism
    namespace Logging
    {
        //! The actual stream we will be logging to.
        extern std::wostream* logstream;

        //! If true, we log the timestamp. If false, we don't.
        extern bool logTimestamp;

        //! Thread safe the logging mechanism
        extern std::mutex mutex;

        //! Send a timestamp to logstream if logTimestamp is true
        void timestamp();

        //! Set to true if you want to suppress non-error logs
        extern bool suppress;
    }
}

//! This is for the user to log whatever they want.
#define INFO_LOG(data) {\
    if(!::Fastcgipp::Logging::suppress)\
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[info] " << data << std::endl;\
    }}

//! Log any "errors" that cannot be recovered from and then exit.
/*!
 * This should encompass all errors that should \a never happen, regardless of
 * what the external conditions are. This also presumes that there is no
 * possibility to recover from the error and we should simply terminate.
 */
#define FAIL_LOG(data) {\
    if(!::Fastcgipp::Logging::suppress)\
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[fail] " << data << std::endl;\
        std::exit(EXIT_FAILURE);\
    }}

#if FASTCGIPP_LOG_LEVEL > 0
//! Log any "errors" that can be recovered from.
/*!
 * This should encompass all errors that should \a never happen, regardless of
 * what the external conditions are. This also presumes that there is a
 * mechanism to recover from said error.
 */
#define ERROR_LOG(data) \
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[error] " << data << std::endl;\
    }
#else
#define ERROR_LOG(data)
#endif

#if FASTCGIPP_LOG_LEVEL > 1
//! Log any externally caused "errors"
/*!
 * We allocate this "log level" to errors that are caused by external factors
 * like bad data from a web server. Once can't say that these errors should
 * never happen since they are external controlled.
 */
#define WARNING_LOG(data) {\
    if(!::Fastcgipp::Logging::suppress)\
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[warning] " << data << std::endl;\
    }}
#else
#define WARNING_LOG(data)
#endif

#if FASTCGIPP_LOG_LEVEL > 2
//! The intention here is for general debug/analysis logging
#define DEBUG_LOG(data) {\
    if(!::Fastcgipp::Logging::suppress)\
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[debug] " << data << std::endl;\
    }}
#else
#define DEBUG_LOG(data)
#endif

#endif
