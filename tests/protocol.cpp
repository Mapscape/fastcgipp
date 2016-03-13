#include "fastcgi++/log.hpp"
#include "fastcgi++/protocol.hpp"

#include <random>
#include <memory>
#include <cstdint>

int main()
{
    Fastcgipp::Logging::logTimestamp = true;  

    INFO_LOG("Testing Fastcgipp::Protocol::BigEndian with a 64 bit signed integer")
    {
        const int64_t actual = -0x62c74ce376736dd0;
        const Fastcgipp::Protocol::BigEndian<int64_t> reversed(actual);
        const unsigned char* data = (const unsigned char*)&reversed;

        if(!(
                    reversed == actual &&
                    data[0] == 0x9d &&
                    data[1] == 0x38 &&
                    data[2] == 0xb3 &&
                    data[3] == 0x1c &&
                    data[4] == 0x89 &&
                    data[5] == 0x8c &&
                    data[6] == 0x92 &&
                    data[7] == 0x30))
            ERROR_LOG("Test failed!!")
    }

    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution<size_t> randomShortSize(1, 127);
    std::uniform_int_distribution<size_t> randomLongSize(128, 10000000);

    INFO_LOG("Testing Fastcgipp::Protocol::processParamHeader() with short values and short names")
    for(int i=0; i<10; ++i)
    {
        const size_t nameSize = randomShortSize(engine);
        const size_t valueSize = randomShortSize(engine);
        const size_t dataSize = 2+nameSize+valueSize;
        std::unique_ptr<char> body(new char[dataSize]);
        char* const nameStart = body.get()+2;
        char* const valueStart = nameStart+nameSize;
        *(body.get()+0) = (char)nameSize;
        *(body.get()+1) = (char)valueSize;

        const char* nameResult;
        size_t nameSizeResult;
        const char* valueResult;
        size_t valueSizeResult;

        size_t passedSize;
        if(i<5)
            passedSize = i;
        else
            passedSize = dataSize-(i-5);

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.get(),
                passedSize,
                nameResult,
                nameSizeResult,
                valueResult,
                valueSizeResult);        

        if(((passedSize==dataSize)!=retVal) || (retVal && !(
                nameResult == nameStart &&
                nameSizeResult == nameSize &&
                valueResult == valueStart &&
                valueSizeResult == valueSize)))
            ERROR_LOG("Test failed!!")
    }

    INFO_LOG("Testing Fastcgii::Protocol::processParamHeader() with long values and short names")
    for(int i=0; i<100; ++i)
    {
        const size_t nameSize = randomShortSize(engine);
        const size_t valueSize = randomLongSize(engine);
        const size_t dataSize = 5+nameSize+valueSize;
        std::unique_ptr<char> body(new char[dataSize]);
        char* const nameStart = body.get()+5;
        char* const valueStart = nameStart+nameSize;
        *(body.get()+0) = (char)nameSize;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)(body.get()+1) 
            = (uint32_t)valueSize;
        *(body.get()+1) |= 0x80;

        const char* nameResult;
        size_t nameSizeResult;
        const char* valueResult;
        size_t valueSizeResult;

        size_t passedSize;
        if(i<50)
            passedSize = i;
        else
            passedSize = dataSize-(i-50);

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.get(),
                passedSize,
                nameResult,
                nameSizeResult,
                valueResult,
                valueSizeResult);        

        if(((passedSize==dataSize)!=retVal) || (retVal && !(
                nameResult == nameStart &&
                nameSizeResult == nameSize &&
                valueResult == valueStart &&
                valueSizeResult == valueSize)))
            ERROR_LOG("Test failed!!")
    }

    INFO_LOG("Testing Fastcgii::Protocol::processParamHeader() with short values and long names")
    for(int i=0; i<10; ++i)
    {
        const size_t nameSize = randomLongSize(engine);
        const size_t valueSize = randomShortSize(engine);
        const size_t dataSize = 5+nameSize+valueSize;
        std::unique_ptr<char> body(new char[dataSize]);
        char* const nameStart = body.get()+5;
        char* const valueStart = nameStart+nameSize;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)(body.get()) 
            = (uint32_t)nameSize;
        *(body.get()) |= 0x80;
        *(body.get()+4) = (char)valueSize;

        const char* nameResult;
        size_t nameSizeResult;
        const char* valueResult;
        size_t valueSizeResult;

        size_t passedSize;
        if(i<50)
            passedSize = i;
        else
            passedSize = dataSize-(i-50);

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.get(),
                passedSize,
                nameResult,
                nameSizeResult,
                valueResult,
                valueSizeResult);        

        if(((passedSize==dataSize)!=retVal) || (retVal && !(
                nameResult == nameStart &&
                nameSizeResult == nameSize &&
                valueResult == valueStart &&
                valueSizeResult == valueSize)))
            ERROR_LOG("Test failed!!")
    }

    INFO_LOG("Testing Fastcgii::Protocol::processParamHeader() with short values and long names")
    for(int i=0; i<10; ++i)
    {
        const size_t nameSize = randomLongSize(engine);
        const size_t valueSize = randomLongSize(engine);
        const size_t dataSize = 8+nameSize+valueSize;
        std::unique_ptr<char> body(new char[dataSize]);
        char* const nameStart = body.get()+8;
        char* const valueStart = nameStart+nameSize;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)(body.get()) 
            = (uint32_t)nameSize;
        *(body.get()) |= 0x80;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)(body.get()+4) 
            = (uint32_t)valueSize;
        *(body.get()+4) |= 0x80;

        const char* nameResult;
        size_t nameSizeResult;
        const char* valueResult;
        size_t valueSizeResult;

        size_t passedSize;
        if(i<50)
            passedSize = i;
        else
            passedSize = dataSize-(i-50);

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.get(),
                passedSize,
                nameResult,
                nameSizeResult,
                valueResult,
                valueSizeResult);        

        if(((passedSize==dataSize)!=retVal) || (retVal && !(
                nameResult == nameStart &&
                nameSizeResult == nameSize &&
                valueResult == valueStart &&
                valueSizeResult == valueSize)))
            ERROR_LOG("Test failed!!")
    }
}
