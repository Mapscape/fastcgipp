#include "fastcgi++/log.hpp"
#include "fastcgi++/transceiver.hpp"
#include "fastcgi++/sockets.hpp"

#include <map>
#include <mutex>
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>

struct Pair
{
    Fastcgipp::Socket socket;
    std::vector<char> data;
};

void receive(
        std::map<Fastcgipp::Protocol::FcgiId, Pair>& requests,
        std::mutex& mutex,
        Fastcgipp::Protocol::RequestId id,
        Fastcgipp::Message&& message)
{
    std::vector<char> data;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto item = requests.find(id.m_id);
        if(item == requests.end())
            FAIL_LOG("receive() got a message associated with an unknown ID")
        data.swap(item->second.data);
    }

    if(!std::equal(
                data.cbegin(),
                data.cend(),
                message.data.get(),
                message.data.get()+message.size))
        FAIL_LOG("recieve() got a message that didn't match what was sent. " << message.size << " vs " << data.size())
}

struct FullMessage
{
    Fastcgipp::Protocol::Header header;
    char body[50000];
};

int main()
{
    std::map<Fastcgipp::Protocol::FcgiId, Pair> requests;
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

    Fastcgipp::SocketGroup group;

    // Connected
    Fastcgipp::Socket socket=group.connect("127.0.0.1", "23987");
    
    // Send data to transceiver
    {
        std::vector<char> data;
        data.resize(sizeof(FullMessage));
        Fastcgipp::Protocol::Header& header = 
            *(Fastcgipp::Protocol::Header*)data.data();
        header.fcgiId = 12345;
        header.contentLength = 50000-51;
        header.paddingLength = 51;

        {
            std::lock_guard<std::mutex> lock(mutex);
            requests[12345].data.swap(data);
        }

        for(auto i=data.cbegin(); i<data.cend(); )
            i += socket.write(&*i, data.cend()-i);
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

    transceiver.stop();

    return 0;
}
