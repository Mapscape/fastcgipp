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

const unsigned int maxConnections=64;
const unsigned int maxRequests=2024;

const unsigned int seed = 2006;
const size_t messageSize = 12314;
const unsigned int echoers = 1;
unsigned int echos = 0;

enum class Kill: char
{
    DONT = 0,
    CLIENT = 1,
    SERVER = 2
};

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
    if(id.m_id != Fastcgipp::Protocol::badFcgiId)
    {
        {
            std::lock_guard<std::mutex> lock(echoMutex);
            echoQueue.emplace(id, std::move(message));
        }
        echoCv.notify_one();
    }
}

Fastcgipp::Transceiver transceiver(receive);

void echo()
{
    std::mt19937 rd(seed);
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
                        size_t((echo.message.size-position)*extra(rd)));
                const size_t size = std::min(echo.message.size-position, block.m_size);
                std::copy(
                        echo.message.data.get()+position,
                        echo.message.data.get()+position+size,
                        block.m_data);
                position += size;
                const Kill& killer=*(Kill*)(echo.message.data.get()
                        +sizeof(Fastcgipp::Protocol::Header));
                const bool kill =
                    position<echo.message.size?false:killer==Kill::SERVER;
                transceiver.commitWrite(
                        size,
                        echo.id.m_socket,
                        kill);
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
    std::random_device trueRand;
    std::uniform_int_distribution<> portDist(2048, 65534);
    const std::string port = std::to_string(portDist(trueRand));

    if(!transceiver.listen("127.0.0.1", port.c_str()))
        FAIL_LOG("Unable to listen")
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
    std::bernoulli_distribution nastyDist(0.0001);
    std::uniform_int_distribution<> requestDist(0, 65534);
    std::discrete_distribution<> killDist({2, 1, 1});

    Fastcgipp::SocketGroup group;
    unsigned int connections=0;
    unsigned int requestCount=0;

    typedef std::map<Fastcgipp::Socket, std::vector<char>> Buffers;
    Buffers buffers;
    typedef Fastcgipp::Protocol::Requests<std::vector<char>> Requests;
    Requests requests;
    Requests::iterator request;

    while(requestCount<maxRequests || requests.size())
    {
        // Should we sent data?
        if(!(requestCount>=maxRequests && requests.empty()) && boolDist(rd))
        {
            // Yes will send data

            // Should we make a new connection or use an old one?
            if(connections>=maxConnections || requestCount>=maxRequests || (!requests.empty() && boolDist(rd)))
            {
                // Let's use an old one
                request = requests.begin();

                // Or maybe we should simulate a nasty killed connection?
                if(requests.size()>=5 && nastyDist(rd))
                {
                    const_cast<Fastcgipp::Socket&>(
                            request->first.m_socket).close();
                    --connections;
                    auto range = requests.equal_range(request->first.m_socket);
                    requests.erase(range.first, range.second);
                    buffers.erase(request->first.m_socket);
                    goto RECEIVE;
                }

                // Find a usefull request
                while(request != requests.end())
                {
                    if(request->second.empty())
                        break;

                    const Kill& kill=*(Kill*)(request->second.data()
                            +sizeof(Fastcgipp::Protocol::Header));
                    if(requestCount<maxRequests && kill == Kill::DONT)
                        break;
                    ++request;
                }

                if(request == requests.end())
                    goto RECEIVE;

                // Should we reuse the request itself?
                if(requestCount<maxRequests && !request->second.empty())
                {
                    // A new request over the same connection
                    std::pair<Requests::iterator, bool> pair;
                    pair.second = false;
                    while(!pair.second)
                        pair = requests.insert(std::make_pair(
                                    Fastcgipp::Protocol::RequestId(
                                        requestDist(rd),
                                        request->first.m_socket),
                                    std::vector<char>()));
                    requestCount++;
                    request = pair.first;
                    request->second.reserve(sizeof(FullMessage));
                }
                else if(!request->second.empty())
                    FAIL_LOG("QUOI LE FUCK")
                    //goto RECEIVE;
            }
            else
            {
                auto socket = group.connect("127.0.0.1", port.c_str());
                if(!socket.valid())
                    FAIL_LOG("Couldn't connect")

                // Let's make a new one
                auto pair = requests.insert(std::make_pair(
                            Fastcgipp::Protocol::RequestId(
                                requestDist(rd),
                                socket),
                            std::vector<char>()));
                requestCount++;
                if(pair.second)
                    request = pair.first;
                else
                    FAIL_LOG("Couln't create a new connection")

                request->second.reserve(sizeof(FullMessage));
                ++connections;
            }
            request->second.resize(sizeof(FullMessage));
            Fastcgipp::Protocol::Header& header = 
                *(Fastcgipp::Protocol::Header*)request->second.data();
            header.fcgiId = request->first.m_id;
            header.contentLength = messageSize-51;
            header.paddingLength = 51;

            Kill& kill=*(Kill*)(request->second.data()
                    +sizeof(Fastcgipp::Protocol::Header));


            kill = (Kill)killDist(rd);

            if(kill == Kill::SERVER)
            {
                // Are we sharing this socket?
                auto range = requests.equal_range(request->first.m_socket);
                for(auto it = range.first; it != range.second; ++it)
                    if(it != request)
                    {
                        kill = Kill::CLIENT;
                        break;
                    }
            }

            ssize_t sent=0;
            for(
                    auto i=request->second.cbegin();
                    i<request->second.cend(); 
                    i += sent)
            {
                sent = const_cast<Fastcgipp::Socket&>(
                    request->first.m_socket).write(
                        &*i,
                        request->second.cend()-i);
                if(sent<=0)
                {
                    if(i != request->second.cbegin())
                        FAIL_LOG("A socket died while trying to send")
                    goto RECEIVE;
                }
            }
        }
        else
        {
            // We shall receive data
RECEIVE:
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
                    const ssize_t read = socket.read(
                            buffer.data()+received,
                            buffer.size()-received);
                    if(read<0)
                    {
                        if(received)
                            FAIL_LOG("FACK")
                        socket.close();
                        --connections;
                        auto range = requests.equal_range(socket);
                        int distance = 0;
                        for(auto it = range.first; it != range.second; ++it)
                            ++distance;
                        if(distance != 1)
                            FAIL_LOG("Got a server side kill that affects multiple requests")
                        requests.erase(range.first);
                        buffers.erase(socket);
                        continue;
                    }
                    received += read;
                    if(received < buffer.size())
                    {
                        buffer.resize(received);
                        continue;
                    }
                }

                buffer.resize(
                        +sizeof(Fastcgipp::Protocol::Header)
                        +((Fastcgipp::Protocol::Header*)buffer.data())->contentLength
                        +((Fastcgipp::Protocol::Header*)buffer.data())->paddingLength);

                Fastcgipp::Protocol::Header& header =
                    *(Fastcgipp::Protocol::Header*)buffer.data();
                buffer.reserve(buffer.size());
                const ssize_t read = socket.read(
                        buffer.data()+received,
                        buffer.size()-received);
                if(read<0)
                    FAIL_LOG("FACK")
                received += read;
                if(received < buffer.size())
                {
                    buffer.resize(received);
                    continue;
                }

                auto request = requests.find(Fastcgipp::Protocol::RequestId(
                            header.fcgiId,
                            socket));
                if(request == requests.end())
                    FAIL_LOG("Got an echo " << header.fcgiId << " that isn't in our requests")
                if(request->second != buffer)
                    FAIL_LOG("Got an echo that doesn't match what we sent")

                bool shared=false;
                {
                    auto range = requests.equal_range(socket);
                    for(auto it = range.first; it != range.second; ++it)
                        if(it != request)
                        {
                            shared=true;
                            break;
                        }
                }
                
                Kill kill=*(Kill*)(request->second.data()
                        +sizeof(Fastcgipp::Protocol::Header));
                switch(kill)
                {
                    case Kill::CLIENT:
                        if(shared)
                        {
                            buffer.resize(0);
                            buffer.reserve(0);
                        }
                        else
                        {
                            --connections;
                            socket.close();
                            buffers.erase(socket);
                        }
                        requests.erase(request);
                        break;

                    default:
                        request->second.resize(0);
                        buffer.resize(0);
                        buffer.reserve(0);
                }
            }
        }
    }

    if(group.size())
        FAIL_LOG("Main loop finished but there are still active sockets " << group.size())
    if(buffers.size())
        FAIL_LOG("Main loop finished but there are still buffers")
    if(requests.size())
        FAIL_LOG("Main loop finished but there are still requests")

    transceiver.stop();

    echoTerminate = true;
    echoCv.notify_all();

    for(std::thread& thread: threads)
        thread.join();

    return 0;
}
