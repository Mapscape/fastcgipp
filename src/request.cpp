/*!
 * @file       request.cpp
 * @brief      Defines the Request class
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       May 15, 2016
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

    Protocol::Header& header=*(Protocol::Header*)record.data();
    header.version = Protocol::version;
    header.type = Protocol::RecordType::END_REQUEST;
    header.fcgiId = m_id.m_id;
    header.contentLength = sizeof(Protocol::EndRequest);
    header.paddingLength = 0;

    Protocol::EndRequest& body =
        *(Protocol::EndRequest*)(record.data()+sizeof(header));
    body.appStatus = 0;
    body.protocolStatus = m_status;

    m_send(m_id.m_socket, std::move(record), m_kill);
}

template std::unique_lock<std::mutex> Fastcgipp::Request<char>::handler();
template std::unique_lock<std::mutex> Fastcgipp::Request<wchar_t>::handler();
template<class charT>
std::unique_lock<std::mutex>Fastcgipp::Request<charT>::handler()
{
    std::unique_lock<std::mutex> lock(m_messagesMutex);
    while(!m_messages.empty())
    {
        Message message = std::move(m_messages.front());
        m_messages.pop();
        lock.unlock();

        if(message.type == 0)
        {
            const Protocol::Header& header =
                *(Protocol::Header*)message.data.data();
            const auto body = message.data.cbegin()+sizeof(header);
            const auto bodyEnd = body+header.contentLength;

            if(header.type == Protocol::RecordType::ABORT_REQUEST)
            {
                complete();
                goto exit;
            }

            if(header.type != m_state)
            {
                WARNING_LOG("Records received out of order from web server")
                errorHandler();
                goto exit;
            }

            switch(m_state)
            {
                case Protocol::RecordType::PARAMS:
                {
                    if(!(
                                role()==Protocol::Role::RESPONDER
                                || role()==Protocol::Role::AUTHORIZER))
                    {
                        m_status = Protocol::ProtocolStatus::UNKNOWN_ROLE;
                        WARNING_LOG("We got asked to do an unknown role")
                        errorHandler();
                        goto exit;
                    }

                    if(header.contentLength == 0)
                    {
                        if(m_maxPostSize
                                && environment().contentLength > m_maxPostSize)
                        {
                            bigPostErrorHandler();
                            goto exit;
                        }
                        m_state = Protocol::RecordType::IN;
                        lock.lock();
                        continue;
                    }
                    m_environment.fill(body,  bodyEnd);
                    lock.lock();
                    continue;
                }

                case Protocol::RecordType::IN:
                {
                    if(header.contentLength==0)
                    {
                        if(!inProcessor() && !m_environment.parsePostBuffer())
                        {
                            WARNING_LOG("Unknown content type from client")
                            errorHandler();
                            goto exit;
                        }

                        m_environment.clearPostBuffer();
                        m_state = Protocol::RecordType::OUT;
                        break;
                    }

                    m_environment.fillPostBuffer(body, bodyEnd);
                    inHandler(header.contentLength);
                    lock.lock();
                    continue;
                }

                default:
                {
                    ERROR_LOG("Our request is in a weird state.")
                    errorHandler();
                    goto exit;
                }
            }
        }

        m_message = std::move(message);
        if(response())
        {
            complete();
            break;
        }
        lock.lock();
    }
exit:
    return lock;
}

template void Fastcgipp::Request<char>::errorHandler();
template void Fastcgipp::Request<wchar_t>::errorHandler();
template<class charT> void Fastcgipp::Request<charT>::errorHandler()
{
    out << \
"Status: 500 Internal Server Error\n"\
"Content-Type: text/html; charset=utf-8\r\n\r\n"\
"<!DOCTYPE html>"\
"<html lang='en'>"\
    "<head>"\
        "<title>500 Internal Server Error</title>"\
    "</head>"\
    "<body>"\
        "<h1>500 Internal Server Error</h1>"\
    "</body>"\
"</html>";

    complete();
}

template void Fastcgipp::Request<char>::bigPostErrorHandler();
template void Fastcgipp::Request<wchar_t>::bigPostErrorHandler();
template<class charT> void Fastcgipp::Request<charT>::bigPostErrorHandler()
{
        out << \
"Status: 413 Request Entity Too Large\n"\
"Content-Type: text/html; charset=utf-8\r\n\r\n"\
"<!DOCTYPE html>"\
"<html lang='en'>"\
    "<head>"\
        "<title>413 Request Entity Too Large</title>"\
    "</head>"\
    "<body>"\
        "<h1>413 Request Entity Too Large</h1>"\
    "</body>"\
"</html>";

    complete();
}

template void Fastcgipp::Request<wchar_t>::configure(
        const Protocol::RequestId& id,
        const Protocol::Role& role,
        bool kill,
        const std::function<void(const Socket&, std::vector<char>&&, bool)> send,
        const std::function<void(Message)> callback);
template void Fastcgipp::Request<char>::configure(
        const Protocol::RequestId& id,
        const Protocol::Role& role,
        bool kill,
        const std::function<void(const Socket&, std::vector<char>&&, bool)> send,
        const std::function<void(Message)> callback);
template<class charT> void Fastcgipp::Request<charT>::configure(
        const Protocol::RequestId& id,
        const Protocol::Role& role,
        bool kill,
        const std::function<void(const Socket&, std::vector<char>&&, bool)> send,
        const std::function<void(Message)> callback)
{
    using namespace std::placeholders;

    m_kill=kill;
    m_id=id;
    m_role=role;
    m_callback=callback;
    m_send=send;

    m_outStreamBuffer.configure(
            id,
            Protocol::RecordType::OUT,
            std::bind(send, _1, _2, false));
    m_errStreamBuffer.configure(
            id,
            Protocol::RecordType::ERR,
            std::bind(send, _1, _2, false));
}
