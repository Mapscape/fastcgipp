/*!
 * @file       transceiver.hpp
 * @brief      Defines the Fastcgipp::Transceiver class
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       April 24, 2016
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

#include "fastcgi++/transceiver.hpp"

#include "fastcgi++/log.hpp"
bool Fastcgipp::Transceiver::transmit()
{
    std::unique_ptr<Record> record;

    while(!m_sendBuffer.empty())
    {
        {
            std::lock_guard<std::mutex> lock(m_sendBufferMutex);
            record = std::move(m_sendBuffer.front());
            m_sendBuffer.pop_front();
        }

        const ssize_t sent = record->socket.write(
                &*record->read,
                record->data.cend()-record->read);
        if(sent>=0)
        {
            record->read += sent;
            if(record->read != record->data.cend())
            {
                {
                    std::lock_guard<std::mutex> lock(m_sendBufferMutex);
                    m_sendBuffer.push_front(std::move(record));
                }
                return false;
            }
            if(record->kill)
                record->socket.close();
        }
    }

    return true;
}

void Fastcgipp::Transceiver::handler()
{
    bool flushed=false;
    Socket socket;

    while(!(m_stop && m_sockets.size()==0))
    {
        socket = m_sockets.poll(flushed);
        receive(socket);
        flushed = transmit();
    }
}

void Fastcgipp::Transceiver::stop()
{
    std::lock_guard<std::mutex> lock(m_startStopMutex);
    if(m_thread.joinable())
    {
        m_stop=true;
        m_sockets.accept(false);
        m_thread.join();
        m_sockets.accept(true);
    }
}

void Fastcgipp::Transceiver::start()
{
    std::lock_guard<std::mutex> lock(m_startStopMutex);
    if(!m_thread.joinable())
    {
        m_stop=false;
        std::thread thread(&Fastcgipp::Transceiver::handler, this);
        m_thread.swap(thread);
    }
}

Fastcgipp::Transceiver::Transceiver(
        const std::function<void(Protocol::RequestId, Message&&)> sendMessage):
    m_sendMessage(sendMessage)
{
}

void Fastcgipp::Transceiver::receive(Socket& socket)
{
    if(socket.valid())
    {
        std::vector<char>& buffer=m_receiveBuffers[socket];
        size_t received = buffer.size();

        // Are we receiving a header?
        if(received < sizeof(Protocol::Header))
        {
            if(buffer.empty())
                buffer.reserve(sizeof(Protocol::Header));
            buffer.resize(sizeof(Fastcgipp::Protocol::Header));

            const ssize_t read = socket.read(
                    buffer.data()+received,
                    buffer.size()-received);
            if(read<0)
            {
                cleanupSocket(socket);
                return;
            }
            received += read;
            if(received < buffer.size())
            {
                buffer.resize(received);
                return;
            }
        }

        buffer.resize(
                +sizeof(Fastcgipp::Protocol::Header)
                +((Fastcgipp::Protocol::Header*)buffer.data())->contentLength
                +((Fastcgipp::Protocol::Header*)buffer.data())->paddingLength);
        buffer.reserve(buffer.size());

        Fastcgipp::Protocol::Header& header =
            *(Fastcgipp::Protocol::Header*)buffer.data();

        const ssize_t read = socket.read(
                buffer.data()+received,
                buffer.size()-received);

        if(read<0)
        {
            cleanupSocket(socket);
            return;
        }
        received += read;
        if(received < buffer.size())
        {
            buffer.resize(received);
            return;
        }

        Message message;
        message.data.swap(buffer);

        m_sendMessage(
                Protocol::RequestId(header.fcgiId, socket),
                std::move(message));
    }
    else
        cleanupSocket(socket);
}

void Fastcgipp::Transceiver::cleanupSocket(const Socket& socket)
{
    m_receiveBuffers.erase(socket);
    m_sendMessage(
            Fastcgipp::Protocol::RequestId(Protocol::badFcgiId, socket),
            Message());
}

void Fastcgipp::Transceiver::send(
        const Socket& socket,
        std::vector<char>&& data,
        bool kill)
{
    std::unique_ptr<Record> record(new Record(
                socket,
                std::move(data),
                kill));
    {
        std::lock_guard<std::mutex> lock(m_sendBufferMutex);
        m_sendBuffer.push_back(std::move(record));
    }
    m_sockets.wake();
}
