/*!
 * @file       protocol.cpp
 * @brief      Defines everything for relating to the FastCGI protocol itself.
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 6, 2016
 * @copyright  Copyright &copy; 2016 Eddie Carle. This project is released under
 *             the GNU Lesser General Public License Version 3.
 *
 * It is this file, along with sockets.hpp, that must be modified to make
 * fastcgi++ work on any non-linux operating system.
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

#include "fastcgi++/protocol.hpp"

void Fastcgipp::Protocol::processParamHeader(
        const char* data,
        size_t dataSize,
        const char*& name,
        size_t& nameSize,
        const char*& value,
        size_t& valueSize)
{
    if(*data>>7)
    {
        nameSize=BigEndian<uint32_t>::read(data) & 0x7fffffff;
        data+=sizeof(uint32_t);
    }
    else nameSize=*data++;

    if(*data>>7)
    {
        valueSize=BigEndian<uint32_t>::read(data) & 0x7fffffff;
        data+=sizeof(uint32_t);
    }
    else valueSize=*data++;
    name=data;
    value=name+nameSize;
}

const Fastcgipp::Protocol::ManagementReply<14, 2>
Fastcgipp::Protocol::maxConnsReply("FCGI_MAX_CONNS", "10");

const Fastcgipp::Protocol::ManagementReply<13, 2>
Fastcgipp::Protocol::maxReqsReply("FCGI_MAX_REQS", "50");

const Fastcgipp::Protocol::ManagementReply<15, 1>
Fastcgipp::Protocol::mpxsConnsReply("FCGI_MPXS_CONNS", "1");

//const char Fastcgipp::version[]=PACKAGE_VERSION;
const char Fastcgipp::version[]="3.0alpha";
