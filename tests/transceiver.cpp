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

struct Pair
{
    Fastcgipp::Socket socket;
    Fastcgipp::Socket clientSocket;
    std::vector<char> data;
};

void receive(
        std::multimap<Fastcgipp::Protocol::FcgiId, Pair>& requests,
        std::mutex& mutex,
        Fastcgipp::Protocol::RequestId id,
        Fastcgipp::Message&& message)
{
    std::vector<char> data;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto range = requests.equal_range(id.m_id);
        if(range.first == requests.end())
            FAIL_LOG("receive() got a message associated with an unknown ID")
        
        auto invalid = range.second;
        auto item = range.second;
        for(auto it=range.first; it!=range.second; ++it)
        {
            if(!it->second.socket.valid())
                invalid=it;
            else if(it->second.socket == id.m_socket)
            {
                item=it;
                break;
            }
        }
        if(item == range.second)
            item = invalid;
        if(item == range.second)
            FAIL_LOG("receive() got a message associated with an unknown socket")

        item->second.socket = id.m_socket;
        data.swap(item->second.data);
    }

    if(!std::equal(
                data.cbegin(),
                data.cend(),
                message.data.get(),
                message.data.get()+message.size))
        FAIL_LOG("recieve() got a message that didn't match what was sent")
    else
        DEBUG_LOG("Got a good message")
}

struct FullMessage
{
    Fastcgipp::Protocol::Header header;
    char body[10000];
};

const unsigned int seed=2006;

int main()
{
    std::multimap<Fastcgipp::Protocol::FcgiId, Pair> requests;
    std::mutex mutex;

    Fastcgipp::Transceiver transceiver(
            std::bind(
                receive,
                std::ref(requests),
                std::ref(mutex),
                std::placeholders::_1,
                std::placeholders::_2));
    transceiver.listen("127.0.0.1", "23987");
    transceiver.start();


    std::mt19937 rd(seed);
    std::bernoulli_distribution boolDist(0.5);
    std::uniform_int_distribution<> requestDist(0, 65534);
    std::multimap<Fastcgipp::Protocol::FcgiId, Pair>::iterator request;

    {
    Fastcgipp::SocketGroup group;

    // Send data to transceiver
    for(int i=0; i<100; ++i)
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1us);
        std::lock_guard<std::mutex> lock(mutex);

        // Should we make a new connection or use an old one?
        if(requests.size() && boolDist(rd))
        {
            // Let's use an old one
            std::uniform_int_distribution<> dist(0, requests.size()-1);
            request = requests.begin();
            std::advance(request, dist(rd));

            // Should we reuse the request or make a new one?
            if(!request->second.data.empty() || boolDist(rd))
            {
                // A new request over the same connection
                auto& serverSocket = request->second.socket;
                auto& clientSocket = request->second.clientSocket;

                request = requests.insert(std::make_pair(
                            requestDist(rd),
                            Pair()));
                request->second.socket = serverSocket;
                request->second.clientSocket = clientSocket;
                DEBUG_LOG("Making new request with id=" << request->first)
            }
            else
                DEBUG_LOG("Reusing request with id=" << request->first)
        }
        else
        {
            // Let's make a new one
            request = requests.insert(std::make_pair(
                        requestDist(rd),
                        Pair()));
            DEBUG_LOG("Making new connection with id=" << request->first)
            request->second.clientSocket = group.connect("127.0.0.1", "23987");
        }
        auto& data = request->second.data;

        data.resize(sizeof(FullMessage));
        Fastcgipp::Protocol::Header& header = 
            *(Fastcgipp::Protocol::Header*)data.data();
        header.fcgiId = request->first;
        header.contentLength = 10000-51;
        header.paddingLength = 51;

        for(auto i=data.cbegin(); i<data.cend(); )
            i += request->second.clientSocket.write(&*i, data.cend()-i);
    }

    // Get data from transceiver
    {
        std::vector<char> data;
        data.resize(sizeof(FullMessage));
        Fastcgipp::Protocol::Header& header = 
            *(Fastcgipp::Protocol::Header*)data.data();
        header.fcgiId = 12345;
        header.contentLength = 50000-51;
        header.paddingLength = 51;
        
        /*
        Fastcgipp::WriteBlock block;
        const size_t differential = 181;
        auto position = data.cbegin();

        do
        {
            block = transceiver.requestWrite(data.cend()-position);
            std::copy(
                    position,
                    position
                        +block.size
                        -(differential<block.size?differential:0),
                    block.data);
            transceiver.commitWrite();*/
    }
    }
    transceiver.stop();
    for(auto& request: requests)
    {
        if(!request.second.data.empty() || request.second.clientSocket.valid())
            FAIL_LOG("A request is incomplete")
    }

    return 0;
}
