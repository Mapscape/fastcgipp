/*!
 * @file       fcgistreambuf.cpp
 * @brief      Defines the FcgiStreambuf class
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       May 6, 2016
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

#include "fastcgi++/fcgistreambuf.hpp"
#include "fastcgi++/log.hpp"

//#include <codecvt>
#include <algorithm>

namespace Fastcgipp
{
    template <> bool
    Fastcgipp::FcgiStreambuf<wchar_t, std::char_traits<wchar_t>>::emptyBuffer()
    {
        const codecvt_imp converter;
        std::codecvt_base::result result;
        std::vector<char> record;
        size_t count;
        mbstate_t state = mbstate_t();
        char* toNext;
        const char_type* fromNext;

        while((count = this->pptr() - this->pbase()) != 0)
        {
            record.resize(sizeof(Protocol::Header)
                    +std::min((size_t)0xffffU,
                        (count*converter.max_length()+Protocol::chunkSize-1)
                        /Protocol::chunkSize*Protocol::chunkSize));

            Protocol::Header& header = *(Protocol::Header*)record.data();

            result = converter.out(
                    state,
                    this->pbase(),
                    this->pptr(),
                    fromNext,
                    record.data()+sizeof(Protocol::Header),
                    &*record.end(),
                    toNext);

            if(result == std::codecvt_base::error
                    || result == std::codecvt_base::noconv)
            {
                ERROR_LOG("FcgiStreambuf code conversion failed")
                pbump(-count);
                return false;
            }
            pbump(-(fromNext - this->pbase()));
            record.resize(
                    (toNext-record.data()+Protocol::chunkSize-1)
                        /Protocol::chunkSize*Protocol::chunkSize);

            header.version = Protocol::version;
            header.type = m_type;
            header.fcgiId = m_id.m_id;
            header.contentLength =
                toNext-record.data()-sizeof(Protocol::Header);
            header.paddingLength =
                record.size()-header.contentLength-sizeof(Protocol::Header);

            send(m_id.m_socket, std::move(record));
        }

        return true;
    }

    template <>
    bool Fastcgipp::FcgiStreambuf<char, std::char_traits<char>>::emptyBuffer()
    {
        std::vector<char> record;
        size_t count;

        while((count = this->pptr() - this->pbase()) != 0)
        {
            record.resize(sizeof(Protocol::Header)
                    +std::min((size_t)0xffffU,
                        (count+Protocol::chunkSize-1)
                        /Protocol::chunkSize*Protocol::chunkSize));

            Protocol::Header& header = *(Protocol::Header*)record.data();
            header.contentLength = std::min(
                    count,
                    record.size()-sizeof(Protocol::Header));

            std::copy(
                    this->pbase(),
                    this->pbase()+header.contentLength,
                    record.begin()+sizeof(Protocol::Header));

            pbump(-header.contentLength);
            record.resize(
                    (header.contentLength
                        +sizeof(Protocol::Header)
                        +Protocol::chunkSize-1)
                    /Protocol::chunkSize*Protocol::chunkSize);

            header.version = Protocol::version;
            header.type = m_type;
            header.fcgiId = m_id.m_id;
            header.paddingLength =
                record.size()-header.contentLength-sizeof(Protocol::Header);

            send(m_id.m_socket, std::move(record));
        }

        return true;
    }
}

template Fastcgipp::FcgiStreambuf<char, std::char_traits<char>>::int_type
Fastcgipp::FcgiStreambuf<char, std::char_traits<char>>::overflow(int_type c);
template Fastcgipp::FcgiStreambuf<wchar_t, std::char_traits<wchar_t>>::int_type
Fastcgipp::FcgiStreambuf<wchar_t, std::char_traits<wchar_t>>::overflow(int_type c);
template <class charT, class traits>
typename Fastcgipp::FcgiStreambuf<charT, traits>::int_type
Fastcgipp::FcgiStreambuf<charT, traits>::overflow(int_type c)
{
    if(!emptyBuffer())
        return traits_type::eof();
    if(!traits_type::eq_int_type(c, traits_type::eof()))
        return this->sputc(c);
    else
        return traits_type::not_eof(c);
}

template
void Fastcgipp::FcgiStreambuf<char, std::char_traits<char>>::dump(
        const char* data,
        size_t size);
template
void Fastcgipp::FcgiStreambuf<wchar_t, std::char_traits<wchar_t>>::dump(
        const char* data,
        size_t size);
template <class charT, class traits>
void Fastcgipp::FcgiStreambuf<charT, traits>::dump(
        const char* data,
        size_t size)
{
    std::vector<char> record;

    emptyBuffer();

    while(size != 0)
    {
        record.resize(sizeof(Protocol::Header)
                +std::min((size_t)0xffffU,
                    (size+Protocol::chunkSize-1)
                    /Protocol::chunkSize*Protocol::chunkSize));

        Protocol::Header& header = *(Protocol::Header*)record.data();
        header.contentLength = std::min(
                size,
                record.size()-sizeof(Protocol::Header));

        std::copy(
                data,
                data+header.contentLength,
                record.begin()+sizeof(Protocol::Header));

        size -= header.contentLength;
        data += header.contentLength;
        record.resize(
                (header.contentLength
                    +sizeof(Protocol::Header)
                    +Protocol::chunkSize-1)
                /Protocol::chunkSize*Protocol::chunkSize);

        header.version = Protocol::version;
        header.type = m_type;
        header.fcgiId = m_id.m_id;
        header.paddingLength =
            record.size()-header.contentLength-sizeof(Protocol::Header);

        send(m_id.m_socket, std::move(record));
    }
}

template
void Fastcgipp::FcgiStreambuf<char, std::char_traits<char>>::dump(
        std::basic_istream<char>& stream);
template
void Fastcgipp::FcgiStreambuf<wchar_t, std::char_traits<wchar_t>>::dump(
        std::basic_istream<char>& stream);
template <class charT, class traits>
void Fastcgipp::FcgiStreambuf<charT, traits>::dump(
        std::basic_istream<char>& stream)
{
    std::vector<char> record;
    const ssize_t maxContentLength = 0xffff;

    emptyBuffer();

    do
    {
        record.resize(sizeof(Protocol::Header) + maxContentLength);

        Protocol::Header& header = *(Protocol::Header*)record.data();

        stream.read(record.data()+sizeof(Protocol::Header), maxContentLength);
        header.contentLength = stream.gcount();

        record.resize(
                (header.contentLength
                    +sizeof(Protocol::Header)
                    +Protocol::chunkSize-1)
                /Protocol::chunkSize*Protocol::chunkSize);

        header.version = Protocol::version;
        header.type = m_type;
        header.fcgiId = m_id.m_id;
        header.paddingLength =
            record.size()-header.contentLength-sizeof(Protocol::Header);

        send(m_id.m_socket, std::move(record));
    } while(stream.gcount() < maxContentLength);
}
