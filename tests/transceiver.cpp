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

const unsigned int seed = 2006;
const size_t messageSize = 12314;
const unsigned int echoers = 5;

struct Echo
{
    Fastcgipp::Protocol::RequestId id;
    Fastcgipp::Message message;
    size_t position;

    Echo(
            Fastcgipp::Protocol::RequestId id_,
            Fastcgipp::Message&& message_,
            size_t position_):
        id(id_),
        message(std::move(message_)),
        position(position_)
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
        echoQueue.emplace(id, std::move(message), 0);
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

            while(echo.position < echo.message.size)
            {
                auto block = transceiver.requestWrite(
                        size_t(echo.message.size*extra(rd)));
                const size_t size = std::min(echo.message.size, block.m_size);
                std::copy(
                        echo.message.data.get(),
                        echo.message.data.get()+size,
                        block.m_data);
                transceiver.commitWrite(block.m_size, echo.id.m_socket, killIt(rd));
                echo.position += size;
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

struct Buffer
{
    std::vector<char> send;
    std::vector<char> receive;
};

int main()
{
    transceiver.listen("127.0.0.1", "23987");
    transceiver.start();
    echoTerminate = false;

    std::mt19937 rd(seed);
    std::bernoulli_distribution boolDist(0.5);
    std::uniform_int_distribution<> requestDist(0, 65534);

    Fastcgipp::SocketGroup group;
    const unsigned int maxConnections=128;
    unsigned int connections=0;

    std::multimap<Fastcgipp::Protocol::RequestId, Buffer> requests;
    std::multimap<Fastcgipp::Protocol::RequestId, Buffer>::iterator request;

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
                if(!request->second.send.empty() || boolDist(rd))
                {
                    // A new request over the same connection
                    request = requests.insert(std::make_pair(
                                Fastcgipp::Protocol::RequestId(
                                    requestDist(rd),
                                    request->first.m_socket),
                                Buffer()));
                    request->second.send.reserve(messageSize);
                    request->second.receive.reserve(messageSize);
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
                request = requests.insert(std::make_pair(
                            Fastcgipp::Protocol::RequestId(
                                requestDist(rd),
                                group.connect(
                                        "127.0.0.1",
                                        "23987")),
                            Buffer()));
                request->second.send.reserve(messageSize);
                request->second.receive.reserve(messageSize);
                ++connections;
                DEBUG_LOG("Making new connection with id=" << request->first.m_id)
            }
            request->second.send.resize(sizeof(FullMessage));
            Fastcgipp::Protocol::Header& header = 
                *(Fastcgipp::Protocol::Header*)request->second.send.data();
            header.fcgiId = request->first.m_id;
            header.contentLength = messageSize-51;
            header.paddingLength = 51;

            for(
                    auto i=request->second.send.cbegin();
                    i<request->second.send.cend(); )
                i += const_cast<Fastcgipp::Socket&>(
                    request->first.m_socket).write(
                        &*i,
                        request->second.send.cend()-i);
        }
        else
        {
            auto socket = group.poll(false);
            if(socket.valid())
            {
                Fastcgipp::Protocol::Header header;
                size_t received=0;
                while(received<sizeof(header))
                    received += socket.read(
                            (char*)&header,
                            sizeof(header)-received);


                request = requests.begin();
                while(request->first.m_socket 
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
