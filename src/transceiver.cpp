/*!
 * @file       transceiver.cpp
 * @brief      Defines the Fastcgipp::Transceiver class
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       May 18, 2016
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
#if FASTCGIPP_LOG_LEVEL > 3
            ++m_recordsSent;
#endif
            if(record->kill)
            {
                record->socket.close();
                m_receiveBuffers.erase(record->socket);
#if FASTCGIPP_LOG_LEVEL > 3
                ++m_connectionKillCount;
#endif
            }
        }
    }

    return true;
}

void Fastcgipp::Transceiver::handler()
{
    bool flushed=false;
    Socket socket;

    while(!m_terminate && !(m_stop && m_sockets.size()==0))
    {
        socket = m_sockets.poll(flushed);
        receive(socket);
        flushed = transmit();
    }
}

void Fastcgipp::Transceiver::stop()
{
    m_stop=true;
    m_sockets.accept(false);
}

void Fastcgipp::Transceiver::terminate()
{
    m_terminate=true;
    m_sockets.wake();
}

void Fastcgipp::Transceiver::start()
{
    m_stop=false;
    m_terminate=false;
    m_sockets.accept(true);
    if(!m_thread.joinable())
    {
        std::thread thread(&Fastcgipp::Transceiver::handler, this);
        m_thread.swap(thread);
    }
}

void Fastcgipp::Transceiver::join()
{
    if(m_thread.joinable())
    {
        m_thread.join();
    }
}

Fastcgipp::Transceiver::Transceiver(
        const std::function<void(Protocol::RequestId, Message&&)> sendMessage):
    m_sendMessage(sendMessage)
#if FASTCGIPP_LOG_LEVEL > 3
    ,m_connectionKillCount(0),
    m_connectionRDHupCount(0),
    m_recordsSent(0),
    m_recordsQueued(0),
    m_recordsReceived(0)
#endif
{
    DIAG_LOG("Transceiver::Transciever(): Initialized")
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
            buffer.resize(sizeof(Protocol::Header));

            const ssize_t read = socket.read(
                    buffer.data()+received,
                    buffer.size()-received);
            if(read<0)
            {
                cleanupSocket(socket);
                return;
            }
            received += read;
            if(received < sizeof(Protocol::Header))
            {
                buffer.resize(received);
                return;
            }
        }

        const size_t recordSize = sizeof(Protocol::Header)
            +((Protocol::Header*)buffer.data())->contentLength
            +((Protocol::Header*)buffer.data())->paddingLength;
        buffer.resize(recordSize);

        Protocol::Header& header = *(Protocol::Header*)buffer.data();

        const ssize_t read = socket.read(
                buffer.data()+received,
                buffer.size()-received);

        if(read<0)
        {
            cleanupSocket(socket);
            return;
        }
        received += read;
        if(received < recordSize)
        {
            buffer.resize(received);
            return;
        }

        Message message;
        message.data.swap(buffer);

        m_sendMessage(
                Protocol::RequestId(header.fcgiId, socket),
                std::move(message));
#if FASTCGIPP_LOG_LEVEL > 3
        ++m_recordsReceived;
#endif
    }
}

void Fastcgipp::Transceiver::cleanupSocket(const Socket& socket)
{
    m_receiveBuffers.erase(socket);
    m_sendMessage(
            Fastcgipp::Protocol::RequestId(Protocol::badFcgiId, socket),
            Message());
    socket.close();
#if FASTCGIPP_LOG_LEVEL > 3
    ++m_connectionRDHupCount;
#endif
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
#if FASTCGIPP_LOG_LEVEL > 3
    ++m_recordsQueued;
#endif
}

Fastcgipp::Transceiver::~Transceiver()
{
    terminate();
    DIAG_LOG("Transceiver::~Transceiver(): Locally closed sockets ==== " \
            << m_connectionKillCount)
    DIAG_LOG("Transceiver::~Transceiver(): Remotely closed sockets === " \
            << m_connectionRDHupCount)
    DIAG_LOG("Transceiver::~Transceiver(): Remaining receive buffers = " \
            << m_receiveBuffers.size())
    DIAG_LOG("Transceiver::~Transceiver(): Records queued === " \
            << m_recordsQueued)
    DIAG_LOG("Transceiver::~Transceiver(): Records sent ===== " \
            << m_recordsSent)
    DIAG_LOG("Transceiver::~Transceiver(): Records received = " \
            << m_recordsReceived)
}
