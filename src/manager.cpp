/*!
 * @file       manager.cpp
 * @brief      Defines the Manager class
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       May 14, 2016
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

#include "fastcgi++/log.hpp"
#include "fastcgi++/manager.hpp"

Fastcgipp::Manager_base* Fastcgipp::Manager_base::instance=nullptr;

Fastcgipp::Manager_base::Manager_base(unsigned threads):
    m_transceiver(std::bind(
                &Fastcgipp::Manager_base::push,
                this,
                std::placeholders::_1,
                std::placeholders::_2)),
    m_terminate(true),
    m_stop(true),
    m_threads(threads)
{
    if(instance != nullptr)
        FAIL_LOG("You're not allowed to have multiple manager instances")
    instance = this;
}

void Fastcgipp::Manager_base::terminate()
{
    std::lock_guard<std::mutex> lock(m_tasksMutex);
    m_terminate=true;
    m_transceiver.terminate();
    m_wake.notify_all();
}

void Fastcgipp::Manager_base::stop()
{
    std::lock_guard<std::mutex> lock(m_tasksMutex);
    m_stop=true;
    m_transceiver.stop();
    m_wake.notify_all();
}

void Fastcgipp::Manager_base::start()
{
    std::lock_guard<std::mutex> lock(m_tasksMutex);
    DEBUG_LOG("Starting fastcgi++ manager")
    m_stop=false;
    m_terminate=false;
    m_transceiver.start();
    for(auto& thread: m_threads)
        if(!thread.joinable())
        {
            std::thread newThread(&Fastcgipp::Manager_base::handler, this);
            thread.swap(newThread);
        }
}

void Fastcgipp::Manager_base::join()
{
    for(auto& thread: m_threads)
        if(thread.joinable())
            thread.join();
    m_transceiver.join();
}

#include <signal.h>
void Fastcgipp::Manager_base::setupSignals()
{
	struct sigaction sigAction;
	sigAction.sa_handler=Fastcgipp::Manager_base::signalHandler;
	sigemptyset(&sigAction.sa_mask);
	sigAction.sa_flags=0;

	sigaction(SIGPIPE, &sigAction, NULL);
	sigaction(SIGUSR1, &sigAction, NULL);
	sigaction(SIGTERM, &sigAction, NULL);
}

void Fastcgipp::Manager_base::signalHandler(int signum)
{
	switch(signum)
	{
		case SIGUSR1:
		{
			if(instance)
            {
                DEBUG_LOG("Received SIGUSR1. Stopping fastcgi++ manager.")
                instance->stop();
            }
            else
                WARNING_LOG("Received SIGUSR1 but fastcgi++ manager isn't "\
                        "running")
			break;
		}
		case SIGTERM:
		{
			if(instance)
            {
                DEBUG_LOG("Received SIGTERM. Terminating fastcgi++ manager.")
                instance->stop();
            }
            else
                WARNING_LOG("Received SIGTERM but fastcgi++ manager isn't "\
                        "running")
			break;
		}
	}
}

void Fastcgipp::Manager_base::localHandler()
{
    Message message;
    Socket socket;
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        message = std::move(m_messages.front().first);
        socket = m_messages.front().second;
        m_messages.pop();
    }

	if(message.type == 0)
	{
		const Protocol::Header& header=*(Protocol::Header*)message.data.data(); 
		switch(header.type)
		{
            case Protocol::RecordType::GET_VALUES:
			{
                std::vector<char>::const_iterator name;
                std::vector<char>::const_iterator value;
                std::vector<char>::const_iterator end;

                while(Protocol::processParamHeader(
                        message.data.cbegin()+sizeof(header),
                        message.data.cend(),
                        name,
                        value,
                        end))
                {
                    switch(value-name)
                    {
                        case 14:
                        {    
                            if(std::equal(name, value, "FCGI_MAX_CONNS"))
                            {
                                std::vector<char> record(
                                        sizeof(Protocol::maxConnsReply));
                                const char* const start 
                                    = (const char*)&Protocol::maxConnsReply;
                                const char* end = start
                                    +sizeof(Protocol::maxConnsReply);
                                std::copy(start, end, record.begin());
                                m_transceiver.send(
                                        socket,
                                        std::move(record),
                                        false);
                            }
                            break;
                        }
                        case 13:
                        {
                            if(std::equal(name, value, "FCGI_MAX_REQS"))
                            {
                                std::vector<char> record(
                                        sizeof(Protocol::maxReqsReply));
                                const char* const start 
                                    = (const char*)&Protocol::maxReqsReply;
                                const char* end = start
                                    + sizeof(Protocol::maxReqsReply);
                                std::copy(start, end, record.begin());
                                m_transceiver.send(
                                        socket,
                                        std::move(record),
                                        false);
                            }
                            break;
                        }
                        case 15:
                        {
                            if(std::equal(name, value, "FCGI_MPXS_CONNS"))
                            {
                                std::vector<char> record(
                                        sizeof(Protocol::mpxsConnsReply));
                                const char* const start 
                                    = (const char*)&Protocol::mpxsConnsReply;
                                const char* end = start
                                    + sizeof(Protocol::mpxsConnsReply);
                                std::copy(start, end, record.begin());
                                m_transceiver.send(
                                        socket,
                                        std::move(record),
                                        false);
                            }
                            break;
                        }
                    }
                }
				break;
			}

			default:
			{
                std::vector<char> record(
                        sizeof(Protocol::Header)
                        +sizeof(Protocol::UnknownType));

                Protocol::Header& sendHeader=*(Protocol::Header*)record.data();
				sendHeader.version = Protocol::version;
				sendHeader.type = Protocol::RecordType::UNKNOWN_TYPE;
				sendHeader.fcgiId = 0;
				sendHeader.contentLength = sizeof(Protocol::UnknownType);
				sendHeader.paddingLength = 0;

                Protocol::UnknownType& sendBody
                    = *(Protocol::UnknownType*)(record.data()+sizeof(header));
				sendBody.type = header.type;

                m_transceiver.send(socket, std::move(record), false);

				break;
			}
		}
	}
    else
        ERROR_LOG("Got a non-FastCGI record destined for the manager")
}

void Fastcgipp::Manager_base::handler()
{
    std::unique_lock<std::shared_timed_mutex> requestsWriteLock(
            m_requestsMutex,
            std::defer_lock);
    std::unique_lock<std::mutex> tasksLock(m_tasksMutex);
    std::shared_lock<std::shared_timed_mutex> requestsReadLock(m_requestsMutex);

    while(!m_terminate && !(m_stop && m_requests.empty()))
    {
        requestsReadLock.unlock();
        while(!m_tasks.empty())
        {
            auto id = m_tasks.front();
            m_tasks.pop();
            tasksLock.unlock();

            if(id.m_id == 0)
                localHandler();
            else
            {
                requestsReadLock.lock();
                auto request = m_requests.find(id);
                if(request != m_requests.end())
                {
                    std::unique_lock<std::mutex> requestLock(
                            request->second->mutex,
                            std::try_to_lock);
                    requestsReadLock.unlock();

                    if(requestLock)
                    {
                        auto lock = request->second->handler();
                        if(!lock || !id.m_socket.valid())
                        {
                            if(lock)
                                lock.unlock();
                            requestsWriteLock.lock();
                            requestLock.unlock();
                            m_requests.erase(request);
                            requestsWriteLock.unlock();
                        }
                        else
                        {
                            requestLock.unlock();
                            lock.unlock();
                        }
                    }
                }
                else
                    requestsReadLock.unlock();
            }
            tasksLock.lock();
        }

        requestsReadLock.lock();
        if(m_terminate || (m_stop && m_requests.empty()))
            break;
        requestsReadLock.unlock();
        m_wake.wait(tasksLock);
        requestsReadLock.lock();
    }
}

void Fastcgipp::Manager_base::push(Protocol::RequestId id, Message&& message)
{
    if(id.m_id == 0)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_messages.push(std::make_pair(std::move(message), id.m_socket));
    }
    else if(id.m_id == Protocol::badFcgiId)
    {
        std::lock_guard<std::shared_timed_mutex> lock(m_requestsMutex);
        const auto range = m_requests.equal_range(id.m_socket);
        auto request = range.first;
        while(request != range.second)
        {
            std::unique_lock<std::mutex> lock(
                    request->second->mutex,
                    std::try_to_lock);
            if(lock)
            {
                lock.unlock();
                request = m_requests.erase(request);
            }
            else
                ++request;
        }
        return;
    }
    else
    {
        std::unique_lock<std::shared_timed_mutex> lock(m_requestsMutex);
        auto request = m_requests.find(id);
        if(request == m_requests.end())
        {
            const Protocol::Header& header=
                *(Protocol::Header*)message.data.data();
            if(header.type == Protocol::RecordType::BEGIN_REQUEST)
            {
                const Protocol::BeginRequest& body
                    =*(Protocol::BeginRequest*)(
                            message.data.data()
                            +sizeof(header));

                request = m_requests.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(id),
                        std::forward_as_tuple()).first;

                request->second = makeRequest(
                        id,
                        body.role,
                        body.kill());
                lock.unlock();
            }
            else
                WARNING_LOG("Got a non BEGIN_REQUEST record for a request that"\
                        " doesn't exist")
            return;
        }
        else
            request->second->push(std::move(message));
    }
    std::lock_guard<std::mutex> lock(m_tasksMutex);
    m_tasks.push(id);
    m_wake.notify_one();
}
