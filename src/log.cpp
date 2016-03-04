/*!
 * @file       log.cpp
 * @brief      Defines the Fastcgipp debugging/logging mechanism
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 4, 2016
 * @copyright  Copyright &copy; 2016 Eddie Carle. This project is released under
 *             the GNU General Public License Version 3.
 */

/***************************************************************************
* Copyright (C) 2016 Eddie Carle [eddie@isatec.ca]                         *
*                                                                          *
* This file is part of fastcgi++.                                          *
*                                                                          *
* fastcgi++ is free software: you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as  published   *
* by the Free Software Foundation, either version 3 of the License, or (at *
* your option) any later version.                                          *
*                                                                          *
* fastcgi++ is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or    *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public     *
* License for more details.                                                *
*                                                                          *
* You should have received a copy of the GNU Lesser General Public License *
* along with fastcgi++.  If not, see <http://www.gnu.org/licenses/>.       *
****************************************************************************/

#include "fastcgi++/log.hpp"

#include <iomanip>
#include <iostream>
#include <ctime>

std::wostream* Fastcgipp::Logging::logstream(&std::wcerr);
bool Fastcgipp::Logging::logTimestamp(false);
std::mutex Fastcgipp::Logging::mutex;

void Fastcgipp::Logging::timestamp()
{
    if(logTimestamp)
    {
        std::time_t now = std::time(nullptr);
/***** std::put_time doesn't exist in gcc yet *****
        *logstream << std::put_time(
                std::localtime(&now),
                "[%Y-%b-%d %H:%M:%S.%f] ");
***** So until it does we'll use the following trash *****/
        char buffer[256];
        std::strftime(
                buffer,
                256,
                "[%Y-%b-%d %H:%M:%S.%f] ",
                std::localtime(&now));
        *logstream << buffer;
    }
}
