#include "fastcgi++/log.hpp"
#include "fastcgi++/protocol.hpp"

#include <random>
#include <memory>
#include <cstdint>
#include <vector>

int main()
{
    // Testing Fastcgipp::Protocol::BigEndian with a 64 bit signed integer
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
            FAIL_LOG("Fastcgipp::Protocol::BigEndian with a 64 bit signed int")
    }

    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution<size_t> randomShortSize(1, 127);
    std::uniform_int_distribution<size_t> randomLongSize(128, 10000000);

    // Testing Fastcgipp::Protocol::processParamHeader() with short values and
    // short names
    for(int i=0; i<10; ++i)
    {
        const size_t nameSize = randomShortSize(engine);
        const size_t valueSize = randomShortSize(engine);
        const size_t dataSize = 2+nameSize+valueSize;
        std::vector<char> body(dataSize);
        const std::vector<char>::const_iterator name = body.cbegin()+2;
        const std::vector<char>::const_iterator value = name+nameSize;
        const std::vector<char>::const_iterator end = value+valueSize;
        body[0] = (char)nameSize;
        body[1] = (char)valueSize;

        std::vector<char>::const_iterator nameResult;
        std::vector<char>::const_iterator valueResult;
        std::vector<char>::const_iterator endResult;

        const std::vector<char>::const_iterator passedEnd =
            i<5?body.cbegin()+i:body.cend()-i;

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.cbegin(),
                passedEnd,
                nameResult,
                valueResult,
                endResult);

        if(((passedEnd==body.cend())!=retVal) || (retVal && !(
                nameResult == name &&
                valueResult == value &&
                endResult == end)))
            FAIL_LOG("Fastcgipp::Protocol::processParamHeader with short "\
                    "values and short names")
    }

    // Testing Fastcgii::Protocol::processParamHeader() with long values and
    // short names
    for(int i=0; i<100; ++i)
    {
        const size_t nameSize = randomShortSize(engine);
        const size_t valueSize = randomLongSize(engine);
        const size_t dataSize = 5+nameSize+valueSize;
        std::vector<char> body(dataSize);
        const std::vector<char>::const_iterator name = body.cbegin()+5;
        const std::vector<char>::const_iterator value = name+nameSize;
        const std::vector<char>::const_iterator end = value+valueSize;
        body[0] = (char)nameSize;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)&body[1]
            = (uint32_t)valueSize;
        body[1] |= 0x80;

        std::vector<char>::const_iterator nameResult;
        std::vector<char>::const_iterator valueResult;
        std::vector<char>::const_iterator endResult;

        const std::vector<char>::const_iterator passedEnd =
            i<50?body.cbegin()+i:body.cend()-i;

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.cbegin(),
                passedEnd,
                nameResult,
                valueResult,
                endResult);

        if(((passedEnd==body.cend())!=retVal) || (retVal && !(
                nameResult == name &&
                valueResult == value &&
                endResult == end)))
            FAIL_LOG("Fastcgipp::Protocol::processParamHeader with long "\
                    "values and short names")
    }

    // Testing Fastcgii::Protocol::processParamHeader() with short values and
    // long names
    for(int i=0; i<10; ++i)
    {
        const size_t nameSize = randomLongSize(engine);
        const size_t valueSize = randomShortSize(engine);
        const size_t dataSize = 5+nameSize+valueSize;
        std::vector<char> body(dataSize);
        const std::vector<char>::const_iterator name = body.cbegin()+5;
        const std::vector<char>::const_iterator value = name+nameSize;
        const std::vector<char>::const_iterator end = value+valueSize;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)body.data()
            = (uint32_t)nameSize;
        body[0] |= 0x80;
        body[4] = (char)valueSize;

        std::vector<char>::const_iterator nameResult;
        std::vector<char>::const_iterator valueResult;
        std::vector<char>::const_iterator endResult;

        const std::vector<char>::const_iterator passedEnd =
            i<50?body.cbegin()+i:body.cend()-i;

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.cbegin(),
                passedEnd,
                nameResult,
                valueResult,
                endResult);

        if(((passedEnd==body.cend())!=retVal) || (retVal && !(
                nameResult == name &&
                valueResult == value &&
                endResult == end)))
            FAIL_LOG("Fastcgipp::Protocol::processParamHeader with short "\
                    "values and long names")
    }

    // Testing Fastcgii::Protocol::processParamHeader() with long values and
    // long names
    for(int i=0; i<10; ++i)
    {
        const size_t nameSize = randomLongSize(engine);
        const size_t valueSize = randomLongSize(engine);
        const size_t dataSize = 8+nameSize+valueSize;
        std::vector<char> body(dataSize);
        const std::vector<char>::const_iterator name = body.cbegin()+8;
        const std::vector<char>::const_iterator value = name+nameSize;
        const std::vector<char>::const_iterator end = value+valueSize;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)body.data()
            = (uint32_t)nameSize;
        body[0] |= 0x80;
        *(Fastcgipp::Protocol::BigEndian<int32_t>*)&body[4]
            = (uint32_t)valueSize;
        body[4] |= 0x80;

        std::vector<char>::const_iterator nameResult;
        std::vector<char>::const_iterator valueResult;
        std::vector<char>::const_iterator endResult;

        const std::vector<char>::const_iterator passedEnd =
            i<50?body.cbegin()+i:body.cend()-i;

        const bool retVal = Fastcgipp::Protocol::processParamHeader(
                body.cbegin(),
                passedEnd,
                nameResult,
                valueResult,
                endResult);

        if(((passedEnd==body.cend())!=retVal) || (retVal && !(
                nameResult == name &&
                valueResult == value &&
                endResult == end)))
            FAIL_LOG("Fastcgipp::Protocol::processParamHeader with long "\
                    "values and long names")
    }

    return 0;
}
