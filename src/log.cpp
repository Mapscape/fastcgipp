/*!
 * @file       log.cpp
 * @brief      Defines the Fastcgipp debugging/logging mechanism
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 21, 2016
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

#include "fastcgi++/log.hpp"

#include <iomanip>
#include <iostream>
#include <ctime>
#include <chrono>
#include <cmath>

std::wostream* Fastcgipp::Logging::logstream(&std::wcerr);
bool Fastcgipp::Logging::logTimestamp(false);
std::mutex Fastcgipp::Logging::mutex;
bool Fastcgipp::Logging::suppress(false);

void Fastcgipp::Logging::timestamp()
{
    if(logTimestamp)
    {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto nowTimeT = std::chrono::system_clock::to_time_t(now);
        const auto ms =
            (unsigned long)(std::chrono::duration<double, std::milli>(
                now.time_since_epoch()).count())%1000;

        *logstream
            << std::put_time(std::localtime(&nowTimeT), L"[%Y-%b-%d %H:%M:%S.")
            << ms << L"] ";
    }
}
