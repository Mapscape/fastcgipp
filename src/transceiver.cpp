//! \file transceiver.cpp Defines member functions for Fastcgipp::Transceiver
/***************************************************************************
* Copyright (C) 2016 Eddie Carle [eddie@isatec.ca]                         *
*                                                                          *
* This file is part of fastcgi++.                                          *
*                                                                          *
* fastcgi++ is free software: you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as  published   *
* by the Free Software Foundation, either version 3 of the License, or (at *
* your option) any later version.                                          *
*                                                                          *
* fastcgi++ is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or    *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public     *
* License for more details.                                                *
*                                                                          *
* You should have received a copy of the GNU Lesser General Public License *
* along with fastcgi++.  If not, see <http://www.gnu.org/licenses/>.       *
****************************************************************************/

#include <fastcgi++/transceiver.hpp>

void Fastcgipp::Transceiver::transmit()
{
    while(!m_sendBuffer.empty())
    {{
        SendBuffer::ReadBlock sendBlock(m_sendBuffer.requestRead());
        const size_t sent = sendBlock.m_socket.write(
                sendBlock.m_data,
                sendBlock.m_size);
        m_sendBuffer.free(sent);
        if(sent != sendBlock.m_size)
            break;
    }}
}

Fastcgipp::WriteBlock Fastcgipp::Transceiver::SendBuffer::requestWrite(
        size_t size)
{
    m_writeLock.lock();
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    return WriteBlock(
            m_write->m_end,
            std::min(
                size,
                (size_t)(
                    m_write->m_data.get()
                    +Chunk::size
                    -m_write->m_end)
                )
            );
}

void Fastcgipp::Transceiver::SendBuffer::commitWrite(
        size_t size,
        const Socket& socket,
        bool kill)
{
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    m_write->m_end += size;
    if(minWriteBlockSize>(m_write->m_data.get()+Chunk::size-m_write->m_end))
    {
        // We've got less than a minimum write block size left in this chunk
        ++m_write;
        if(m_write == m_chunks.end())
        {
            // We were on the last chunk!
            m_chunks.push_back(Chunk());
            --m_write;
        }
    }
    m_frames.push(Frame(size, kill, socket));

    m_writeLock.unlock();
}

void Fastcgipp::Transceiver::handler()
{
    transmit();
    Socket socket = m_listener.poll(true);
    receive(socket);
    cleanupReceiveBuffers();
}

void Fastcgipp::Transceiver::SendBuffer::free(size_t size)
{
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    m_read += size;
    if(m_write != m_chunks.begin() && m_read >= m_chunks.begin()->m_end)
    {
        // We have read everything out of the first chunk
        if(m_write == --m_chunks.end())
        {
            // We are currently writing to the last chunk so we'll need a new
            // chunk at the end of the buffer so let us move the fully read
            // first chunk to the end
            m_chunks.begin()->m_end = m_chunks.begin()->m_data.get();
            m_chunks.splice(
                    m_chunks.end(),
                    m_chunks,
                    m_chunks.begin());
        }
        else
        {
            // We don't need this first chunk at all anymore. Free it up.
            m_chunks.pop_front();
        }

        // Of course set the read pointer to the start of the next chunk
        m_read = m_chunks.begin()->m_data.get();
    }

    m_frames.front().m_size -= size;
    if(m_frames.front().m_size == 0)
    {
        // We've read out a full frames worth of data
        if(m_frames.front().m_close)
            m_frames.front().m_socket.close();

        m_frames.pop();
    }
}

Fastcgipp::Transceiver::Transceiver(
        const socket_t& socket,
        std::function<void(Protocol::RequestId, Message&&)> sendMessage):
    m_sendMessage(sendMessage),
    m_listener(socket)
{}

void Fastcgipp::Transceiver::cleanupReceiveBuffers()
{
    auto buffer=m_receiveBuffers.begin();

    while(buffer != m_receiveBuffers.end())
    {
        if(buffer->first.valid())
            ++buffer;
        else
            buffer = m_receiveBuffers.erase(buffer);
    }
}

void Fastcgipp::Transceiver::receive(Socket& socket)
{
    if(socket.valid())
    {
        Message& messageBuffer=m_receiveBuffers[socket].message;
        Protocol::Header& headerBuffer=m_receiveBuffers[socket].header;

        size_t actual;
        // Are we in the process of recieving some part of a frame?
        if(!messageBuffer.data)
        {
            // Are we recieving a partial header or new? We are using
            // messageBuffer.size to indicate where we are in the header's
            // reception.
            actual=socket.read(
                    (char*)&headerBuffer+messageBuffer.size,
                    sizeof(Protocol::Header)-messageBuffer.size);
            if(!socket.valid())
            {
                m_receiveBuffers.erase(socket);
                return;
            }

            messageBuffer.size += actual;

            if(messageBuffer.size != sizeof(Protocol::Header))
                return;

            messageBuffer.data.reset(new char[
                    sizeof(Protocol::Header)
                    +headerBuffer.contentLength
                    +headerBuffer.paddingLength]);
            std::copy(
                    (const char*)(&headerBuffer),
                    (const char*)(&headerBuffer)+sizeof(Protocol::Header),
                    messageBuffer.data.get());
        }

        const Protocol::Header& header=
            *(const Protocol::Header*)messageBuffer.data.get();
        const size_t needed=
            header.contentLength
            +header.paddingLength
            +sizeof(Protocol::Header)
            -messageBuffer.size;

        actual=socket.read(
                messageBuffer.data.get()+messageBuffer.size,
                needed);
        if(!socket.valid())
        {
            m_receiveBuffers.erase(socket);
            return;
        }
        messageBuffer.size += actual;

        // Did we recieve a full frame?
        if(actual==needed)
        {       
            m_sendMessage(
                    Protocol::RequestId(header.fcgiId, socket),
                    std::move(messageBuffer));
            messageBuffer.size=0;
            messageBuffer.data.reset();
        }
    }
}

Fastcgipp::Transceiver::SendBuffer::ReadBlock
Fastcgipp::Transceiver::SendBuffer::requestRead()
{
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    return ReadBlock(
            m_read,
            m_frames.front().m_size,
            m_frames.front().m_socket);
}
