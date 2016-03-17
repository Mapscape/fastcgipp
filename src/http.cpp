/*!
 * @file       http.cpp
 * @brief      Defines elements of the HTTP protocol
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 16, 2016
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

#include <locale>
#include <codecvt>
#include <utility>
#include <sstream>
#include <iomanip>

#include "fastcgi++/log.hpp"
#include "fastcgi++/http.hpp"


void Fastcgipp::Http::charToString(
        const char* start,
        const char* end,
        std::wstring& string)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    try
    {
        string = converter.from_bytes(start, end);
    }
    catch(const std::range_error& e)
    {
        WARNING_LOG("Error in code conversion to from utf8")
    }
}

template int Fastcgipp::Http::atoi<char>(const char* start, const char* end);
template int Fastcgipp::Http::atoi<wchar_t>(
        const wchar_t* start,
        const wchar_t* end);
template<class charT>
int Fastcgipp::Http::atoi(const charT* start, const charT* end)
{
    bool neg=false;
    if(*start=='-')
    {
        neg=true;
        ++start;
    }
    int result=0;
    for(; 0x30 <= *start && *start <= 0x39 && start<end; ++start)
        result=result*10+(*start&0x0f);

    return neg?-result:result;
}

template std::vector<char>::iterator Fastcgipp::Http::percentEscapedToRealBytes<
    std::vector<char>::iterator,
    std::vector<char>::const_iterator>(
        std::vector<char>::const_iterator start,
        std::vector<char>::const_iterator end,
        std::vector<char>::iterator destination);
template char* Fastcgipp::Http::percentEscapedToRealBytes<char*, const char*>(
        const char* start,
        const char* end,
        char* destination);
template<class InputIt, class OutputIt>
InputIt Fastcgipp::Http::percentEscapedToRealBytes(
        OutputIt start,
        OutputIt end,
        InputIt destination)
{
    enum State
    {
        NORMAL,
        DECODINGFIRST,
        DECODINGSECOND,
    } state = NORMAL;

    while(start != end)
    {
        if(state == NORMAL)
        {
            if(*start=='%')
            {
                *destination=0;
                state = DECODINGFIRST;
            }
            else if(*start=='+')
                *destination++=' ';
            else
                *destination++=*start;
        }
        else if(state == DECODINGFIRST)
        {
            if((*start|0x20) >= 'a' && (*start|0x20) <= 'f')
                *destination = ((*start|0x20)-0x57)<<4;
            else if(*start >= '0' && *start <= '9')
                *destination = (*start&0x0f)<<4;

            state = DECODINGSECOND;
        }
        else if(state == DECODINGSECOND)
        {
            if((*start|0x20) >= 'a' && (*start|0x20) <= 'f')
                *destination |= (*start|0x20)-0x57;
            else if(*start >= '0' && *start <= '9')
                *destination |= *start&0x0f;

            ++destination;
            state = NORMAL;
        }
        ++start;
    }
    return destination;
}

template void Fastcgipp::Http::Environment<char>::fill(
        const char* start,
        const char* end);
template void Fastcgipp::Http::Environment<wchar_t>::fill(
        const char* start,
        const char* end);
template<class charT> void Fastcgipp::Http::Environment<charT>::fill(
        const char* start,
        const char* end)
{
    size_t nameSize;
    size_t valueSize;
    const char* name;
    const char* value;
    while(Protocol::processParamHeader(
            start,
            end-start,
            name,
            nameSize,
            value,
            valueSize))
    {
        switch(nameSize)
        {
        case 9:
            if(!memcmp(name, "HTTP_HOST", 9))
                charToString(value, value+valueSize, host);
            else if(!memcmp(name, "PATH_INFO", 9))
            {
                std::unique_ptr<char> buffer(new char[valueSize]);
                int size=-1;
                for(
                        const char* source=value;
                        source<value+valueSize+1;
                        ++source, ++size)
                {
                    if(*source == '/' || source == value+valueSize)
                    {
                        if(size > 0)
                        {
                            percentEscapedToRealBytes(
                                    source-size,
                                    source,
                                    buffer.get());
                            pathInfo.push_back(std::basic_string<charT>());
                            charToString(
                                    buffer.get(),
                                    buffer.get()+size,
                                    pathInfo.back());
                        }
                        size=-1;
                    }
                }
            }
            break;
        case 11:
            if(!memcmp(name, "HTTP_ACCEPT", 11))
                charToString(value, value+valueSize, acceptContentTypes);
            else if(!memcmp(name, "HTTP_COOKIE", 11))
                decodeUrlEncoded(value, value+valueSize, cookies, ';');
            else if(!memcmp(name, "SERVER_ADDR", 11))
                serverAddress.assign(value, value+valueSize);
            else if(!memcmp(name, "REMOTE_ADDR", 11))
                remoteAddress.assign(value, value+valueSize);
            else if(!memcmp(name, "SERVER_PORT", 11))
                serverPort=atoi(value, value+valueSize);
            else if(!memcmp(name, "REMOTE_PORT", 11))
                remotePort=atoi(value, value+valueSize);
            else if(!memcmp(name, "SCRIPT_NAME", 11))
                charToString(value, value+valueSize, scriptName);
            else if(!memcmp(name, "REQUEST_URI", 11))
                charToString(value, value+valueSize, requestUri);
            break;
        case 12:
            if(!memcmp(name, "HTTP_REFERER", 12) && valueSize)
                charToString(value, value+valueSize, referer);
            else if(!memcmp(name, "CONTENT_TYPE", 12))
            {
                const char* end=(char*)memchr(value, ';', valueSize);
                charToString(
                        value,
                        end?end:value+valueSize,
                        contentType);
                if(end)
                {
                    const char* start=(char*)memchr(end, '=', valueSize-(end-value));
                    if(start)
                    {
                        boundary.resize(valueSize-(++start-value));
                        std::copy(
                                start,
                                start+boundary.size(),
                                boundary.begin());
                    }
                }
            }
            else if(!memcmp(name, "QUERY_STRING", 12) && valueSize)
                decodeUrlEncoded(value, value+valueSize, gets);
            break;
        case 13:
            if(!memcmp(name, "DOCUMENT_ROOT", 13))
                charToString(value, value+valueSize, root);
            break;
        case 14:
            if(!memcmp(name, "REQUEST_METHOD", 14))
            {
                requestMethod = RequestMethod::ERROR;
                switch(valueSize)
                {
                case 3:
                    if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::GET],
                                3)) requestMethod = RequestMethod::GET;
                    else if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::PUT],
                                3)) requestMethod = RequestMethod::PUT;
                    break;
                case 4:
                    if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::HEAD],
                                4)) requestMethod = RequestMethod::HEAD;
                    else if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::POST],
                                4)) requestMethod = RequestMethod::POST;
                    break;
                case 5:
                    if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::TRACE],
                                5)) requestMethod = RequestMethod::TRACE;
                    break;
                case 6:
                    if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::DELETE],
                                6)) requestMethod = RequestMethod::DELETE;
                    break;
                case 7:
                    if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::OPTIONS],
                                7)) requestMethod = RequestMethod::OPTIONS;
                    else if(!memcmp(
                                value,
                                requestMethodLabels[(int)RequestMethod::OPTIONS],
                                7)) requestMethod = RequestMethod::CONNECT;
                    break;
                }
            }
            else if(!memcmp(name, "CONTENT_LENGTH", 14))
                contentLength=atoi(value, value+valueSize);
            break;
        case 15:
            if(!memcmp(name, "HTTP_USER_AGENT", 15))
                charToString(value, value+valueSize, userAgent);
            else if(!memcmp(name, "HTTP_KEEP_ALIVE", 15))
                keepAlive=atoi(value, value+valueSize);
            break;
        case 18:
            if(!memcmp(name, "HTTP_IF_NONE_MATCH", 18))
                etag=atoi(value, value+valueSize);
            break;
        case 19:
            if(!memcmp(name, "HTTP_ACCEPT_CHARSET", 19))
                charToString(value, value+valueSize, acceptCharsets);
            break;
        case 20:
            if(!memcmp(name, "HTTP_ACCEPT_LANGUAGE", 20))
                charToString(value, value+valueSize, acceptLanguages);
            break;
        case 22:
            if(!memcmp(name, "HTTP_IF_MODIFIED_SINCE", 22))
            {
                std::stringstream dateStream;
                std::tm time;
                dateStream.write(value, valueSize);
                dateStream >> std::get_time(
                        &time,
                        "%a, %d %b %Y %H:%M:%S GMT");
                ifModifiedSince = std::mktime(&time);
            }
            break;
        }
        start = value+valueSize;
    }
}

template void Fastcgipp::Http::Environment<char>::fillPostBuffer(
        const char* data,
        size_t size);
template<class charT>
void Fastcgipp::Http::Environment<charT>::fillPostBuffer(
        const char* data,
        size_t size)
{
    if(!m_postBuffer.size())
        m_postBuffer.reserve(contentLength);

    const size_t oldSize = m_postBuffer.size();
    m_postBuffer.resize(oldSize+size);
    std::copy(
            data,
            data+oldSize,
            m_postBuffer.begin()+oldSize);
}

template bool Fastcgipp::Http::Environment<char>::parsePostBuffer();
template bool Fastcgipp::Http::Environment<wchar_t>::parsePostBuffer();
template<class charT> bool Fastcgipp::Http::Environment<charT>::parsePostBuffer()
{
    static const char multipart[] = "multipart/form-data";
    static const char urlEncoded[] = "application/x-www-form-urlencoded";

    if(!m_postBuffer.size())
        return true;

    bool parsed = false;

    if(std::equal(
                multipart,
                multipart+sizeof(multipart)-1,
                contentType.begin(),
                contentType.end()))
    {
        parsePostsMultipart();
        parsed = true;
    }
    else if(std::equal(
                urlEncoded,
                urlEncoded+sizeof(urlEncoded)-1,
                contentType.begin(),
                contentType.end()))
    {
        parsePostsUrlEncoded();
        parsed = true;
    }

    if(parsed)
    {
        clearPostBuffer();
        return true;
    }

    return false;
}

template void Fastcgipp::Http::Environment<char>::parsePostsMultipart();
template void Fastcgipp::Http::Environment<wchar_t>::parsePostsMultipart();
template<class charT>
void Fastcgipp::Http::Environment<charT>::parsePostsMultipart()
{
    static const std::string cName("name=\"");
    static const std::string cFilename("filename=\"");
    static const std::string cContentType("Content-Type: ");
    static const std::string cBody("\r\n\r\n");

    auto nameStart(m_postBuffer.cend());
    auto nameEnd(m_postBuffer.cend());
    auto filenameStart(m_postBuffer.cend());
    auto filenameEnd(m_postBuffer.cend());
    auto contentTypeStart(m_postBuffer.cend());
    auto contentTypeEnd(m_postBuffer.cend());
    auto bodyStart(m_postBuffer.cend());
    auto bodyEnd(m_postBuffer.cend());

    enum State
    {
        HEADER,
        NAME,
        FILENAME,
        CONTENT_TYPE,
        BODY
    } state=HEADER;

    for(auto byte = m_postBuffer.cbegin(); byte < m_postBuffer.cend(); ++byte)
    {
        switch(state)
        {
            case HEADER:
            {
                if(
                        nameEnd == m_postBuffer.cend() &&
                        size_t(m_postBuffer.cend()-byte) < cName.size() &&
                        std::equal(cName.begin(), cName.end(), byte))
                {
                    byte += cName.size()-1;
                    nameStart = byte+1;
                    state = NAME;
                }
                else if(
                        filenameEnd == m_postBuffer.cend() &&
                        size_t(m_postBuffer.cend()-byte) < cFilename.size() &&
                        std::equal(cFilename.begin(), cFilename.end(), byte))
                {
                    byte += cFilename.size()-1;
                    filenameStart = byte+1;
                    state = FILENAME;
                }
                else if(
                        contentTypeEnd == m_postBuffer.cend() &&
                        size_t(m_postBuffer.cend()-byte) < cContentType.size() &&
                        std::equal(cContentType.begin(), cContentType.end(), byte))
                {
                    byte += cContentType.size()-1;
                    contentTypeStart = byte+1;
                    state = CONTENT_TYPE;
                }
                else if(
                        bodyEnd == m_postBuffer.cend() &&
                        size_t(m_postBuffer.cend()-byte) < cBody.size() &&
                        std::equal(cBody.begin(), cBody.end(), byte))
                {
                    byte += cBody.size()-1;
                    bodyStart = byte+1;
                    state = BODY;
                }

                break;
            }

            case NAME:
            {
                if(*byte == '"')
                {
                    nameEnd=byte;
                    state=HEADER;
                }
                break;
            }

            case FILENAME:
            {
                if(*byte == '"')
                {
                    filenameEnd=byte;
                    state=HEADER;
                }
                break;
            }

            case CONTENT_TYPE:
            {
                if(*byte == '\r' || *byte == '\n')
                {
                    contentTypeEnd = byte--;
                    state=HEADER;
                }
                break;
            }

            case BODY:
            {
                if(
                        size_t(m_postBuffer.cend()-byte) < boundary.size() &&
                        std::equal(boundary.begin(), boundary.end(), byte))
                {
                    bodyEnd = byte-2;
                    if(bodyEnd<bodyStart)
                        bodyEnd = bodyStart;
                    else if(
                            bodyEnd-bodyStart>=2
                            && *(bodyEnd-1)=='\n'
                            && *(bodyEnd-2)=='\r')
                        bodyEnd -= 2;
                    const size_t bodySize = size_t(bodyEnd-bodyStart);

                    if(nameEnd != m_postBuffer.cend())
                    {
                        std::basic_string<charT> name;
                        charToString(&*nameStart, &*nameEnd, name);

                        if(contentTypeEnd != m_postBuffer.cend())
                        {
                            File<charT> file;
                            charToString(
                                    &*contentTypeStart,
                                    &*contentTypeEnd,
                                    file.contentType);
                            if(filenameEnd != m_postBuffer.cend())
                                charToString(
                                        &*filenameStart,
                                        &*filenameEnd,
                                        file.filename);

                            file.m_size=bodySize;
                            if(bodySize)
                            {
                                file.m_data.reset(new char[bodySize]);
                                std::copy(
                                        bodyStart,
                                        bodyEnd,
                                        file.m_data.get());
                            }
                            files.insert(std::make_pair(
                                        std::move(name),
                                        std::move(file)));
                        }
                        else
                        {
                            std::basic_string<charT> value;
                            charToString(&*bodyStart, &*bodyEnd, value);
                            posts.insert(std::make_pair(
                                        std::move(name),
                                        std::move(value)));
                        }
                    }

                    state=HEADER;
                    nameStart = m_postBuffer.cend();
                    nameEnd = m_postBuffer.cend();
                    filenameStart = m_postBuffer.cend();
                    filenameEnd = m_postBuffer.cend();
                    contentTypeStart = m_postBuffer.cend();
                    contentTypeEnd = m_postBuffer.cend();
                    bodyStart = m_postBuffer.cend();
                    bodyEnd = m_postBuffer.cend();
                }

                break;
            }
        }
    }
}

template void Fastcgipp::Http::Environment<char>::parsePostsUrlEncoded();
template void Fastcgipp::Http::Environment<wchar_t>::parsePostsUrlEncoded();
template<class charT>
void Fastcgipp::Http::Environment<charT>::parsePostsUrlEncoded()
{
    decodeUrlEncoded(&m_postBuffer.front(), &m_postBuffer.back()+1, posts);
}

bool Fastcgipp::Http::SessionId::s_seeded=false;

Fastcgipp::Http::SessionId::SessionId()
{
    if(!s_seeded)
    {
        std::srand(std::time(nullptr));
        s_seeded=true;
    }

    for(unsigned char& byte: m_data)
        byte=(unsigned char)(std::rand()%256);
    m_timestamp = std::time(nullptr);
}

template const Fastcgipp::Http::SessionId&
Fastcgipp::Http::SessionId::operator=<const char>(const char* data);
template const Fastcgipp::Http::SessionId&
Fastcgipp::Http::SessionId::operator=<const wchar_t>(const wchar_t* data);
template<class charT> const Fastcgipp::Http::SessionId&
Fastcgipp::Http::SessionId::operator=(charT* data)
{
    auto writeEnd = base64Decode(
            data,
            data+(size*4-1)/3+1,
            m_data.begin());
    m_timestamp = std::time(nullptr);
    return *this;
}

template void Fastcgipp::Http::decodeUrlEncoded<char>(
        const char* start,
        const char* end,
        std::multimap<
            std::basic_string<char>,
            std::basic_string<char>>& output,
        const char fieldSeperator);
template void Fastcgipp::Http::decodeUrlEncoded<wchar_t>(
        const char* start,
        const char* end,
        std::multimap<
            std::basic_string<wchar_t>,
            std::basic_string<wchar_t>>& output,
        const char fieldSeperator);
template<class charT> void Fastcgipp::Http::decodeUrlEncoded(
        const char* start,
        const char* end,
        std::multimap<
            std::basic_string<charT>,
            std::basic_string<charT>>& output,
        const char fieldSeperator)
{
    std::vector<char> data(start, end);

    auto nameStart(data.begin());
    auto nameEnd(data.end());
    auto valueStart(data.end());
    auto valueEnd(data.end());

    for(auto byte=data.begin(); byte != data.end(); ++byte)
    {
        if(nameEnd != data.end())
        {
            if(byte+1==data.end() || *byte==fieldSeperator)
            {
                if(*byte == fieldSeperator)
                    valueEnd = byte;
                else
                    valueEnd = byte+1;

                valueEnd=percentEscapedToRealBytes(
                        std::vector<char>::const_iterator(valueStart),
                        std::vector<char>::const_iterator(valueEnd),
                        valueStart);

                std::basic_string<charT> name;
                charToString(&*nameStart, &*nameEnd, name);
                nameStart=byte+1;
                nameEnd=data.end();

                std::basic_string<charT> value;
                charToString(&*valueStart, &*valueEnd, value);
                valueStart=data.end();
                valueEnd=data.end();

                output.insert(std::make_pair(
                            std::move(name),
                            std::move(value)));
            }
        }
        else
        {
            if(*byte == '=' && nameStart != data.end())
            {
                nameEnd=percentEscapedToRealBytes(
                        std::vector<char>::const_iterator(nameStart),
                        std::vector<char>::const_iterator(byte),
                        nameStart);
                valueStart=byte+1;
            }
        }
    }
}

extern const std::array<const char, 64> Fastcgipp::Http::base64Characters =
{{
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S',
    'T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l',
    'm','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4',
    '5','6','7','8','9','+','/'
}};

const std::array<const char* const, 9> Fastcgipp::Http::requestMethodLabels =
{
    "ERROR",
    "HEAD",
    "GET",
    "POST",
    "PUT",
    "DELETE",
    "TRACE",
    "OPTIONS",
    "CONNECT"
};

Fastcgipp::Http::Address& Fastcgipp::Http::Address::operator&=(
        const Address& x)
{
    *(uint64_t*)m_data.data() &= *(const uint64_t*)x.m_data.data();
    *(uint64_t*)(m_data.data()+8) &= *(const uint64_t*)(x.m_data.data()+8);

    return *this;
}

Fastcgipp::Http::Address Fastcgipp::Http::Address::operator&(
        const Address& x) const
{
    Address address(*this);
    address &= x;

    return address;
}

template void Fastcgipp::Http::Address::assign<char>(
        const char* start,
        const char* end);
template void Fastcgipp::Http::Address::assign<wchar_t>(
        const wchar_t* start,
        const wchar_t* end);
template<class charT> void Fastcgipp::Http::Address::assign(
        const charT* start,
        const charT* end)
{
    const charT* read=start-1;
    auto write=m_data.begin();
    auto pad=m_data.end();
    unsigned char offset;
    uint16_t chunk=0;
    bool error=false;

    while(1)
    {
        ++read;
        if(read >= end || *read == ':')
        {
            if(read == start || *(read-1) == ':')
            {
                if(pad!=m_data.end() && pad!=write)
                {
                    error=true;
                    break;
                }
                else
                    pad = write;
            }
            else
            {
                *write = (chunk&0xff00)>>8;
                *(write+1) = chunk&0x00ff;
                chunk = 0;
                write += 2;
                if(write>=m_data.end() || read>=end)
                {
                    if(read>=end && write<m_data.end() && pad==m_data.end())
                        error=true;
                    break;
                }
            }
            continue;
        }
        else if('0' <= *read && *read <= '9')
            offset = '0';
        else if('A' <= *read && *read <= 'F')
            offset = 'A'-10;
        else if('a' <= *read && *read <= 'f')
            offset = 'a'-10;
        else if(*read == '.')
        {
            if(write == m_data.begin())
            {
                // We must be getting a pure ipv4 formatted address. Not an
                // ::ffff:xxx.xxx.xxx.xxx style ipv4 address.
                *(uint16_t*)write = 0xffff;
                pad = m_data.begin();
                write += 2;
            }
            else if(write - m_data.begin() > 12)
            {
                // We don't have enought space for an ipv4 address
                error=true;
                break;
            }

            // First convert the value stored in chunk to the first part of the
            // ipv4 address
            *write = 0;
            for(int i=0; i<3; ++i)
            {
                *write = *write * 10 + ((chunk&0x0f00)>>8);
                chunk <<= 4;
            }
            ++write;

            // Now we'll get the remaining pieces
            for(int i=0; i<3 && read<end; ++i)
            {
                const charT* point=std::find(read, end, '.');
                if(point<end-1)
                    read=point;
                else
                {
                    error=true;
                    break;
                }
                *write++ = atoi(++read, end);
            }
            break;
        }
        else
        {
            error=true;
            break;
        }
        chunk <<= 4;
        chunk |= *read-offset;
    }



    if(error)
    {
        m_data.fill(0);
        WARNING_LOG("Error converting IPv6 address " \
                << std::wstring(start, end))
    }
    else if(pad != m_data.end())
    {
        if(pad==write)
            std::fill(write, m_data.end(), 0);
        else
        {
            auto padEnd=pad+(m_data.end()-write);
            std::copy(pad, padEnd, padEnd);
            std::fill(pad, padEnd, 0);
        }
    }
}

template std::basic_ostream<char, std::char_traits<char>>&
Fastcgipp::Http::operator<< <char, std::char_traits<char>>(
        std::basic_ostream<char, std::char_traits<char>>& os,
        const Address& address);
template std::basic_ostream<wchar_t, std::char_traits<wchar_t>>&
Fastcgipp::Http::operator<< <wchar_t, std::char_traits<wchar_t>>(
        std::basic_ostream<wchar_t, std::char_traits<wchar_t>>& os,
        const Address& address);
template<class charT, class Traits> std::basic_ostream<charT, Traits>&
Fastcgipp::Http::operator<<(
        std::basic_ostream<charT, Traits>& os,
        const Address& address)
{
    using namespace std;
    if(!os.good()) return os;

    try
    {
        typename basic_ostream<charT, Traits>::sentry opfx(os);
        if(opfx)
        {
            streamsize fieldWidth=os.width(0);
            charT buffer[40];
            charT* bufPtr=buffer;
            locale loc(os.getloc(), new num_put<charT, charT*>);

            const uint16_t* subStart=0;
            const uint16_t* subEnd=0;
            {
                const uint16_t* subStartCandidate;
                const uint16_t* subEndCandidate;
                bool inZero = false;

                for(const uint16_t* it = (const uint16_t*)address.m_data.data();
                        it < (const uint16_t*)(address.m_data.data()+Address::size);
                        ++it)
                {
                    if(*it == 0)
                    {
                        if(!inZero)
                        {
                            subStartCandidate = it;
                            subEndCandidate = it;
                            inZero=true;
                        }
                        ++subEndCandidate;
                    }
                    else if(inZero)
                    {
                        if(subEndCandidate-subStartCandidate > subEnd-subStart)
                        {
                            subStart=subStartCandidate;
                            subEnd=subEndCandidate-1;
                        }
                        inZero=false;
                    }
                }
                if(inZero)
                {
                    if(subEndCandidate-subStartCandidate > subEnd-subStart)
                    {
                        subStart=subStartCandidate;
                        subEnd=subEndCandidate-1;
                    }
                    inZero=false;
                }
            }

            ios_base::fmtflags oldFlags = os.flags();
            os.setf(ios::hex, ios::basefield);

            if(
                    subStart==(const uint16_t*)address.m_data.data()
                    && subEnd==(const uint16_t*)address.m_data.data()+4
                    && *((const uint16_t*)address.m_data.data()+5) == 0xffff)
            {
                // It is an ipv4 address
                *bufPtr++=os.widen(':');
                *bufPtr++=os.widen(':');
                bufPtr=use_facet<num_put<charT, charT*> >(loc).put(
                        bufPtr,
                        os,
                        os.fill(),
                        static_cast<unsigned long int>(0xffff));
                *bufPtr++=os.widen(':');
                os.setf(ios::dec, ios::basefield);

                for(
                        const unsigned char* it = address.m_data.data()+12;
                        it < address.m_data.data()+Address::size;
                        ++it)
                {
                    bufPtr=use_facet<num_put<charT, charT*> >(loc).put(
                            bufPtr,
                            os,
                            os.fill(),
                            static_cast<unsigned long int>(*it));
                    *bufPtr++=os.widen('.');
                }
                --bufPtr;
            }
            else
            {
                // It is an ipv6 address
                for(const uint16_t* it = (const uint16_t*)address.m_data.data();
                        it < (const uint16_t*)(
                            address.m_data.data()+Address::size);
                        ++it)
                {
                    if(subStart <= it && it <= subEnd)
                    {
                        if(
                                it == subStart 
                                && it == (const uint16_t*)address.m_data.data())
                            *bufPtr++=os.widen(':');
                        if(it == subEnd)
                            *bufPtr++=os.widen(':');
                    }
                    else
                    {
                        bufPtr=use_facet<num_put<charT, charT*> >(loc).put(
                                bufPtr,
                                os,
                                os.fill(),
                                static_cast<unsigned long int>(
                                    *(const Protocol::BigEndian<uint16_t>*)it));

                        if(it < (const uint16_t*)(address.m_data.data()+Address::size)-1)
                            *bufPtr++=os.widen(':');
                    }
                }
            }

            os.flags(oldFlags);

            charT* ptr=buffer;
            ostreambuf_iterator<charT,Traits> sink(os);
            if(os.flags() & ios_base::left)
                for(int i=max(fieldWidth, bufPtr-buffer); i>0; i--)
                {
                    if(ptr!=bufPtr) *sink++=*ptr++;
                    else *sink++=os.fill();
                }
            else
                for(int i=fieldWidth-(bufPtr-buffer); ptr!=bufPtr;)
                {
                    if(i>0) { *sink++=os.fill(); --i; }
                    else *sink++=*ptr++;
                }

            if(sink.failed()) os.setstate(ios_base::failbit);
        }
    }
    catch(bad_alloc&)
    {
        ios_base::iostate exception_mask = os.exceptions();
        os.exceptions(ios_base::goodbit);
        os.setstate(ios_base::badbit);
        os.exceptions(exception_mask);
        if(exception_mask & ios_base::badbit) throw;
    }
    catch(...)
    {
        ios_base::iostate exception_mask = os.exceptions();
        os.exceptions(ios_base::goodbit);
        os.setstate(ios_base::failbit);
        os.exceptions(exception_mask);
        if(exception_mask & ios_base::failbit) throw;
    }
    return os;
}

template std::basic_istream<char, std::char_traits<char>>&
Fastcgipp::Http::operator>> <char, std::char_traits<char>>(
        std::basic_istream<char, std::char_traits<char>>& is,
        Address& address);
template std::basic_istream<wchar_t, std::char_traits<wchar_t>>&
Fastcgipp::Http::operator>> <wchar_t, std::char_traits<wchar_t>>(
        std::basic_istream<wchar_t, std::char_traits<wchar_t>>& is,
        Address& address);
template<class charT, class Traits> std::basic_istream<charT, Traits>&
Fastcgipp::Http::operator>>(
        std::basic_istream<charT, Traits>& is,
        Address& address)
{
    using namespace std;
    if(!is.good()) return is;

    ios_base::iostate err = ios::goodbit;
    try
    {
        typename basic_istream<charT, Traits>::sentry ipfx(is);
        if(ipfx)
        {
            istreambuf_iterator<charT, Traits> read(is);
            unsigned char buffer[Address::size];
            unsigned char* write=buffer;
            unsigned char* pad=0;
            unsigned char offset;
            unsigned char count=0;
            uint16_t chunk=0;
            charT lastChar=0;

            for(;;++read)
            {
                if(++count>40)
                {
                    err = ios::failbit;
                    break;
                }
                else if('0' <= *read && *read <= '9')
                    offset = '0';
                else if('A' <= *read && *read <= 'F')
                    offset = 'A'-10;
                else if('a' <= *read && *read <= 'f')
                    offset = 'a'-10;
                else if(*read == '.')
                {
                    if(write == buffer)
                    {
                        // We must be getting a pure ipv4 formatted address. Not an ::ffff:xxx.xxx.xxx.xxx style ipv4 address.
                        *(uint16_t*)write = 0xffff;
                        pad = buffer;
                        write+=2;
                    }
                    else if(write - buffer > 12)
                    {
                        // We don't have enought space for an ipv4 address
                        err = ios::failbit;
                        break;
                    }

                    // First convert the value stored in chunk to the first part of the ipv4 address
                    *write = 0;
                    for(int i=0; i<3; ++i)
                    {
                        *write = *write * 10 + ((chunk&0x0f00)>>8);
                        chunk <<= 4;
                    }
                    ++write;

                    // Now we'll get the remaining pieces
                    for(int i=0; i<3; ++i)
                    {
                        if(*read != is.widen('.'))
                        {
                            err = ios::failbit;
                            break;
                        }
                        unsigned int value;
                        use_facet<num_get<charT, istreambuf_iterator<charT, Traits> > >(is.getloc()).get(++read, istreambuf_iterator<charT, Traits>(), is, err, value);
                        *write++ = value;
                    }
                    break;
                }
                else
                {
                    if(*read == ':' && (!lastChar || lastChar == ':'))
                    {
                        if(pad && pad != write)
                        {
                            err = ios::failbit;
                            break;
                        }
                        else
                            pad = write;
                    }
                    else
                    {
                        *write = (chunk&0xff00)>>8;
                        *(write+1) = chunk&0x00ff;
                        chunk = 0;
                        write += 2;
                        if(write>=buffer+Address::size)
                            break;
                        if(*read!=':')
                        {
                            if(!pad)
                                err = ios::failbit;
                            break;
                        }
                    }
                    lastChar=':';
                    continue;
                }
                chunk <<= 4;
                chunk |= *read-offset;
                lastChar=*read;

            }

            if(err == ios::goodbit)
            {
                if(pad)
                {
                    if(pad==write)
                        std::memset(write, 0, Address::size-(write-buffer));
                    else
                    {
                        const size_t padSize=buffer+Address::size-write;
                        std::memmove(pad+padSize, pad, write-pad);
                        std::memset(pad, 0, padSize);
                    }
                }
                address=buffer;
            }
            else
                is.setstate(err);
        }
    }
    catch(bad_alloc&)
    {
        ios_base::iostate exception_mask = is.exceptions();
        is.exceptions(ios_base::goodbit);
        is.setstate(ios_base::badbit);
        is.exceptions(exception_mask);
        if(exception_mask & ios_base::badbit) throw;
    }
    catch(...)
    {
        ios_base::iostate exception_mask = is.exceptions();
        is.exceptions(ios_base::goodbit);
        is.setstate(ios_base::failbit);
        is.exceptions(exception_mask);
        if(exception_mask & ios_base::failbit) throw;
    }

    return is;
}

Fastcgipp::Http::Address::operator bool() const
{
    static const std::array<const unsigned char, 16> nullString =
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    if(std::equal(m_data.begin(), m_data.end(), nullString.begin()))
        return false;
    return true;
}
