/*!
 * @file       http.cpp
 * @brief      Defines elements of the HTTP protocol
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

#include <locale>
#include <codecvt>
#include <utility>
#include <sstream>
#include <iomanip>
#include <random>

#include "fastcgi++/log.hpp"
#include "fastcgi++/http.hpp"


void Fastcgipp::Http::vecToString(
        std::vector<char>::const_iterator start,
        std::vector<char>::const_iterator end,
        std::wstring& string)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    try
    {
        string = converter.from_bytes(&*start, &*end);
    }
    catch(const std::range_error& e)
    {
        WARNING_LOG("Error in code conversion from utf8")
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

std::vector<char>::iterator Fastcgipp::Http::percentEscapedToRealBytes(
        std::vector<char>::const_iterator start,
        std::vector<char>::const_iterator end,
        std::vector<char>::iterator destination)
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
        std::vector<char>::const_iterator data,
        const std::vector<char>::const_iterator dataEnd);
template void Fastcgipp::Http::Environment<wchar_t>::fill(
        std::vector<char>::const_iterator data,
        const std::vector<char>::const_iterator dataEnd);
template<class charT> void Fastcgipp::Http::Environment<charT>::fill(
        std::vector<char>::const_iterator data,
        const std::vector<char>::const_iterator dataEnd)
{
    std::vector<char>::const_iterator name;
    std::vector<char>::const_iterator value;
    std::vector<char>::const_iterator end;

    while(Protocol::processParamHeader(
            data,
            dataEnd,
            name,
            value,
            end))
    {
        switch(value-name)
        {
        case 9:
            if(std::equal(name, value, "HTTP_HOST"))
                vecToString(value, end, host);
            else if(std::equal(name, value, "PATH_INFO"))
            {
                std::vector<char> buffer(end-value);
                int size=-1;
                for(
                        auto source=value;
                        source<=end;
                        ++source, ++size)
                {
                    if(*source == '/' || source == end)
                    {
                        if(size > 0)
                        {
                            const auto bufferEnd = percentEscapedToRealBytes(
                                    source-size,
                                    source,
                                    buffer.begin());
                            pathInfo.push_back(std::basic_string<charT>());
                            vecToString(
                                    buffer.cbegin(),
                                    bufferEnd,
                                    pathInfo.back());
                        }
                        size=-1;
                    }
                }
            }
            break;
        case 11:
            if(std::equal(name, value, "HTTP_ACCEPT"))
                vecToString(value, end, acceptContentTypes);
            else if(std::equal(name, value, "HTTP_COOKIE"))
                decodeUrlEncoded(value, end, cookies, ';');
            else if(std::equal(name, value, "SERVER_ADDR"))
                serverAddress.assign(&*value, &*end);
            else if(std::equal(name, value, "REMOTE_ADDR"))
                remoteAddress.assign(&*value, &*end);
            else if(std::equal(name, value, "SERVER_PORT"))
                serverPort=atoi(&*value, &*end);
            else if(std::equal(name, value, "REMOTE_PORT"))
                remotePort=atoi(&*value, &*end);
            else if(std::equal(name, value, "SCRIPT_NAME"))
                vecToString(value, end, scriptName);
            else if(std::equal(name, value, "REQUEST_URI"))
                vecToString(value, end, requestUri);
            break;
        case 12:
            if(std::equal(name, value, "HTTP_REFERER"))
                vecToString(value, end, referer);
            else if(std::equal(name, value, "CONTENT_TYPE"))
            {
                const auto semicolon = std::find(value, end, ';');
                vecToString(
                        value,
                        semicolon,
                        contentType);
                if(semicolon != end)
                {
                    const auto equals = std::find(semicolon, end, '=');
                    if(equals != end)
                        boundary.assign(
                                equals+1,
                                end);
                }
            }
            else if(std::equal(name, value, "QUERY_STRING"))
                decodeUrlEncoded(value, end, gets);
            break;
        case 13:
            if(std::equal(name, value, "DOCUMENT_ROOT"))
                vecToString(value, end, root);
            break;
        case 14:
            if(std::equal(name, value, "REQUEST_METHOD"))
            {
                requestMethod = RequestMethod::ERROR;
                switch(end-value)
                {
                case 3:
                    if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::GET]))
                        requestMethod = RequestMethod::GET;
                    else if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::PUT]))
                        requestMethod = RequestMethod::PUT;
                    break;
                case 4:
                    if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::HEAD]))
                        requestMethod = RequestMethod::HEAD;
                    else if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::POST]))
                        requestMethod = RequestMethod::POST;
                    break;
                case 5:
                    if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::TRACE]))
                        requestMethod = RequestMethod::TRACE;
                    break;
                case 6:
                    if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::DELETE]))
                        requestMethod = RequestMethod::DELETE;
                    break;
                case 7:
                    if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::OPTIONS]))
                        requestMethod = RequestMethod::OPTIONS;
                    else if(std::equal(
                                value,
                                end,
                                requestMethodLabels[(int)RequestMethod::OPTIONS]))
                        requestMethod = RequestMethod::CONNECT;
                    break;
                }
            }
            else if(std::equal(name, value, "CONTENT_LENGTH"))
                contentLength=atoi(&*value, &*end);
            break;
        case 15:
            if(std::equal(name, value, "HTTP_USER_AGENT"))
                vecToString(value, end, userAgent);
            else if(std::equal(name, value, "HTTP_KEEP_ALIVE"))
                keepAlive=atoi(&*value, &*end);
            break;
        case 18:
            if(std::equal(name, value, "HTTP_IF_NONE_MATCH"))
                etag=atoi(&*value, &*end);
            break;
        case 19:
            if(std::equal(name, value, "HTTP_ACCEPT_CHARSET"))
                vecToString(value, end, acceptCharsets);
            break;
        case 20:
            if(std::equal(name, value, "HTTP_ACCEPT_LANGUAGE"))
                vecToString(value, end, acceptLanguages);
            break;
        case 22:
            if(std::equal(name, value, "HTTP_IF_MODIFIED_SINCE"))
            {
                std::stringstream dateStream;
                std::tm time;
                dateStream.write(&*value, end-value);
                dateStream >> std::get_time(
                        &time,
                        "%a, %d %b %Y %H:%M:%S GMT");
                ifModifiedSince = std::mktime(&time);
            }
            break;
        }
        data = end;
    }
}

template void Fastcgipp::Http::Environment<char>::fillPostBuffer(
        const std::vector<char>::const_iterator start,
        const std::vector<char>::const_iterator end);
template void Fastcgipp::Http::Environment<wchar_t>::fillPostBuffer(
        const std::vector<char>::const_iterator start,
        const std::vector<char>::const_iterator end);
template<class charT>
void Fastcgipp::Http::Environment<charT>::fillPostBuffer(
        const std::vector<char>::const_iterator start,
        const std::vector<char>::const_iterator end)
{
    if(m_postBuffer.empty())
        m_postBuffer.reserve(contentLength);
    m_postBuffer.insert(m_postBuffer.end(), start, end);
}

template bool Fastcgipp::Http::Environment<char>::parsePostBuffer();
template bool Fastcgipp::Http::Environment<wchar_t>::parsePostBuffer();
template<class charT> bool Fastcgipp::Http::Environment<charT>::parsePostBuffer()
{
    static const std::string multipartStr("multipart/form-data");
    static const std::string urlEncodedStr("application/x-www-form-urlencoded");

    if(!m_postBuffer.size())
        return true;

    bool parsed = false;

    if(std::equal(
                multipartStr.cbegin(),
                multipartStr.cend(),
                contentType.cbegin(),
                contentType.cend()))
    {
        parsePostsMultipart();
        parsed = true;
    }
    else if(std::equal(
                urlEncodedStr.cbegin(),
                urlEncodedStr.cend(),
                contentType.cbegin(),
                contentType.cend()))
    {
        parsePostsUrlEncoded();
        parsed = true;
    }

    return parsed;
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
                const size_t bytesLeft = size_t(m_postBuffer.cend()-byte);

                if(
                        nameEnd == m_postBuffer.cend() &&
                        bytesLeft >= cName.size() &&
                        std::equal(cName.begin(), cName.end(), byte))
                {
                    byte += cName.size()-1;
                    nameStart = byte+1;
                    state = NAME;
                }
                else if(
                        filenameEnd == m_postBuffer.cend() &&
                        bytesLeft >= cFilename.size() &&
                        std::equal(cFilename.begin(), cFilename.end(), byte))
                {
                    byte += cFilename.size()-1;
                    filenameStart = byte+1;
                    state = FILENAME;
                }
                else if(
                        contentTypeEnd == m_postBuffer.cend() &&
                        bytesLeft >= cContentType.size() &&
                        std::equal(cContentType.begin(), cContentType.end(), byte))
                {
                    byte += cContentType.size()-1;
                    contentTypeStart = byte+1;
                    state = CONTENT_TYPE;
                }
                else if(
                        bodyEnd == m_postBuffer.cend() &&
                        bytesLeft >= cBody.size() &&
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
                const size_t bytesLeft = size_t(m_postBuffer.cend()-byte);

                if(
                        bytesLeft >= boundary.size() &&
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

                    if(nameEnd != m_postBuffer.cend())
                    {
                        std::basic_string<charT> name;
                        vecToString(nameStart, nameEnd, name);

                        if(contentTypeEnd != m_postBuffer.cend())
                        {
                            File<charT> file;
                            vecToString(
                                    contentTypeStart,
                                    contentTypeEnd,
                                    file.contentType);
                            if(filenameEnd != m_postBuffer.cend())
                                vecToString(
                                        filenameStart,
                                        filenameEnd,
                                        file.filename);

                            file.data.assign(bodyStart, bodyEnd);

                            files.insert(std::make_pair(
                                        std::move(name),
                                        std::move(file)));
                        }
                        else
                        {
                            std::basic_string<charT> value;
                            vecToString(bodyStart, bodyEnd, value);
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
    decodeUrlEncoded(m_postBuffer.cbegin(), m_postBuffer.cend(), posts);
}

Fastcgipp::Http::SessionId::SessionId()
{
    std::random_device device;
    std::uniform_int_distribution<unsigned short> distribution(0, 255);

    for(unsigned char& byte: m_data)
        byte = (unsigned char)distribution(device);
    m_timestamp = std::time(nullptr);
}

template Fastcgipp::Http::SessionId::SessionId<char>(
        const std::basic_string<char>& string);
template Fastcgipp::Http::SessionId::SessionId<wchar_t>(
        const std::basic_string<wchar_t>& string);
template<class charT> Fastcgipp::Http::SessionId::SessionId(
        const std::basic_string<charT>& string)
{
    base64Decode(
            string.begin(),
            string.begin()+std::min(stringLength, string.size()),
            m_data.begin());
    m_timestamp = std::time(nullptr);
}

const size_t Fastcgipp::Http::SessionId::stringLength;
const size_t Fastcgipp::Http::SessionId::size;

template void Fastcgipp::Http::decodeUrlEncoded<char>(
        std::vector<char>::const_iterator data,
        const std::vector<char>::const_iterator dataEnd,
        std::multimap<
            std::basic_string<char>,
            std::basic_string<char>>& output,
        const char fieldSeperator);
template void Fastcgipp::Http::decodeUrlEncoded<wchar_t>(
        std::vector<char>::const_iterator data,
        const std::vector<char>::const_iterator dataEnd,
        std::multimap<
            std::basic_string<wchar_t>,
            std::basic_string<wchar_t>>& output,
        const char fieldSeperator);
template<class charT> void Fastcgipp::Http::decodeUrlEncoded(
        std::vector<char>::const_iterator data,
        const std::vector<char>::const_iterator dataEnd,
        std::multimap<
            std::basic_string<charT>,
            std::basic_string<charT>>& output,
        const char fieldSeperator)
{
    std::vector<char> buffer(dataEnd-data);
    std::basic_string<charT> name;
    std::basic_string<charT> value;

    auto nameStart(data);
    auto nameEnd(dataEnd);
    auto valueStart(dataEnd);
    auto valueEnd(dataEnd);

    while(data != dataEnd)
    {
        if(nameEnd != dataEnd)
        {
            if(data+1==dataEnd || *data==fieldSeperator)
            {
                if(*data == fieldSeperator)
                    valueEnd = data;
                else
                    valueEnd = data+1;

                valueEnd=percentEscapedToRealBytes(
                        valueStart,
                        valueEnd,
                        buffer.begin());
                vecToString(buffer.cbegin(), valueEnd, value);

                output.insert(std::make_pair(
                            std::move(name),
                            std::move(value)));

                nameStart=data+1;
                nameEnd=dataEnd;
                valueStart=dataEnd;
                valueEnd=dataEnd;
            }
        }
        else
        {
            if(*data == '=')
            {
                nameEnd=percentEscapedToRealBytes(
                        nameStart,
                        data,
                        buffer.begin());
                vecToString(buffer.cbegin(), nameEnd, name);
                valueStart=data+1;
            }
        }
        ++data;
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
            std::copy(pad, write, padEnd);
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
