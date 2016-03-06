/*!
 * @file       log.hpp
 * @brief      Declares the Fastcgipp debugging/logging mechanism
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 6, 2016
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

#ifndef LOG_HPP
#define LOG_HPP

#include <ostream>
#include <mutex>

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
    }
}

#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

//! This is for the user to log whatever they want.
#define INFO_LOG(data) \
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[info] " << data << std::endl;\
    }

#if LOG_LEVEL > 0
//! The intention here is to log any "errors" that are terminal.
#define ERROR_LOG(data) \
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[error] " << data << std::endl;\
    }
#else
#define ERROR_LOG(data)
#endif

#if LOG_LEVEL > 1
//! The intention here is to log any "errors" that are not terminal.
#define WARNING_LOG(data) \
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[warning] " << data << std::endl;\
    }
#else
#define WARNING_LOG(data)
#endif

#if LOG_LEVEL > 2
//! The intention here is for general debug/analysis logging
#define DEBUG_LOG(data) \
    { \
        std::lock_guard<std::mutex> lock(::Fastcgipp::Logging::mutex);\
        ::Fastcgipp::Logging::timestamp();\
        *::Fastcgipp::Logging::logstream << "[debug] " << data << std::endl;\
    }
#else
#define DEBUG_LOG(data)
#endif

#endif
