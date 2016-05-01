/*!
 * @file       fcgistream.cpp
 * @brief      Declares the fastcgi++ stream
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       May 1, 2016
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

#include "fastcgi++/fcgistream.hpp"
#include "fastcgi++/log.hpp"

#include <codecvt>
#include <algorithm>

namespace Fastcgipp
{
    template <>
    bool Fastcgipp::Fcgibuf<wchar_t, std::char_traits<wchar_t>>::emptyBuffer()
    {
        const std::codecvt_utf8<char_type> converter;
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
                ERROR_LOG("Fcgibuf code conversion failed")
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
    bool Fastcgipp::Fcgibuf<char, std::char_traits<char>>::emptyBuffer()
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

    template <>
    const std::map<char, const std::basic_string<char>>
    Fastcgipp::Fcgibuf<char, std::char_traits<char>>::htmlCharacters
    {
        std::make_pair('"', "&quot;"),
        std::make_pair('>', "&gt;"),
        std::make_pair('<', "&lt;"),
        std::make_pair('&', "&amp;"),
        std::make_pair(0x27, "&apos;")
    };

    template <>
    const std::map<wchar_t, const std::basic_string<wchar_t>>
    Fastcgipp::Fcgibuf<wchar_t, std::char_traits<wchar_t>>::htmlCharacters
    {
        std::make_pair('"', L"&quot;"),
        std::make_pair('>', L"&gt;"),
        std::make_pair('<', L"&lt;"),
        std::make_pair('&', L"&amp;"),
        std::make_pair(0x27, L"&apos;")
    };

    template <>
    const std::map<char, const std::basic_string<char>>
    Fastcgipp::Fcgibuf<char, std::char_traits<char>>::urlCharacters
	{
        std::make_pair('!', "%21"),
        std::make_pair(']', "%5D"),
        std::make_pair('[', "%5B"),
        std::make_pair('#', "%23"),
        std::make_pair('?', "%3F"),
        std::make_pair('/', "%2F"),
        std::make_pair(',', "%2C"),
        std::make_pair('$', "%24"),
        std::make_pair('+', "%2B"),
        std::make_pair('=', "%3D"),
        std::make_pair('&', "%26"),
        std::make_pair('@', "%40"),
        std::make_pair(':', "%3A"),
        std::make_pair(';', "%3B"),
        std::make_pair(')', "%29"),
        std::make_pair('(', "%28"),
        std::make_pair(0x27, "%27"),
        std::make_pair('*', "%2A"),
        std::make_pair('<', "%3C"),
        std::make_pair('>', "%3E"),
        std::make_pair('"', "%22"),
        std::make_pair(' ', "%20"),
        std::make_pair('%', "%25")
	};

    template <>
    const std::map<wchar_t, const std::basic_string<wchar_t>>
    Fastcgipp::Fcgibuf<wchar_t, std::char_traits<wchar_t>>::urlCharacters
	{
        std::make_pair('!', L"%21"),
        std::make_pair(']', L"%5D"),
        std::make_pair('[', L"%5B"),
        std::make_pair('#', L"%23"),
        std::make_pair('?', L"%3F"),
        std::make_pair('/', L"%2F"),
        std::make_pair(',', L"%2C"),
        std::make_pair('$', L"%24"),
        std::make_pair('+', L"%2B"),
        std::make_pair('=', L"%3D"),
        std::make_pair('&', L"%26"),
        std::make_pair('@', L"%40"),
        std::make_pair(':', L"%3A"),
        std::make_pair(';', L"%3B"),
        std::make_pair(')', L"%29"),
        std::make_pair('(', L"%28"),
        std::make_pair(0x27, L"%27"),
        std::make_pair('*', L"%2A"),
        std::make_pair('<', L"%3C"),
        std::make_pair('>', L"%3E"),
        std::make_pair('"', L"%22"),
        std::make_pair(' ', L"%20"),
        std::make_pair('%', L"%25")
	};
}

template Fastcgipp::Fcgibuf<char, std::char_traits<char>>::int_type
Fastcgipp::Fcgibuf<char, std::char_traits<char>>::overflow(int_type c);
template Fastcgipp::Fcgibuf<wchar_t, std::char_traits<wchar_t>>::int_type
Fastcgipp::Fcgibuf<wchar_t, std::char_traits<wchar_t>>::overflow(int_type c);
template <class charT, class traits>
typename Fastcgipp::Fcgibuf<charT, traits>::int_type
Fastcgipp::Fcgibuf<charT, traits>::overflow(int_type c)
{
	if(!emptyBuffer())
		return traits_type::eof();
	if(!traits_type::eq_int_type(c, traits_type::eof()))
		return this->sputc(c);
	else
		return traits_type::not_eof(c);
}

template
std::streamsize Fastcgipp::Fcgibuf<char, std::char_traits<char> >::xsputn(
        const char_type *s,
        std::streamsize n);
template
std::streamsize Fastcgipp::Fcgibuf<wchar_t, std::char_traits<wchar_t> >::xsputn(
        const char_type *s,
        std::streamsize n);
template <class charT, class traits>
std::streamsize Fastcgipp::Fcgibuf<charT, traits>::xsputn(
        const char_type *s,
        std::streamsize n)
{
	const char_type* const end = s+n;

	while(true)
	{
        if(m_encoding == encoding::NONE)
        {
            const std::streamsize actual = std::min(
                    end-s,
                    this->epptr()-this->pptr());
            std::copy(s, s+actual, this->pptr());
            this->pbump(actual);
			s += actual;
        }
        else
        {
            const std::map<charT, const std::basic_string<charT>>* map;
            if(m_encoding == encoding::HTML)
                map = &htmlCharacters;
            else
                map = &urlCharacters;

            while(s<end)
            {
                const size_t writeSpace = this->epptr() - this->pptr();
                const auto mapping = map->find(*s);
                if(mapping == map->cend())
                {
                    if(writeSpace < 1)
                        break;
                    *this->pptr() = *s++;
                    this->pbump(1);
                }
                else
                {
                    if(writeSpace < mapping->second.size())
                        break;
                    std::copy(
                            mapping->second.cbegin(),
                            mapping->second.cend(),
                            this->pptr());
                    s += mapping->second.size();
                    this->pbump(mapping->second.size());
                }
            }
        }

		if(s<end)
            break;
        else
			emptyBuffer();
	}

	return n;
}

/*






#include <cstring>
#include <algorithm>
#include <map>
#include <iterator>
#include <boost/iostreams/code_converter.hpp>

#include "fastcgi++/fcgistream.hpp"
#include "utf8_codecvt.hpp"

template<typename charT> template<typename Sink> std::streamsize Fastcgipp::Fcgistream<charT>::Encoder::write(Sink& dest, const charT* s, std::streamsize n)
{
	const static std::map<charT, std::basic_string<charT>> htmlCharacters
    {
        std::make_pair('"', 
    };
	if(!htmlCharacters.size())
	{
		const char quot[]="&quot;";
		std::copy(quot, quot+sizeof(quot)-1, std::back_inserter(htmlCharacters['"']));

		const char gt[]="&gt;";
		std::copy(gt, gt+sizeof(gt)-1, std::back_inserter(htmlCharacters['>']));

		const char lt[]="&lt;";
		std::copy(lt, lt+sizeof(lt)-1, std::back_inserter(htmlCharacters['<']));

		const char amp[]="&amp;";
		std::copy(amp, amp+sizeof(amp)-1, std::back_inserter(htmlCharacters['&']));

		const char apos[]="&apos;";
		std::copy(apos, apos+sizeof(apos)-1, std::back_inserter(htmlCharacters['\'']));
	}

	static std::map<charT, std::basic_string<charT> > urlCharacters;
	if(!urlCharacters.size())
	{
		const char exclaim[]="%21";
		std::copy(exclaim, exclaim+sizeof(exclaim)-1, std::back_inserter(urlCharacters['!']));

		const char rightbrac[]="%5D";
		std::copy(rightbrac, rightbrac+sizeof(rightbrac)-1, std::back_inserter(urlCharacters[']']));

		const char leftbrac[]="%5B";
		std::copy(leftbrac, leftbrac+sizeof(leftbrac)-1, std::back_inserter(urlCharacters['[']));

		const char number[]="%23";
		std::copy(number, number+sizeof(number)-1, std::back_inserter(urlCharacters['#']));

		const char question[]="%3F";
		std::copy(question, question+sizeof(question)-1, std::back_inserter(urlCharacters['?']));

		const char slash[]="%2F";
		std::copy(slash, slash+sizeof(slash)-1, std::back_inserter(urlCharacters['/']));

		const char comma[]="%2C";
		std::copy(comma, comma+sizeof(comma)-1, std::back_inserter(urlCharacters[',']));

		const char money[]="%24";
		std::copy(money, money+sizeof(money)-1, std::back_inserter(urlCharacters['$']));

		const char plus[]="%2B";
		std::copy(plus, plus+sizeof(plus)-1, std::back_inserter(urlCharacters['+']));

		const char equal[]="%3D";
		std::copy(equal, equal+sizeof(equal)-1, std::back_inserter(urlCharacters['=']));

		const char andsym[]="%26";
		std::copy(andsym, andsym+sizeof(andsym)-1, std::back_inserter(urlCharacters['&']));

		const char at[]="%40";
		std::copy(at, at+sizeof(at)-1, std::back_inserter(urlCharacters['@']));

		const char colon[]="%3A";
		std::copy(colon, colon+sizeof(colon)-1, std::back_inserter(urlCharacters[':']));

		const char semi[]="%3B";
		std::copy(semi, semi+sizeof(semi)-1, std::back_inserter(urlCharacters[';']));

		const char rightpar[]="%29";
		std::copy(rightpar, rightpar+sizeof(rightpar)-1, std::back_inserter(urlCharacters[')']));

		const char leftpar[]="%28";
		std::copy(leftpar, leftpar+sizeof(leftpar)-1, std::back_inserter(urlCharacters['(']));

		const char apos[]="%27";
		std::copy(apos, apos+sizeof(apos)-1, std::back_inserter(urlCharacters['\'']));

		const char star[]="%2A";
		std::copy(star, star+sizeof(star)-1, std::back_inserter(urlCharacters['*']));

		const char lt[]="%3C";
		std::copy(lt, lt+sizeof(lt)-1, std::back_inserter(urlCharacters['<']));

		const char gt[]="%3E";
		std::copy(gt, gt+sizeof(gt)-1, std::back_inserter(urlCharacters['>']));

		const char quot[]="%22";
		std::copy(quot, quot+sizeof(quot)-1, std::back_inserter(urlCharacters['"']));

		const char space[]="%20";
		std::copy(space, space+sizeof(space)-1, std::back_inserter(urlCharacters[' ']));

		const char percent[]="%25";
		std::copy(percent, percent+sizeof(percent)-1, std::back_inserter(urlCharacters['%']));
	}

	if(m_state==NONE)
		boost::iostreams::write(dest, s, n);
	else
	{
		std::map<charT, std::basic_string<charT> >* characters;
		switch(m_state)
		{
			case HTML:
				characters = &htmlCharacters;
				break;
			case URL:
				characters = &urlCharacters;
				break;
		}

		const charT* start=s;
		typename std::map<charT, std::basic_string<charT> >::const_iterator it;
		for(const charT* i=s; i < s+n; ++i)
		{
			it=characters->find(*i);
			if(it!=characters->end())
			{
				if(start<i) boost::iostreams::write(dest, start, start-i);
				boost::iostreams::write(dest, it->second.data(), it->second.size());
				start=i+1;
			}
		}
		int size=s+n-start;
		if(size) boost::iostreams::write(dest, start, size);
	}
	return n;
}

std::streamsize Fastcgipp::FcgistreamSink::write(const char* s, std::streamsize n)
{
	using namespace std;
	using namespace Protocol;
	const std::streamsize totalUsed=n;
	while(1)
	{{
		if(!n)
			break;

		int remainder=n%chunkSize;
		size_t size=n+sizeof(Header)+(remainder?(chunkSize-remainder):remainder);
		if(size>numeric_limits<uint16_t>::max()) size=numeric_limits<uint16_t>::max();
		Block dataBlock(m_transceiver->requestWrite(size));
		size=(dataBlock.size/chunkSize)*chunkSize;

		uint16_t contentLength=std::min(size-sizeof(Header), size_t(n));
		memcpy(dataBlock.data+sizeof(Header), s, contentLength);

		s+=contentLength;
		n-=contentLength;

		uint8_t contentPadding=chunkSize-contentLength%chunkSize;
		if(contentPadding==8) contentPadding=0;
		
		Header& header=*(Header*)dataBlock.data;
		header.setVersion(Protocol::version);
		header.setType(m_type);
		header.setRequestId(m_id.fcgiId);
		header.setContentLength(contentLength);
		header.setPaddingLength(contentPadding);

		m_transceiver->secureWrite(size, m_id, false);	
	}}
	return totalUsed;
}

void Fastcgipp::FcgistreamSink::dump(std::basic_istream<char>& stream)
{
	const size_t bufferSize=32768;
	char buffer[bufferSize];

	while(stream.good())
	{
		stream.read(buffer, bufferSize);
		write(buffer, stream.gcount());
	}
}

template<typename T, typename toChar, typename fromChar> T& fixPush(boost::iostreams::filtering_stream<boost::iostreams::output, fromChar>& stream, const T& t, int buffer_size)
{
	stream.push(t, buffer_size);
	return *stream.template component<T>(stream.size()-1);
}

template<> Fastcgipp::FcgistreamSink& fixPush<Fastcgipp::FcgistreamSink, char, wchar_t>(boost::iostreams::filtering_stream<boost::iostreams::output, wchar_t>& stream, const Fastcgipp::FcgistreamSink& t, int buffer_size)
{
	stream.push(boost::iostreams::code_converter<Fastcgipp::FcgistreamSink, utf8CodeCvt::utf8_codecvt_facet>(t, buffer_size));
	return **stream.component<boost::iostreams::code_converter<Fastcgipp::FcgistreamSink, utf8CodeCvt::utf8_codecvt_facet> >(stream.size()-1);
}


template Fastcgipp::Fcgistream<char>::Fcgistream();
template Fastcgipp::Fcgistream<wchar_t>::Fcgistream();
template<typename charT> Fastcgipp::Fcgistream<charT>::Fcgistream():
	m_encoder(fixPush<Encoder, charT, charT>(*this, Encoder(), 0)),
	m_sink(fixPush<FcgistreamSink, char, charT>(*this, FcgistreamSink(), 8192))
{}

template std::basic_ostream<char, std::char_traits<char> >& Fastcgipp::operator<< <char, std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >& os, const encoding& enc);
template std::basic_ostream<wchar_t, std::char_traits<wchar_t> >& Fastcgipp::operator<< <wchar_t, std::char_traits<wchar_t> >(std::basic_ostream<wchar_t, std::char_traits<wchar_t> >& os, const encoding& enc);
template<class charT, class Traits> std::basic_ostream<charT, Traits>& Fastcgipp::operator<<(std::basic_ostream<charT, Traits>& os, const encoding& enc)
{
	try
	{
		Fcgistream<charT>& stream(dynamic_cast<Fcgistream<charT>&>(os));
		stream.setEncoding(enc.m_type);
	}
	catch(std::bad_cast& bc)
	{
	}

	return os;
}*/
