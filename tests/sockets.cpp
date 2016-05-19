#include "fastcgi++/log.hpp"
#include "fastcgi++/sockets.hpp"

#include <vector>
#include <map>
#include <random>
#include <algorithm>
#include <iterator>
#include <mutex>
#include <thread>
#include <string>
#include <condition_variable>

const unsigned int chunkSize=1024;
const unsigned int tranCount=768;
const unsigned int socketCount=512;
const unsigned int maxConc=64;
const unsigned int seed=2006;

std::string port;

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
    std::mt19937 rd(seed);
    std::mt19937_64 rd_64(seed);
    std::bernoulli_distribution boolDist(0.75);
    unsigned int state;
    while(maxSockets<socketCount || buffers.size())
    {
        // Any data waiting for us to recieve?
        do
        {
            const Fastcgipp::Socket socket=group.poll(false);
            if(socket.valid())
            {
                auto pair = buffers.find(socket);
                if(pair == buffers.end())
                    FAIL_LOG("Got a valid socket client side that isn't "\
                            "accounted for")
                auto& buffer = pair->second;
                const ssize_t read = socket.read(
                        &*buffer.receive,
                        buffer.buffer.end()-buffer.receive);
                if(read<0)
                {
                    if(!(buffer.receive == buffer.buffer.begin() &&
                            (buffer.send == buffer.data.cend() ||
                             buffer.send == buffer.data.cbegin())))
                        FAIL_LOG("Socket killed when it's not done echoing")
                    if(buffer.send == buffer.data.cend())
                        --sends;
                    buffers.erase(pair);
                    continue;
                }
                if(buffer.send != buffer.data.cend())
                    FAIL_LOG("Poll gave us a socket we are sending on")
                buffer.receive += read;
                if(buffer.receive == buffer.buffer.end())
                {
                    if(buffer.data != buffer.buffer)
                        FAIL_LOG("Echoed data does not match that which was "\
                                "sent")

                    ++buffer.count;

                    if(buffer.count == tranCount)
                    {
                        socket.close();
                        buffers.erase(pair);
                    }
                    else
                    {
                        ++sends;
                        buffer.send = buffer.data.cbegin();
                        buffer.receive = buffer.buffer.begin();
                        for(
                                auto i = (uint64_t*)(&*buffer.data.begin());
                                i < (uint64_t*)(&*buffer.data.end());
                                ++i)
                            *i = rd_64();
                    }
                }
            }
            else
                break;
        } while(boolDist(rd));

        // Check for dirty sockets
        for(auto buffer=buffers.cbegin(); buffer != buffers.cend(); ++buffer)
            if(!buffer->first.valid())
                FAIL_LOG("We've got a dirty socket client side")

        const unsigned int connects = std::min(
                socketCount-maxSockets,
                unsigned(maxConc-buffers.size()));

        // Do we initiate a connection, send data or wait?
        if(connects>0 || sends>0)
        {
            std::discrete_distribution<> dist({
                    double(connects),
                    double(sends)});
            state=dist(rd);
        }
        else
            state=2;

        switch(state)
        {
            case 0:
            {
                unsigned int count = connects;
                do
                {
                    const Fastcgipp::Socket socket =
                        group.connect("127.0.0.1", port.c_str());
                    if(!socket.valid())
                        FAIL_LOG("Unable to connect to server")
                    Buffer& buffer(buffers[socket]);
                    buffer.buffer.resize(chunkSize);
                    buffer.data.resize(chunkSize);
                    for(
                            auto i = (uint64_t*)(&*buffer.data.begin());
                            i < (uint64_t*)(&*buffer.data.end());
                            ++i)
                        *i = rd_64();

                    buffer.send = buffer.data.cbegin();
                    buffer.receive = buffer.buffer.begin();
                    buffer.count = 0;
                    ++maxSockets;
                    ++sends;
                } while(boolDist(rd) && --count>0);
                break;
            }
            case 1:
            {
                unsigned int count=sends;
                do
                {
                    auto pair = buffers.begin();
                    while(pair != buffers.end() && (!pair->first.valid() ||
                            pair->second.data.end() == pair->second.send))
                        ++pair;

                    if(pair == buffers.end())
                        break;

                    const Fastcgipp::Socket& socket = pair->first;
                    Buffer& buffer = pair->second;
                    const ssize_t sent = socket.write(
                            &*buffer.send,
                            buffer.data.end()-buffer.send);
                    if(sent < 0)
                        break;
                    else if(sent == 0)
                        FAIL_LOG("We failed sending data!")
                    buffer.send += sent;

                    if(buffer.send == buffer.data.end())
                        --sends;
                } while(boolDist(rd) && --count>0);

                break;
            }
            default:
                if(maxSockets<socketCount || buffers.size())
                    group.poll(true);
                break;
        }
    }

    if(group.size())
        FAIL_LOG("Client has active sockets when it shouldn't")

    std::lock_guard<std::mutex> lock(doneMutex);
    done=true;
    serverGroup->wake();
}

