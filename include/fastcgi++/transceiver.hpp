/*!
 * @file       transceiver.hpp
 * @brief      Declares the Fastcgipp::Transceiver class
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 6, 2016
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

#ifndef TRANSCEIVER_HPP
#define TRANSCEIVER_HPP

#include <map>
#include <list>
#include <queue>
#include <algorithm>
#include <map>
#include <functional>
#include <memory>
#include <mutex>

#include <fastcgi++/protocol.hpp>

//! Topmost namespace for the fastcgi++ library
namespace Fastcgipp
{
    //! A raw block of memory
    /*!
     * The purpose of this structure is to communicate a block of data to be
     * written to a Transceiver::SendBuffer
     */
    struct WriteBlock
    {
        //! Construct from a pointer and size
        /*!
         * @param[in] data Pointer to start of memory location
         * @param[in] size Size in bytes of memory location
         */
        WriteBlock(char* data, size_t size):
            m_data(data),
            m_size(size)
        {}

        //! Copies pointer and size, not data
        WriteBlock(const WriteBlock& block):
            m_data(block.m_data),
            m_size(block.m_size)
        {}

        //! Copies pointer and size, not data
        const WriteBlock& operator=(const WriteBlock& block)
        {
            m_data=block.m_data;
            m_size=block.m_size;
            return *this;
        }

        //! Pointer to start of memory location
        char* m_data;

        //! Size in bytes of memory location
        size_t m_size;
    };

    //! Handles low level communication with "the other side"
    /*!
     * This class handles the sending/receiving/buffering of data through the OS
     * level sockets and also the creation/destruction of the sockets
     * themselves.
     */
    class Transceiver
    {
    public:
        //! General transceiver handler
        /*!
         * This function is called by Manager::handler() to both transmit data
         * passed to it from requests and relay received data back to them as a
         * Message. The function will return true if there is nothing at all for
         * it to do.
         *
         * @return Boolean value indicating whether there is data to be
         *         transmitted or received
         */
        void handler();

        //! Request a write block in the buffer for transmission
        /*!
         * This is simply a direct interface to SendBuffer::requestWrite()
         *
         * @param[in] size Requested size of write block
         * @return WriteBlock of writable memory. Size may be less than
         *         requested.
         */
        WriteBlock requestWrite(size_t size)
        {
            return m_sendBuffer.requestWrite(size);
        }

        //! Secure a write in the buffer
        /*!
         * This is simply a direct interface to SendBuffer::commitWrite()
         *
         * @param[in] size Amount of bytes to commit
         * @param[in] socket Associated socket
         * @param[in] kill Boolean value indicating whether or not the file
         *                 descriptor should be closed after transmission
         */
        void commitWrite(
                size_t size,
                const Socket& socket,
                bool kill)
        {
            m_sendBuffer.commitWrite(size, socket, kill);
            m_listener.wake();
        }

        //! Constructor
        /*!
         * Construct a transceiver object based on an initial file descriptor to
         * listen on and a function to pass messages on to.
         *
         * @param[in] fd_ File descriptor to listen for connections on
         * @param[in] sendMessage_ Function to call to pass messages to requests
         */
        Transceiver(
                const socket_t& socket,
                std::function<void(Protocol::RequestId, Message&&)> sendMessage);

    private:
        //! Buffer type for receiving FastCGI records
        struct ReceiveBuffer
        {
            //! Buffer for header information
            Protocol::Header header;

            //! Buffer of complete Message
            Message message;
        };

        //! Buffer type for transmitting of FastCGI records
        /*!
         * This buffer is implemented as a circle of Chunk objects; the number
         * of which can grow and shrink as needed. Write space is requested with
         * requestWrite() which thereby returns a WriteBlock which may be smaller
         * than requested. The write is committed by calling commitWrite(). A
         * smaller space can be committed than was given to write on.
         *
         * All data written to the buffer has an associated file descriptor and
         * FastCGI ID through which it is flushed. This association with data is
         * managed through a queue of Frame objects.
         */
        class SendBuffer
        {
        private:
            //! %Frame of data associated with a specific socket
            struct Frame
            {
                //! Constructor
                /*!
                 * @param[in] size Size of the frame.
                 * @param[in] close Boolean value indicating whether or not the
                 *                  socket should be closed when the frame has
                 *                  been sent.
                 * @param[inout] socket Socket this frame should be sent out on.
                 */
                Frame(
                        size_t size,
                        bool close,
                        const Socket& socket):
                    m_size(size),
                    m_close(close),
                    m_socket(socket)
                {}

                //! Size of the frame
                size_t m_size;

                //! Should the socket be closed when the frame has been flushed?
                const bool m_close;

                //! Complete ID associated with the data frame
                Socket m_socket;
            };

            //! Queue of frames waiting to be transmitted
            std::queue<Frame> m_frames;

            //! Minimum WriteBlock size value that can be given from requestWrite()
            const static unsigned int minWriteBlockSize = 256;

            //! %Chunk of data in SendBuffer
            struct Chunk
            {
                //! Size of data section of the chunk
                const static unsigned int size = 131072;

                //! Actual chunk data
                std::unique_ptr<char> m_data;

                //! Pointer to the first write byte or 1+ the last read byte
                char* m_end;

                //! Creates a new data chunk
                Chunk():
                    m_data(new char[size]),
                    m_end(m_data.get())
                {}

                //! Creates a new chunk that takes the data of the old one
                Chunk(Chunk&& chunk):
                    m_data(std::move(chunk.m_data)),
                    m_end(m_data.get())
                {}
            };

            //! A list of chunks. Can contain from 2-infinity
            std::list<Chunk> m_chunks;

            //! Iterator pointing to the chunk currently used for writing
            std::list<Chunk>::iterator m_write;

            //! Current read spot in the buffer
            char* m_read;

            //! Only one write to the buffer at a time.
            std::mutex m_writeMutex;

            //! Only one write to the buffer at a time.
            std::unique_lock<std::mutex> m_writeLock;

            //! Needed to synchronize read/writes to the buffer
            std::mutex m_bufferMutex;

        public:
            SendBuffer():
                m_chunks(2),
                m_write(m_chunks.begin()),
                m_read(m_chunks.begin()->m_data.get()),
                m_writeLock(m_writeMutex, std::defer_lock)
            {}

            //! Request a write block in the buffer
            /*!
             * @param[in] size Requested size of write block
             * @return WriteBlock of writable memory. Size may be less than
             *         requested.
             */
            WriteBlock requestWrite(size_t size);

            //! Secure a write in the buffer
            /*!
             * @param[in] size Amount of bytes to commit.
             * @param[in] socket Associated socket.
             * @param[in] kill Boolean value indicating whether or not the file
             *                 descriptor should be closed after transmission.
             */
            void commitWrite(size_t size, const Socket& socket, bool kill);

            //! %Block of memory for extraction from SendBuffer
            struct ReadBlock
            {
                //! Constructor
                /*!
                 * @param[in] data Pointer to the first byte in the block
                 * @param[in] size Size in bytes of the data
                 * @param[in] fd %Socket the data should be written to
                 */
                ReadBlock(
                        const char* const data,
                        const size_t size,
                        Socket socket):
                    m_data(data),
                    m_size(size),
                    m_socket(socket)
                {}

                //! Create a new object that points to the same data as the old.
                ReadBlock(const ReadBlock& readBlock):
                    m_data(readBlock.m_data),
                    m_size(readBlock.m_size),
                    m_socket(readBlock.m_socket)
                {}

                //! Initializes an invalid block
                ReadBlock():
                    m_data(nullptr),
                    m_size(0),
                    m_socket(Socket())
                {}

                //! Pointer to the first byte in the block
                const char* const m_data;

                //! Size in bytes of the data
                const size_t m_size;

                //! Socket the data should be written to
                Socket m_socket;
            };

            //! Request a block of data for transmitting
            /*!
             * @return A block of data with a socket to write it to
             */
            inline ReadBlock requestRead();

            //! Mark data in the buffer as transmitted and free it's memory
            /*!
             * @param size Amount of bytes to mark as transmitted and free up
             */
            inline void free(size_t size);

            //! Test of the buffer is empty
            /*!
             * @return true if the buffer is empty
             */
            bool empty() const
            {
                return m_frames.empty();
            }
        };

        //! %Buffer for transmitting data
        SendBuffer m_sendBuffer;

        //! Function to call to pass messages to requests
        std::function<void(Protocol::RequestId, Message&&)> m_sendMessage;

        //! Listen for connections with this
        Listener m_listener;

        //! Container associating sockets with their receive buffers
        std::map<Socket, ReceiveBuffer> m_receiveBuffers;

        //! Transmit all buffered data possible
        inline void transmit();

        //! Remove receive buffers associated to dead sockets.
        inline void cleanupReceiveBuffers();

        //! Receive data on the specified socket.
        inline void receive(Socket& socket);
    };
}

#endif
