#include "fastcgi++/log.hpp"
#include "fastcgi++/sockets.hpp"

#include <vector>
#include <map>
#include <random>
#include <algorithm>
#include <iterator>
#include <mutex>

const size_t chunkSize=1024;
const unsigned int tranCount=512;
const unsigned int socketCount=256;

bool done;
std::mutex doneMutex;

Fastcgipp::SocketGroup* serverGroup;

void client()
{
    Fastcgipp::SocketGroup group;

    struct Buffer
    {
        std::vector<char> buffer;
        std::vector<char> data;
        std::vector<char>::const_iterator send;
        std::vector<char>::iterator receive;
        int count;
    };

    std::map<Fastcgipp::Socket, Buffer> buffers;

    unsigned int maxSockets=0;
    unsigned int sends=0;
    std::random_device rd;
    std::uniform_int_distribution<unsigned long long> intDist;
    unsigned int state;
    while(maxSockets<socketCount || buffers.size())
    {
        // Any data waiting for us to recieve?
        {
            Fastcgipp::Socket socket=group.poll(false);
            if(socket.valid())
            {
                auto buffer = buffers.find(socket);
                if(buffer == buffers.end())
                    FAIL_LOG("Got a valid socket client side that isn't "\
                            "accounted for")
                const size_t read = socket.read(
                        &*buffer->second.receive,
                        buffer->second.buffer.end()-buffer->second.receive);
                buffer->second.receive += read;
                if(buffer->second.receive == buffer->second.buffer.end())
                {
                    if(buffer->second.data != buffer->second.buffer)
                        FAIL_LOG("Echoed data does not match that which was "\
                                "sent")

                    ++buffer->second.count;

                    if(buffer->second.count == tranCount)
                    {
                        socket.close();
                        buffers.erase(buffer);
                    }
                    else
                    {
                        ++sends;
                        buffer->second.send = buffer->second.data.cbegin();
                        buffer->second.receive = buffer->second.buffer.begin();
                    }
                }
            }
        }

        // Check for dirty sockets
        for(const auto& buffer: buffers)
            if(!buffer.first.valid())
                FAIL_LOG("A socket has become invalid client side!")

        // Do we initiate a connection, send data or wait?
        if(socketCount-maxSockets || sends>0)
        {
            std::discrete_distribution<> dist({
                    double(socketCount-maxSockets),
                    double(sends)});
            state=dist(rd);
        }
        else
            state=2;

        switch(state)
        {
            case 0:
            {
                Fastcgipp::Socket socket = group.connect("127.0.0.1", "23987");
                if(!socket.valid())
                    FAIL_LOG("Unable to connect to server")
                Buffer& buffer(buffers[socket]);
                buffer.buffer.resize(chunkSize);
                buffer.data.resize(chunkSize);
                for(
                        auto i = (unsigned long long*)(&*buffer.data.begin());
                        i < (unsigned long long*)(&*buffer.data.end());
                        ++i)
                    *i = intDist(rd);

                buffer.send = buffer.data.cbegin();
                buffer.receive = buffer.buffer.begin();
                buffer.count = 0;
                ++maxSockets;
                ++sends;
                break;
            }
            case 1:
            {
                auto pair = buffers.begin();
                while(pair->second.data.end() == pair->second.send)
                    ++pair;

                if(!pair->first.valid())
                    FAIL_LOG("Trying to send on an invalid socket client side");

                Fastcgipp::Socket& socket =
                    const_cast<Fastcgipp::Socket&>(pair->first);
                Buffer& buffer = pair->second;
                const size_t sent = socket.write(
                        &*buffer.send,
                        buffer.data.end()-buffer.send);
                buffer.send += sent;

                if(buffer.send == buffer.data.end())
                    --sends;

                break;
            }
            default:
                if(maxSockets<socketCount || buffers.size())
                    group.poll(true);
                break;
        }
    }
    
    std::lock_guard<std::mutex> lock(doneMutex);
    done=true;
    serverGroup->wake();
}

void server()
{
    struct Buffer
    {
        std::vector<char> data;
        std::vector<char>::iterator position;
        bool sending;
    };

    Fastcgipp::SocketGroup group;
    serverGroup = &group;
    if(!group.listen("127.0.0.1", "23987"))
        FAIL_LOG("Unable to listen on 127.0.0.1:23987")
    std::map<Fastcgipp::Socket, Buffer> buffers;
    std::unique_lock<std::mutex> lock(doneMutex);
    bool flushed;
    unsigned int sends=0;

    while(!done)
    {
        lock.unlock();

        // Transmit data
        flushed = true;
        for(auto& pair: buffers)
        {
            Fastcgipp::Socket& socket = 
                const_cast<Fastcgipp::Socket&>(pair.first);
            Buffer& buffer = pair.second;
            
            if(socket.valid() && buffer.sending)
            {
                const size_t sent = socket.write(
                        &*buffer.position,
                        buffer.data.end()-buffer.position);
                buffer.position += sent;
                if(buffer.position  == buffer.data.end())
                {
                    buffer.sending = false;
                    buffer.position = buffer.data.begin();
                    --sends;
                }
                else
                    flushed = false;
            }
        }

        // Any data waiting for us to receive?
        {
            Fastcgipp::Socket socket=group.poll(flushed);
            if(socket.valid())
            {
                auto pair = buffers.find(socket);
                if(pair == buffers.end())
                {
                    pair = buffers.insert(std::make_pair(
                                socket,
                                Buffer())).first;
                    pair->second.data.resize(chunkSize);
                    pair->second.position = pair->second.data.begin();
                }
                Buffer& buffer = pair->second;
                if(buffer.sending)
                    FAIL_LOG("Got a valid socket for reception that is now "\
                            "in sending mode")
                const size_t desired = buffer.data.end()-buffer.position;
                if(desired == 0)
                    FAIL_LOG("Trying to read zero bytes on server")
                const size_t read = socket.read(
                        &*buffer.position,
                        desired);
                if(read == 0)
                    FAIL_LOG("Read zero bytes on server side!")
                buffer.position += read;
                if(buffer.position == buffer.data.end())
                {
                    buffer.position = buffer.data.begin();
                    buffer.sending = true;
                    ++sends;
                }
            }
        }

        // Clean up dirty sockets
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

        lock.lock();
    }
    lock.unlock();

    if(group.size())
        FAIL_LOG("Server has active sockets when it shouldn't")
}

int main()
{
    done=false;
    std::thread serverThread(server);
    client();
    serverThread.join();
    return 0;
}
