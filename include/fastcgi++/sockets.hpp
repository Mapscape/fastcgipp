/*!
 * @file       sockets.hpp
 * @brief      Declares everything for interfaces with OS level sockets.
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 4, 2016
 * @copyright  Copyright &copy; 2016 Eddie Carle. This project is released under
 *             the GNU General Public License Version 3.
 *
 * It is this file, along with sockets.cpp, that must be modified to make
 * fastcgi++ work on any non-linux operating system. Although not completely
 * sure, I believe all that needs to be modified in this file is
 * Fastcgipp::socket_t and Fastcgipp::poll_t.
 */

/*******************************************************************************
* Copyright (C) 2016 Eddie Carle [eddie@isatec.ca]                             *
*                                                                              *
* This file is part of fastcgi++.                                              *
*                                                                              *
* fastcgi++ is free software: you can redistribute it and/or modify it under   *
* the terms of the GNU Lesser General Public License as  published by the Free *
* Software Foundation, either version 3 of the License, or (at your option) any*
* later version.                                                               *
*                                                                              *
* fastcgi++ is distributed in the hope that it will be useful, but WITHOUT ANY *
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR*
* A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more    *
* details.                                                                     *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with fastcgi++.  If not, see <http://www.gnu.org/licenses/>.           *
*******************************************************************************/

#ifndef SOCKETS_HPP
#define SOCKETS_HPP

#include <memory>
#include <map>
#include <mutex>

//! Topmost namespace for the fastcgi++ library
namespace Fastcgipp
{
    //! Our socket identifier type in GNU/Linux is simply an int
    typedef int socket_t;

    //! Our polling type in GNU/Linux is simply an int
    typedef int poll_t;

    class Listener;

    //! Class for representing an OS level I/O socket.
    /*!
     * It works together with the Listener class to establish all the
     * interfacing between the OS and the Transceiver class to communicate with
     * the outside world. The objects of this class represent individual
     * connections to the FastCGI server. They are consolidated and managed
     * within the Listener class.
     *
     * <em>No non-const member functions are thread safe. This means you can
     * only use valid() and the comparison operators across multiple threads.
     * </em>
     *
     * @date    March 4, 2016
     * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
     */
    class Socket
    {
    private:
        //! Our respective Listener needs private access.
        friend class Listener;

        //! Data structure to hold the shared socket data.
        struct Data
        {
            //! OS level socket identifier.
            const socket_t m_socket;

            //! Indicates whether or not the socket is dead/invalid.
            bool m_valid;

            //! Listener object this socket is tied to.
            Listener& m_listener;

            //! Sole constructor
            /*!
             * @param [inout] socket The OS level socket identifier to associate
             *                       with this.
             * @param [inout] listener The Listener object that created and is
             *                         consolidating this socket and it's peers.
             * @param [in] valid Set to false if this is a dead socket.
             *                   Sometimes it may be needed to create a socket
             *                   that is invalid off the start.
             */
            Data(
                    const socket_t& socket,
                    bool valid,
                    Listener& listener):
                m_socket(socket),
                m_valid(valid),
                m_listener(listener)
            {}

            Data() =delete;
            Data(const Data&) =delete;
        };

        //! Shared pointer to hold the socket data.
        const std::shared_ptr<Data> m_data;

        //! This is only true for a non-copy constructed object.
        bool m_original;

        //! Sole non-copy/move constructor
        /*!
         * This constructor is only accessible to the Listener class to create
         * new "original" sockets as they are accepted. Only sockets created
         * with this constructor will have m_original set to true.
         *
         * @param [inout] socket The OS level socket identifier to associate
         *                       with this.
         * @param [inout] listener The Listener object that created and is
         *                         consolidating this socket and it's peers.
         * @param [in] valid Set to false if this is a dead socket. Sometimes it
         *                   may be needed to create a socket that is invalid
         *                   off the start.
         */
        Socket(
                const socket_t& socket,
                Listener& listener,
                bool valid=true);

    public:
        //! Try and read a chunk of data out of the socket.
        /*!
         * This function will attempt to read the requested amount of data out
         * of the socket into the buffer. The return value indicates how many
         * bytes were actually read.
         *
         * If an error occurs during the read operation the socket is
         * closed/destroyed and marked invalid. A size of zero is returned in
         * case of an error but does not indicate an error. Zero could also be
         * returned if there is simply no data to read as this function is
         * non-blocking. To test for error simply use the valid() function.
         *
         * @param [out] buffer Pointer to memory location to which data should
         *                     be read into.
         * @param [in] size Maximum amount of data to read into the buffer.
         *                  Obviously this should be less than or equal to the
         *                  actual amount of memory allocated in the buffer.
         * @return Actual number of bytes read into the buffer.
         */
        size_t read(char* buffer, size_t size);

