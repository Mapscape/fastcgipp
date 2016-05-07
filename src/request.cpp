/*!
 * @file       request.cpp
 * @brief      Defines the Request class
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

#include "fastcgi++/request.hpp"
#include "fastcgi++/log.hpp"

template void Fastcgipp::Request<char>::complete();
template void Fastcgipp::Request<wchar_t>::complete();
template<class charT> void Fastcgipp::Request<charT>::complete()
{
	out.flush();
	err.flush();

    std::vector<char> record(
            sizeof(Protocol::Header)+sizeof(Protocol::EndRequest));

	Header& header=*(Protocol::Header*)buffer.data;
	header.setVersion(Protocol::version);
	header.setType(Protocol::RecordType::END_REQUEST);
	header.setRequestId(m_id.fcgiId);
	header.setContentLength(sizeof(Protocol::EndRequest));
	header.setPaddingLength(0);
	
    Protocol::EndRequest& body = 
        *(Protocol::EndRequest*)(buffer.data+sizeof(Header));
	body.setAppStatus(0);
	body.setProtocolStatus(m_status);

    m_send(m_id.m_socket, std::move(record), m_kill);
}

template bool Fastcgipp::Request<char>::handler(Message&& message);
template bool Fastcgipp::Request<wchar_t>::handler(Message&& message);
template<class charT> bool Fastcgipp::Request<charT>::handler(Message&& message)
{
    if(message.type == 0)
    {
        const Protocol::Header& header=*(Protocol::Header*)message().data.get();
        const auto body = message.data.cbegin()+sizeof(header);
        const auto bodyEnd = bodyStart+header.contentLength;

        if(header.type != m_state)
        {
            WARNING_LOG("Records received out of order from web server")
            complete();
            return true;
        }


        switch(m_state)
        {
            case Protocol::RecordType::PARAMS:
            {
                if(!(role()==RESPONDER || role()==AUTHORIZER))
                {
                    m_status = Protocol::ProtocolStatus::UNKNOWN_ROLE;
                    WARNING_LOG("We got asked to do an unknown role")
                    complete();
                    return true;
                }

                if(header.contentLength == 0) 
                {
                    if(m_maxPostSize 
                            && environment().contentLength > m_maxPostSize)
                    {
                        bigPostErrorHandler();
                        complete();
                        return true;
                    }
                    m_state = Protocol::RecordType::IN;
                    return false;
                }
                m_environment.fill(body,  bodyEnd);
                return false;
            }

            case Protocol::RecordType::IN:
            {
                if(header.getContentLength()==0)
                {
                    if(!inProcessor() && !m_environment.parsePostBuffer())
                    {
                        WARNING_LOG("Unknown content type from client")
                        complete();
                        return true;
                    }

                    m_environment.clearPostBuffer();
                    m_state = Protocol::RecordType::OUT;
                    if(response())
                    {
                        complete();
                        return true;
                    }
                    return false;
                }

                m_environment.fillPostBuffer(body, bodyEnd);
                inHandler(header.getContentLength());
                return false;
            }

            case ABORT_REQUEST:
            {
                complete();
                return true;
            }
        }
    }
    else if(response())
    {
        
        m_message = std::move(message);
        complete();
        return true;
    }
}

template void Fastcgipp::Request<char>::errorHandler(const std::exception& error);
template void Fastcgipp::Request<wchar_t>::errorHandler(const std::exception& error);
template<class charT> void Fastcgipp::Request<charT>::errorHandler(const std::exception& error)
{
		out << \
"Status: 500 Internal Server Error\n"\
"Content-Type: text/html; charset=ISO-8859-1\r\n\r\n"\
"<!DOCTYPE html>"\
"<html lang='en'>"\
	"<head>"\
		"<title>500 Internal Server Error</title>"\
	"</head>"\
	"<body>"\
		"<h1>500 Internal Server Error</h1>"\
	"</body>"\
"</html>";

		err << '"' << error.what() << '"' << " from \"http://" << environment().host << environment().requestUri << "\" with a " << environment().requestMethod << " request method.";
}

template void Fastcgipp::Request<char>::bigPostErrorHandler();
template void Fastcgipp::Request<wchar_t>::bigPostErrorHandler();
template<class charT> void Fastcgipp::Request<charT>::bigPostErrorHandler()
{
		out << \
"Status: 413 Request Entity Too Large\n"\
"Content-Type: text/html; charset=ISO-8859-1\r\n\r\n"\
"<!DOCTYPE html>"\
"<html lang='en'>"\
	"<head>"\
		"<title>413 Request Entity Too Large</title>"\
	"</head>"\
	"<body>"\
		"<h1>413 Request Entity Too Large</h1>"\
	"</body>"\
"</html>";
}
