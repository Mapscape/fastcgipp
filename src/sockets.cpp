/*!
 * @file       sockets.cpp
 * @brief      Defines everything for interfaces with OS level sockets.
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 24, 2016
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
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

Fastcgipp::Socket::Socket(
        const socket_t& socket,
        SocketGroup& group,
        bool valid):
    m_data(new Data(socket, valid, group)),
    m_original(true)
{
    epoll_event event;

    event.data.fd = socket;
    event.events = EPOLLIN;
    if(epoll_ctl(group.m_poll, EPOLL_CTL_ADD, socket, &event) != 0)
    {
        WARNING_LOG("Error adding fd " << socket << " to epfd " \
                << group.m_poll << " using epoll_ctl(): " \
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
        ::epoll_ctl(
                m_data->m_group.m_poll,
                EPOLL_CTL_DEL,
                m_data->m_socket,
                nullptr);
        m_data->m_group.m_sockets.erase(m_data->m_socket);
    }
}

Fastcgipp::SocketGroup::SocketGroup():
    m_poll(epoll_create1(0)),
    m_sleeping(false)
{
    // Add our wakeup socket into the epoll list
    epoll_event event;
    socketpair(AF_UNIX, SOCK_STREAM, 0, m_wakeSockets);
    event.data.fd = m_wakeSockets[1];
    event.events = EPOLLIN;
    epoll_ctl(m_poll, EPOLL_CTL_ADD, m_wakeSockets[1], &event);
}

bool Fastcgipp::SocketGroup::listen(const socket_t& listen)
{
    if(m_listeners.find(listen) == m_listeners.end())
    {
        ERROR_LOG("Tried adding a listen socket that is already added.")
        return false;
    }
    else
    {
        m_listeners.insert(listen);

        // Add our listen socket into the epoll list
        epoll_event event;
        event.data.fd = listen;
        event.events = EPOLLIN;
        epoll_ctl(m_poll, EPOLL_CTL_ADD, listen, &event);
        return true;
    }
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
                    FAIL_LOG("Error: the listen socket hung up.")
                }
            }
            else if(event.data.fd == m_wakeSockets[1])
            {
                if(event.events & EPOLLIN)
                {
                    char x[256];
                    if(read(m_wakeSockets[1], x, 256)<1)
                        FAIL_LOG("Error: couldn't read out of wakeup socket")
                    block=false;
                    continue;
                }
                else if(event.events & EPOLLHUP)
                {
                    FAIL_LOG("Error: the wakeup socket hung up.")
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
                        WARNING_LOG("Epoll gave fd " << event.data.fd \
                                << " which isn't in m_sockets.")
                        ::epoll_ctl(
                                m_poll,
                                EPOLL_CTL_DEL,
                                event.data.fd,
                                nullptr);
                        ::close(event.data.fd);
                        continue;
                    }
                    else
                        return socket->second;
                }
                else if(event.events & EPOLLERR)
                {
                    WARNING_LOG("Error in socket" << event.data.fd << ".")
                    m_sockets.erase(event.data.fd);
                    continue;
                }
                else if(event.events & EPOLLHUP)
                {
                    WARNING_LOG("Socket " << event.data.fd << " hung up.")
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
            FAIL_LOG("Error: couldn't write to wakeup socket")
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
        FAIL_LOG("Error on accept() with fd " \
                << listener << ": " \
                << std::strerror(errno))
    if(::fcntl(
            socket,
            F_SETFL,
            (::fcntl(socket, F_GETFL)|O_NONBLOCK)^O_NONBLOCK)
            < 0)
    {
        WARNING_LOG("Error setting NONBLOCK on fd " << socket \
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