        //! Try and write a chunk of data into the socket.
        /*!
         * This function will attempt to write the requested amount of data into
         * the socket from the buffer. The return value indicates how many bytes
         * were actually written.
         *
         * If an error occurs during the write operation the socket is
         * closed/destroyed and marked invalid. A size of zero is returned in
         * case of an error but does not indicate an error. Zero could also be
         * returned if there is simply no room to write data as this function is
         * non-blocking. To test for error simply use the valid() function.
         *
         * @param [out] buffer Pointer to memory location to which data should
         *                     be written from.
         * @param [in] size Maximum amount of data to write from the buffer.
         * @return Actual number of bytes written from the buffer.
         */
        size_t write(const char* buffer, size_t size);

        //! We need this to allow the socket objects to be in sorted containers.
        bool operator<(const Socket& x) const
        {
            return m_data < x.m_data;
        }

        //! We need this to allow the socket objects to be in sorted containers.
        bool operator==(const Socket& x) const
        {
            return m_data == x.m_data;
        }

        //! Copy constructor
        /*!
         * Any sockets built using the copy constructor will not be marked
         * original.
         */
        Socket(const Socket& x):
            m_data(x.m_data),
            m_original(false)
        {}

        //! Move constructor
        /*!
         * This constructor serves the purposes of moving "original" sockets
         * into containers. The source socket has it's originality stripped and
         * moved to the destination.
         */
        Socket(Socket&& x):
            m_data(x.m_data),
            m_original(x.m_original)
        {
            x.m_original=false;
        }

        //! Calls close() on the socket if we are destructing the original
        ~Socket()
        {
            if(m_original)
                close();
        }

        //! Returns true if this socket is still open and capable of read/write.
        bool valid() const
        {
            return m_data->m_valid;
        }

        //! Call this to close the socket
        /*!
         * If the socket is valid, this will do the following:
         *  - Close/hangup the OS level socket.
         *  - Remove the socket from the associated Listener polling set.
         *  - Fully disassociate the socket with the Listener.
         *  - Mark the socket as invalid.
         *
         * If the socket is already invalid, calling this does nothing.
         */
        void close();

        //! Creates an invalid socket with no original.
        Socket();
    };

    //! Class for representing an OS level socket that listens for connections.
    /*!
     * It works together with the Socket class to establish all the interfacing
     * between the OS and the Transceiver class to communicate with the outside
     * world. The object of this class represents the socket that listens for
     * incoming connections to the FastCGI server. This class will create and
     * manage an array of Socket objects as new connections are initiated.
     *
     * <em>The only part of this class that is safe to call from multiple
     * threads is the wake() function.</em>
     *
     * @date    March 4, 2016
     * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
     */
    class Listener
    {
    public:
        //! Constructor
        /*!
         * Construct a Listener object based on an initial socket identifier to
         * listen on.
         *
         * @param[in] listen Socket identifier to listen for connections on
         */
        Listener(socket_t listen);

        //! Poll socket set for new incoming connections and data.
        /*!
         * Calling this will initiate a poll for both new connections and new
         * incoming data within the set of sockets that this object has created.
         * It will also keep an eye out for dead sockets and tear them down
         * accordingly.
         *
         * Its behaviour is as follows:
         *  - If a new connection arrives, it will establish the new connection.
         *  - If a socket is dead, it destroys and marks the socket as invalid.
         *    It will not return anything regarding the dead socket.
         *  - If new data has arrived in a currently active connection it will
         *    return the socket.
         *  - If the call has been set to non-blocking and no new data awaits, a
         *    generic invalid socket is returned.
         *
         * This function can be either blocking or non-blocking depending on the
         * boolean value passed to it. If the call is blocking it can be awoken
         * in a thread safe manner with a call to wake().
         *
         * @param[in] block Set \em true to make the call sleep and wait for new
         *                  data to arrive.
         * @return The socket for which there is new data waiting. Make sure to
         *         Socket::valid() on it to ensure validity.
         */
        Socket poll(bool block);

        //! Wake up from a nap inside poll()
        /*!
         * Calling this simply wakes up the execution thread that poll() is
         * blocking in. If there is no nap taking place, calling this does
         * nothing.
         *
         * This function is thread safe and can be called from anywhere as often
         * as is desired.
         */
        void wake();

    private:
        //! Our sockets need access to our private data
        friend class Socket;

        //! This is the file descriptor we listen for connections on
        const socket_t m_listen;

        //! Our poll object
        const poll_t m_poll;

        //! A pair of sockets for wakeup purposes
        socket_t m_wakeSockets[2];

        //! Set to true while sleeping
        bool m_sleeping;

        //! We need this mutex to thread safe the wake() function.
        std::mutex m_sleepingMutex;

        //! All the sockets
        std::map<socket_t, Socket> m_sockets;

        //! Accept a new connection and create it's socket
        inline void createSocket();
    };
}

#endif
