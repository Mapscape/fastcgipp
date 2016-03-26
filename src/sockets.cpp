/*!
 * @file       sockets.cpp
 * @brief      Defines everything for interfaces with OS level sockets.
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 26, 2016
 * @copyright  Copyright &copy; 2016 Eddie Carle. This project is released under
 *             the GNU Lesser General Public License Version 3.
 *
 * It is this file, along with sockets.hpp, that must be modified to make
 * fastcgi++ work on any non-linux operating system.
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

#include "fastcgi++/sockets.hpp"
#include "fastcgi++/log.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <cstring>

Fastcgipp::Socket::Socket(
        const socket_t& socket,
        SocketGroup& group,
        bool valid):
    m_data(new Data(socket, valid, group)),
    m_original(true)
{
    if(!group.pollAdd(socket))
    {
        ERROR_LOG("Unable to add socket " << socket << " to poll list: " \
                << std::strerror(errno))
        close();
    }
}

size_t Fastcgipp::Socket::read(char* buffer, size_t size)
{
    if(!valid())
        return 0;

    const ssize_t count = ::read(m_data->m_socket, buffer, size);
    if(count<0)
    {
        WARNING_LOG("Socket read() error on fd " \
                << m_data->m_socket << ": " << std::strerror(errno))
        close();
        return 0;
    }
    return (size_t)count;
}

size_t Fastcgipp::Socket::write(const char* buffer, size_t size)
{
    if(!valid())
        return 0;

    const ssize_t count = ::write(m_data->m_socket, buffer, size);
    if(count<0)
    {
        WARNING_LOG("Socket write() error on fd " \
                << m_data->m_socket << ": " << strerror(errno))
        close();
        return 0;
    }
    return (size_t)count;
}

void Fastcgipp::Socket::close()
{
    if(valid())
    {
        ::close(m_data->m_socket);
        m_data->m_valid = false;
        m_data->m_group.pollDel(m_data->m_socket);
        m_data->m_group.m_sockets.erase(m_data->m_socket);
    }
}

Fastcgipp::SocketGroup::SocketGroup():
    m_poll(epoll_create1(0)),
    m_sleeping(false)
{
    // Add our wakeup socket into the epoll list
    socketpair(AF_UNIX, SOCK_STREAM, 0, m_wakeSockets);
    pollAdd(m_wakeSockets[1]);
}

Fastcgipp::SocketGroup::~SocketGroup()
{
    close(m_poll);
    close(m_wakeSockets[0]);
    close(m_wakeSockets[1]);
    for(const auto& listener: m_listeners)
        close(listener);
}

bool Fastcgipp::SocketGroup::listen()
{
    const int listen=0;

    if(m_listeners.find(listen) == m_listeners.end())
    {
        if(!pollAdd(listen))
        {
            ERROR_LOG("Unable to add listen socket " << listen \
                    << " to the poll list: " << std::strerror(errno))
            return false;
        }
        m_listeners.insert(listen);
        return true;
    }
    else
    {
        ERROR_LOG("Socket " << listen << " already being listened to")
        return false;
    }
}

bool Fastcgipp::SocketGroup::listen(
        const char* name,
        uint32_t permissions,
        const char* owner,
        const char* group)
{
    if(std::remove(name) != 0 && errno != ENOENT)
    {
        ERROR_LOG("Unable to delete file \"" << name << "\": " \
                << std::strerror(errno))
        return false;
    }

    const auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1)
    {
        ERROR_LOG("Unable to create unix socket: " << std::strerror(errno))
        return false;

    }

    struct sockaddr_un address;
    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, name, sizeof(address.sun_path) - 1);

    if(bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        ERROR_LOG("Unable to bind to unix socket \"" << name << "\": " \
                << std::strerror(errno));
        close(fd);
        std::remove(name);
        return false;
    }
    unlink(name);

	// Set the user and group of the socket
    if(owner!=nullptr && group!=nullptr)
	{
		struct passwd* passwd = getpwnam(owner);
        struct group* grp = getgrnam(group);
        if(fchown(fd, passwd->pw_uid, grp->gr_gid)==-1)
        {
			ERROR_LOG("Unable to chown " << owner << ":" << group \
                    << " on the unix socket \"" << name << "\": " \
                    << std::strerror(errno));
            close(fd);
            return false;
		}
	}

    // Set the user and group of the socket
    if(fchmod(fd, permissions)<0)
    {
        ERROR_LOG("Unable to set permissions 0" << std::oct << permissions \
                << std::dec << " on \"" << name << "\": " \
                << std::strerror(errno));
        close(fd);
        return false;
    }

    if(::listen(fd, 100) < 0)
    {
        ERROR_LOG("Unable to listen on unix socket :\"" << name << "\": "\
                << std::strerror(errno));
        close(fd);
        return false;
    }

    if(!pollAdd(fd))
    {
        ERROR_LOG("Unable to add unix socket " << fd << " to poll list: "\
                << std::strerror(errno));
        close(fd);
        return false;
    }

    m_listeners.insert(fd);
    return true;
}

bool Fastcgipp::SocketGroup::listen(
        const char* interface,
        const char* service)
{
    if(service == nullptr)
    {
        ERROR_LOG("Cannot call listen(interface, service) with service=nullptr.")
        return false;
    }

    addrinfo hints;
    std::memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    addrinfo* result;

    if(getaddrinfo(interface, service, &hints, &result))
    {
        ERROR_LOG("Unable to use getaddrinfo() on " \
                << (interface==nullptr?"0.0.0.0":interface) << ":" << service << ". " \
                << std::strerror(errno))
        return false;
    }

    int fd=-1;
    for(auto i=result; i!=nullptr; i=result->ai_next)
    {
        fd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if(fd == -1)
            continue;
        if(
                bind(fd, i->ai_addr, i->ai_addrlen) == 0
                && ::listen(fd, 100) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(result);

    if(fd==-1)
    {
        ERROR_LOG("Unable to bind/listen on " \
                << (interface==nullptr?"0.0.0.0":interface) << ":" << service)
        return false;
    }

    if(!pollAdd(fd))
    {
        ERROR_LOG("Unable to add socket " \
                << (interface==nullptr?"0.0.0.0":interface) << ":" << service \
                << " to poll list: " << std::strerror(errno));
        close(fd);
        return false;
    }

    m_listeners.insert(fd);
    return true;
}

Fastcgipp::Socket Fastcgipp::SocketGroup::poll(bool block)
{
    int pollResult;
    epoll_event event;

    while(true)
    {
        if(block)
        {
            std::unique_lock<std::mutex> lock(m_sleepingMutex);
            m_sleeping=true;
            lock.unlock();
            pollResult = epoll_wait(
                    m_poll,
                    &event,
                    1,
                    -1);
            lock.lock();
            m_sleeping=false;
        }
        else
        {
            pollResult = epoll_wait(
                    m_poll,
                    &event,
                    1,
                    0);
        }

        if(pollResult<0)
        {
            FAIL_LOG("Error on epoll_wait() with epfd " \
                    << m_poll << ": " << std::strerror(errno))
        }
        else if(pollResult == 1)
        {
            if(m_listeners.find(event.data.fd) != m_listeners.end())
            {
                if(event.events & EPOLLIN)
                {
                    createSocket(event.data.fd);
                    continue;
                }
                else if(event.events & EPOLLERR)
                {
                    FAIL_LOG("Error in listen socket.")
                }
                else if(event.events & EPOLLHUP)
                {
                    FAIL_LOG("The listen socket hung up.")
                }
            }
            else if(event.data.fd == m_wakeSockets[1])
            {
                if(event.events & EPOLLIN)
                {
                    char x[256];
                    if(read(m_wakeSockets[1], x, 256)<1)
                        FAIL_LOG("Unable to read out of wakeup socket: " << \
                                std::strerror(errno))
                    block=false;
                    continue;
                }
                else if(event.events & EPOLLHUP)
                {
                    FAIL_LOG("The wakeup socket hung up.")
                }
                else if(event.events & EPOLLERR)
                {
                    FAIL_LOG("Error in the wakeup socket.")
                }
            }
            else
            {
                if(event.events & EPOLLIN)
                {
                    const auto socket = m_sockets.find(event.data.fd);
                    if(socket == m_sockets.end())
                    {
                        ERROR_LOG("Epoll gave fd " << event.data.fd \
                                << " which isn't in m_sockets.")
                        pollDel(event.data.fd);
                        ::close(event.data.fd);
                        continue;
                    }
                    else
                        return socket->second;
                }
                else if(event.events & EPOLLERR)
                {
                    ERROR_LOG("Error in socket" << event.data.fd)
                    m_sockets.erase(event.data.fd);
                    continue;
                }
                else if(event.events & EPOLLHUP)
                {
                    WARNING_LOG("Socket " << event.data.fd << " hung up")
                    m_sockets.erase(event.data.fd);
                    continue;
                }
            }
        }

        return Socket();
    }
}

void Fastcgipp::SocketGroup::wake()
{
    std::unique_lock<std::mutex> lock(m_sleepingMutex);
    if(m_sleeping)
    {
        m_sleeping=false;
        lock.unlock();
        char x=0;
        if(write(m_wakeSockets[0], &x, 1) != 1)
            FAIL_LOG("Unable to write to wakeup socket: " \
                    << std::strerror(errno))
    }
}

void Fastcgipp::SocketGroup::createSocket(const socket_t listener)
{
    ::sockaddr_un addr;
    ::socklen_t addrlen=sizeof(sockaddr_un);
    const socket_t socket=::accept(
            listener,
            (::sockaddr*)&addr,
            &addrlen);
    if(socket<0)
        FAIL_LOG("Unable to accept() with fd " \
                << listener << ": " \
                << std::strerror(errno))
    if(::fcntl(
            socket,
            F_SETFL,
            (::fcntl(socket, F_GETFL)|O_NONBLOCK)^O_NONBLOCK)
            < 0)
    {
        WARNING_LOG("Unable to set NONBLOCK on fd " << socket \
                << " with fcntl(): " << std::strerror(errno))
        ::close(socket);
        return;
    }

    m_sockets.erase(socket);
    m_sockets.emplace(
            socket,
            std::move(Socket(socket, *this)));
}

Fastcgipp::Socket::Socket():
    m_data(new Data(-1, false, *(SocketGroup*)(nullptr))),
    m_original(false)
{}

bool Fastcgipp::SocketGroup::pollAdd(const socket_t socket)
{
    epoll_event event;
    event.data.fd = socket;
    event.events = EPOLLIN;
    return epoll_ctl(m_poll, EPOLL_CTL_ADD, socket, &event) != -1;
}

bool Fastcgipp::SocketGroup::pollDel(const socket_t socket)
{
    return epoll_ctl(m_poll, EPOLL_CTL_DEL, socket, nullptr) != -1;
}
