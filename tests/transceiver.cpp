#include "fastcgi++/log.hpp"
#include "fastcgi++/transceiver.hpp"
#include "fastcgi++/sockets.hpp"

#include <map>
#include <mutex>
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>
#include <random>
#include <thread>
#include <chrono>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <array>

const unsigned int seed = 2006;
const size_t messageSize = 12314;
const unsigned int echoers = 5;

struct Echo
{
    Fastcgipp::Protocol::RequestId id;
    Fastcgipp::Message message;

    Echo(
            Fastcgipp::Protocol::RequestId id_,
            Fastcgipp::Message&& message_):
        id(id_),
        message(std::move(message_))
    {}
};
std::mutex echoMutex;
std::queue<Echo> echoQueue;
std::condition_variable echoCv;
std::atomic_bool echoTerminate;

void receive(
        Fastcgipp::Protocol::RequestId id,
        Fastcgipp::Message&& message)
{
    {
        std::lock_guard<std::mutex> lock(echoMutex);
        echoQueue.emplace(id, std::move(message));
    }
    echoCv.notify_one();
}

Fastcgipp::Transceiver transceiver(receive);

void echo()
{
    std::mt19937 rd(seed);
    std::bernoulli_distribution killIt(0.1);
    std::uniform_real_distribution<> extra(1.0, 4.0);

    std::unique_lock<std::mutex> lock(echoMutex);

    while(!echoTerminate || echoQueue.size())
    {
        if(echoQueue.size())
        {
            Echo echo(std::move(echoQueue.front()));
            echoQueue.pop();
            lock.unlock();
            
            size_t position = 0;

            while(position < echo.message.size)
            {
                auto block = transceiver.requestWrite(
                        size_t(echo.message.size*extra(rd)));
                const size_t size = std::min(echo.message.size, block.m_size);
                std::copy(
                        echo.message.data.get(),
                        echo.message.data.get()+size,
                        block.m_data);
                transceiver.commitWrite(
                        block.m_size,
                        echo.id.m_socket,
                        killIt(rd));
                position += size;
            }

            lock.lock();
        }
        else
            echoCv.wait(lock);
    }
}

struct FullMessage
{
    Fastcgipp::Protocol::Header header;
    char body[messageSize];
};

