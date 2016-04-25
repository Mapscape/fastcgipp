/*!
 * @file       protocol.cpp
 * @brief      Defines everything for relating to the FastCGI protocol itself.
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

#include "fastcgi++/protocol.hpp"
#include "fastcgi++/config.h"

bool Fastcgipp::Protocol::processParamHeader(
        std::vector<char>::const_iterator data,
        const std::vector<char>::const_iterator dataEnd,
        std::vector<char>::const_iterator& name,
        std::vector<char>::const_iterator& value,
        std::vector<char>::const_iterator& end)
{
    size_t nameSize;
    size_t valueSize;

    if(data>=dataEnd)
        return false;
    if(*data & 0x80)
    {
        const auto size=data;
        data += sizeof(uint32_t);

        if(data>dataEnd)
            return false;

        nameSize=BigEndian<uint32_t>::read(&*size) & 0x7fffffff;
    }
    else
        nameSize=*data++;

    if(data>=dataEnd)
        return false;
    if(*data & 0x80)
    {
        const auto size=data;
        data += sizeof(uint32_t);

        if(data>dataEnd)
            return false;

        valueSize=BigEndian<uint32_t>::read(&*size) & 0x7fffffff;
    }
    else
        valueSize=*data++;

    name = data;
    value = name+nameSize;
    end = value+valueSize;

    if(end>dataEnd)
        return false;
    else
        return true;
}

const Fastcgipp::Protocol::ManagementReply<14, 2>
Fastcgipp::Protocol::maxConnsReply("FCGI_MAX_CONNS", "10");

const Fastcgipp::Protocol::ManagementReply<13, 2>
Fastcgipp::Protocol::maxReqsReply("FCGI_MAX_REQS", "50");

const Fastcgipp::Protocol::ManagementReply<15, 1>
Fastcgipp::Protocol::mpxsConnsReply("FCGI_MPXS_CONNS", "1");

const char Fastcgipp::version[]=FASTCGIPP_VERSION;