std::condition_variable cv;
std::mutex cvMutex;

void server()
{
    std::unique_lock<std::mutex> cvLock(cvMutex);
    struct Buffer
    {
        std::vector<char> data;
        std::vector<char>::iterator position;
        bool sending;
    };

    Fastcgipp::SocketGroup group;
    serverGroup = &group;
    if(!group.listen("127.0.0.1", port.c_str()))
        FAIL_LOG("Unable to listen")
    cv.notify_all();
    cvLock.unlock();
    std::map<Fastcgipp::Socket, Buffer> buffers;
    std::unique_lock<std::mutex> lock(doneMutex);
    bool flushed;
    unsigned int sends=0;

    std::mt19937 rd(seed);
    std::bernoulli_distribution killSocket(0.0001);

    while(!done)
    {
        lock.unlock();

        // Transmit data
        flushed = true;
        for(auto& pair: buffers)
        {
            const Fastcgipp::Socket& socket = pair.first;
            Buffer& buffer = pair.second;

            if(socket.valid() && buffer.sending)
            {
                const ssize_t sent = socket.write(
                        &*buffer.position,
                        buffer.data.end()-buffer.position);
                if(sent<=0)
                {
                    continue;
                    lock.lock();
                }
                buffer.position += sent;
                if(buffer.position  == buffer.data.end())
                {
                    buffer.sending = false;
                    buffer.position = buffer.data.begin();
                    --sends;
                    if(killSocket(rd))
                    {
                        socket.close();
                        buffers.erase(socket);
                    }
                }
                else
                    flushed = false;
            }
        }

        // Any data waiting for us to receive?
        {
            const Fastcgipp::Socket socket = group.poll(flushed);
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
                const ssize_t read = socket.read(
                        &*buffer.position,
                        desired);
                if(read<0)
                {
                    buffers.erase(pair);
                    lock.lock();
                    continue;
                }
                buffer.position += read;
                if(buffer.position == buffer.data.end())
                {
                    buffer.position = buffer.data.begin();
                    buffer.sending = true;
                    ++sends;
                }
            }
        }

        lock.lock();
    }
    lock.unlock();

    if(group.size())
        FAIL_LOG("Server has active sockets when it shouldn't")
}

#include "fastcgi++/config.h"
#if defined FASTCGIPP_UNIX || defined FASTCGIPP_LINUX
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sstream>
unsigned int openfds()
{
    std::ostringstream ss;
    ss << "/proc/" << getpid() << "/fd";

    DIR* const directory = opendir(ss.str().c_str());
    dirent* file;
    unsigned int count = 0;

    while((file = readdir(directory)) != nullptr)
        ++count;

    closedir(directory);
    return count;
}
#elif
unsigned int openfds()
{
    return 0;
}
#endif

int main()
{
    const auto initialFds = openfds();

    std::random_device trueRand;
    std::uniform_int_distribution<> portDist(2048, 65534);
    port = std::to_string(portDist(trueRand));

    done=false;
    std::thread serverThread(server);
    {
        std::unique_lock<std::mutex> cvLock(cvMutex);
        cv.wait(cvLock);
    }
    client();
    serverThread.join();

    if(openfds() != initialFds)
        FAIL_LOG("There are leftover file descriptors after they should all "\
                "have been closed");
    return 0;
}