int main()
{
    transceiver.listen("127.0.0.1", "23987");
    transceiver.start();
    echoTerminate = false;
    std::array<std::thread, echoers> threads;
    for(std::thread& thread: threads)
    {
        std::thread newThread(echo);
        thread.swap(newThread);
    }

    std::mt19937 rd(seed);
    std::bernoulli_distribution boolDist(0.5);
    std::uniform_int_distribution<> requestDist(0, 65534);

    Fastcgipp::SocketGroup group;
    const unsigned int maxConnections=128;
    unsigned int connections=0;

    std::map<Fastcgipp::Socket, std::vector<char>> buffers;
    std::map<Fastcgipp::Protocol::RequestId, std::vector<char>> requests;
    std::map<Fastcgipp::Protocol::RequestId, std::vector<char>>::iterator request;

    while(connections<maxConnections || requests.size())
    {
        // Should we sent data?
        if(boolDist(rd))
        {
            // Yes will send data

            // Should we make a new connection or use an old one?
            if(connections==maxConnections || (requests.size() && boolDist(rd)))
            {
                // Let's use an old one
                std::uniform_int_distribution<> dist(0, requests.size()-1);
                request = requests.begin();
                std::advance(request, dist(rd));

                // Should we reuse the request or make a new one?
                if(!request->second.empty() || boolDist(rd))
                {
                    std::pair<
                        std::map<
                            Fastcgipp::Protocol::RequestId,
                            std::vector<char>>::iterator,
                        bool> pair;
                    pair.second = false;
                    // A new request over the same connection
                    while(!pair.second)
                        pair = requests.insert(std::make_pair(
                                    Fastcgipp::Protocol::RequestId(
                                        requestDist(rd),
                                        request->first.m_socket),
                                    std::vector<char>()));
                    request = pair.first;
                    request->second.reserve(messageSize);
                    DEBUG_LOG("Making new request with id=" << request->first.m_id)
                }
                else
                {
                    // Or maybe we should simulate a killed connection?
                    if(boolDist(rd) && boolDist(rd) && boolDist(rd) && boolDist(rd))
                    {
                        DEBUG_LOG("Simulating a client side kill")
                        const_cast<Fastcgipp::Socket&>(
                                request->first.m_socket).close();
                        continue;
                    }

                    DEBUG_LOG("Reusing request with id=" << request->first.m_id)
                }
            }
            else
            {
                // Let's make a new one
                auto pair = requests.insert(std::make_pair(
                            Fastcgipp::Protocol::RequestId(
                                requestDist(rd),
                                group.connect(
                                        "127.0.0.1",
                                        "23987")),
                            std::vector<char>()));
                if(pair.second)
                    request = pair.first;
                else
                    FAIL_LOG("Couln't create a new connection")

                request->second.reserve(messageSize);
                ++connections;
                DEBUG_LOG("Making new connection with id=" << request->first.m_id)
            }
            request->second.resize(sizeof(FullMessage));
            Fastcgipp::Protocol::Header& header = 
                *(Fastcgipp::Protocol::Header*)request->second.data();
            header.fcgiId = request->first.m_id;
            header.contentLength = messageSize-51;
            header.paddingLength = 51;

            for(
                    auto i=request->second.cbegin();
                    i<request->second.cend(); )
                i += const_cast<Fastcgipp::Socket&>(
                    request->first.m_socket).write(
                        &*i,
                        request->second.cend()-i);
        }
        else
        {
            auto socket = group.poll(false);
            if(socket.valid())
            {
                std::vector<char>& buffer=buffers[socket];
                if(buffer.empty())
                    buffer.reserve(sizeof(Fastcgipp::Protocol::Header));
                size_t received = buffer.size();

                if(received < sizeof(Fastcgipp::Protocol::Header))
                {
                    buffer.resize(sizeof(Fastcgipp::Protocol::Header));
                    received += socket.read(
                            buffer.data()+received,
                            buffer.size()-received);
                    if(received < buffer.size())
                    {
                        buffer.resize(received);
                        continue;
                    }
                }

                Fastcgipp::Protocol::Header& header =
                    *(Fastcgipp::Protocol::Header*)buffer.data();

                buffer.reserve(
                        +sizeof(Fastcgipp::Protocol::Header)
                        +header.contentLength
                        +header.paddingLength);
                buffer.resize(
                        +sizeof(Fastcgipp::Protocol::Header)
                        +header.contentLength
                        +header.paddingLength);
                received += socket.read(
                        buffer.data()+received,
                        buffer.size()-received);
                if(received < buffer.size())
                {
                    buffer.resize(received);
                    continue;
                }

                DEBUG_LOG("Get an echo with id=" << header.fcgiId << " and size=" << buffer.size())

                auto request = requests.find(Fastcgipp::Protocol::RequestId(
                            header.fcgiId,
                            socket));
                if(request == requests.end())
                    FAIL_LOG("Got an echo that isn't in our requests")
                if(request->second != buffer)
                    FAIL_LOG("GOt an echo that doesn't match what we sent")
            }

            // Clean up buffers
            {
                auto buffer=buffers.begin();

                while(buffer != buffers.end())
                {
                    if(buffer->first.valid())
                        ++buffer;
                    else
                        buffer = buffers.erase(buffer);
                }
            }
            
            // Clean up buffers
            {
                auto request=requests.begin();

                while(request != requests.end())
                {
                    if(request->first.m_socket.valid())
                        ++request;
                    else
                        request = requests.erase(request);
                }
            }
        }
    }

    transceiver.stop();
    /*for(auto& request: requests)
    {
        if(!request.second.data.empty() || request.second.clientSocket.valid())
            FAIL_LOG("A request is incomplete")
    }*/

    return 0;
}
