/*!
 * @file       http.hpp
 * @brief      Declares elements of the HTTP protocol
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       March 14, 2016
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

#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>
#include <ostream>
#include <istream>
#include <iterator>
#include <cstring>
#include <algorithm>
#include <map>
#include <vector>
#include <memory>
#include <array>
#include <ctime>
#include <cstring>

#include <fastcgi++/protocol.hpp>

//! Topmost namespace for the fastcgi++ library
namespace Fastcgipp
{
    //! Defines classes and functions relating to the HTTP protocol
    namespace Http
    {
        //! Holds a file uploaded from the client
        /*!
         * The actual name associated with the file is omitted from the class
         * so it can be linked in an associative container.
         *
         * @tparam charT Type of character to use in the value string (char or
         *               wchar_t)
         *
         * @date    March 11, 2016
         * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
         */
        template<class charT> struct File
        {
            //! Filename
            std::basic_string<charT> filename;

            //! Content Type
            std::basic_string<charT> contentType;

            //! Pointer to file data
            const char* data() const
            {
                return m_data.get();
            }

            //! Size of file data
            size_t size() const
            {
                return m_size;
            }

            //! Release the file data.
            char* release() const
            {
                m_size=0;
                return m_data.release();
            }

            File():
                m_size(0)
            {}

            //! Move constructor
            File(File&& x):
                filename(std::move(x.filename)),
                contentType(std::move(x.contentType)),
                m_data(std::move(x.m_data)),
                m_size(x.m_size)
            {}

        private:
            //! Pointer to file data
            mutable std::unique_ptr<char> m_data;

            //! Size of data in bytes pointed to by data.
            mutable size_t m_size;

            template<class T> friend class Environment;
        };

        //! The HTTP request method as an enumeration
        enum class RequestMethod
        {
            ERROR=0,
            HEAD=1,
            GET=2,
            POST=3,
            PUT=4,
            DELETE=5,
            TRACE=6,
            OPTIONS=7,
            CONNECT=8
        };

        //! Some textual labels for RequestMethod
        extern const std::array<const char* const, 9> requestMethodLabels;

        template<class charT, class Traits>
        inline std::basic_ostream<charT, Traits>& operator<<(
                std::basic_ostream<charT, Traits>& os,
                const RequestMethod requestMethod)
        {
            return os << requestMethodLabels[(int)requestMethod];
        }

        //! Efficiently stores IPv6 addresses
        /*!
         * This class stores IPv6 addresses as a 128 bit array. It does this as
         * opposed to storing the string itself to facilitate efficient logging
         * and processing of the address. The class possesses full IO and
         * comparison capabilities as well as allowing bitwise AND operations
         * for netmask calculation. It detects when an IPv4 address is stored
         * outputs it accordingly.
         *
         * @date    March 14, 2016
         * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
         */
        class Address
        {
        public:
            //! This is the data length of the IPv6 address
            static const size_t size=16;

            //! Data representation of the IPv6 address
            std::array<unsigned char, size> m_data;

            //! Assign the IPv6 address from a data array
            /*!
             * @param[in] data_ Pointer to a 16 byte array
             */
            Address operator=(const unsigned char* data)
            {
                std::copy(data, data+m_data.size(), m_data.begin());
                return *this;
            }

            Address operator=(const Address& address)
            {
                std::copy(
                        address.m_data.begin(),
                        address.m_data.end(),
                        m_data.begin());
                return *this;
            }

            Address(const Address& address)
            {
                std::copy(
                        address.m_data.begin(),
                        address.m_data.end(),
                        m_data.begin());
            }

            //! Construct the IPv6 address from a data array
            /*!
             * @param[in] data_ Pointer to a 16 byte array
             */
            explicit Address(const unsigned char* data)
            {
                std::copy(data, data+m_data.size(), m_data.begin());
            }

            //! Assign the IP address from a string of characters
            /*!
             * In order for this to work the string must represent either an
             * IPv4 address in standard textual decimal form (127.0.0.1) or an
             * IPv6 in standard form.
             *
             * @param[in] start First character of the string
             * @param[in] end Last character of the string + 1
             * @tparam charT Character type.
             */
            template<class charT> void assign(
                    const charT* start,
                    const charT* end);

            bool operator==(const Address& x) const
            {
                return std::memcmp(m_data.data(), x.m_data.data(), size)==0;
            }

            bool operator<(const Address& x) const
            {
                return std::memcmp(m_data.data(), x.m_data.data(), size)<0;
            }

            //! Returns false if the ip address is zeroed. True otherwise
            operator bool() const;

            Address operator&(const Address& x) const;

            Address& operator&=(const Address& x);

            //! Set all bits to zero in IP address
            void zero()
            {
                m_data.fill(0);
            }
        };

        //! Address stream insertion operation
        /*!
         * This stream inserter obeys all stream manipulators regarding
         * alignment, field width and numerical base.
         */
        template<class charT, class Traits>
        std::basic_ostream<charT, Traits>& operator<<(
                std::basic_ostream<charT, Traits>& os,
                const Address& address);

        //! Address stream extractor operation
        /*!
         * In order for this to work the string must represent either an IPv4
         * address in standard textual decimal form (127.0.0.1) or an IPv6 in
         * standard form.
         */
        template<class charT, class Traits>
        std::basic_istream<charT, Traits>& operator>>(
                std::basic_istream<charT, Traits>& is,
                Address& address);

        //! Data structure of HTTP environment data
        /*!
         * This structure contains all HTTP environment data for each
         * individual request. The data is processed from FastCGI parameter
         * records.
         *
         * @tparam charT Character type to use for strings
         *
         * @date    March 14, 2016
         * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
         */
        template<class charT> struct Environment
        {
            //! Hostname of the server
            std::basic_string<charT> host;

            //! User agent string
            std::basic_string<charT> userAgent;

            //! Content types the client accepts
            std::basic_string<charT> acceptContentTypes;

            //! Languages the client accepts
            std::basic_string<charT> acceptLanguages;

            //! Character sets the clients accepts
            std::basic_string<charT> acceptCharsets;

            //! Referral URL
            std::basic_string<charT> referer;

            //! Content type of data sent from client
            std::basic_string<charT> contentType;

            //! HTTP root directory
            std::basic_string<charT> root;

            //! Filename of script relative to the HTTP root directory
            std::basic_string<charT> scriptName;

            //! REQUEST_METHOD
            RequestMethod requestMethod;

            //! REQUEST_URI
            std::basic_string<charT> requestUri;

            //! Path information type
            typedef std::vector<std::basic_string<charT>> PathInfo;

            //! Path information
            PathInfo pathInfo;

            //! The etag the client assumes this document should have
            int etag;

            //! How many seconds the connection should be kept alive
            int keepAlive;

            //! Length of content to be received from the client (post data)
            unsigned int contentLength;

            //! IP address of the server
            Address serverAddress;

            //! IP address of the client
            Address remoteAddress;

            //! TCP port used by the server
            uint16_t serverPort;

            //! TCP port used by the client
            uint16_t remotePort;

            //! Timestamp the client has for this document
            std::time_t ifModifiedSince;

            //! Container with all url-encoded cookie data
            std::multimap<
                std::basic_string<charT>,
                std::basic_string<charT>> cookies;

            //! Container with all url-encoded GET data
            std::multimap<
                std::basic_string<charT>,
                std::basic_string<charT>> gets;

            //! Container of none-file POST data
            std::multimap<
                std::basic_string<charT>,
                std::basic_string<charT>> posts;

            //! Container of file POST data
            std::multimap<
                std::basic_string<charT>,
                File<charT>> files;

            //! Parses FastCGI parameter data into the data structure
            /*!
             * This function will take the body of a FastCGI parameter record
             * and parse the data into the data structure. data should equal
             * the first character of the records body with size being it's
             * content length.
             *
             * @param[in] start Pointer to the first byte of parameter data
             * @param[in] end Pointer to 1+ the last byte of parameter data
             */
            void fill(const char* start, const char* end);

            //! Consolidates POST data into a single buffer
            /*!
             * This function will take arbitrarily divided chunks of raw http
             * post data and consolidate them into m_postBuffer.
             *
             * @param[in] data Pointer to the first byte of post data.
             * @param[in] size Size of data in bytes.
             */
            void fillPostBuffer(const char* data, size_t size);

            //! Attempts to parse the POST buffer
            /*!
             * If the content type is recognized, this function will parse the
             * post buffer and return true. If it isn't recognized, it will
             * return false.
             *
             * @return True if successfully parsed. False otherwise.
             */
            bool parsePostBuffer();

            //! Get the post buffer
            const std::vector<char>& postBuffer() const
            {
                return m_postBuffer;
            }

            //! Clear the post buffer
            void clearPostBuffer()
            {
                m_postBuffer.clear();
                m_postBuffer.reserve(0);
            }

            Environment():
                requestMethod(RequestMethod::ERROR),
                etag(0),
                keepAlive(0),
                contentLength(0),
                serverPort(0),
                remotePort(0)
            {}
        private:
            //! Parses "multipart/form-data" http post data
            inline void parsePostsMultipart();

            //! Parses "application/x-www-form-urlencoded" post data
            inline void parsePostsUrlEncoded();

            //! Raw string of characters representing the post boundary
            std::vector<char> boundary;

            //! Buffer for processing post data
            std::vector<char> m_postBuffer;
        };

        //! Convert a char string to a std::wstring
        /*!
         * @param[in] start First byte in char string
         * @param[in] end 1+ last byte of the string (no null terminator)
         * @param[out] string Reference to the wstring that should be modified
         */
        void charToString(
                const char* start,
                const char* end,
                std::wstring& string);

        //! Convert a char string to a std::string
        /*!
         * @param[in] start First byte in char string
         * @param[in] end 1+ last byte of the string (no null terminator)
         * @param[out] string Reference to the string that should be modified
         */
        inline void charToString(
                const char* start,
                const char* end,
                std::string& string)
        {
            string.assign(start, end);
        }

        //! Convert a char string to an integer
        /*!
         * This function is very similar to std::atoi() except that it takes
         * start/end values of a non null terminated char string instead of a
         * null terminated string. The first character must be either a number
         * or a minus sign (-). As soon as the end is reached or a non
         * numerical character is reached, the result is tallied and returned.
         *
         * @param[in] start Pointer to the first byte in the string
         * @param[in] end Pointer to the last byte in the string + 1
         * @return Integer value represented by the string
         */
        template<class charT> int atoi(const charT* start, const charT* end);

        //! Decodes a url-encoded string into a multimap container
        /*!
         * @param[in] start Data to decode
         * @param[in] end +1 last byte to decode
         * @param[out] output Container to output data into
         * @param[in] fieldSeparator Character that signifies field separation
         */
        template<class charT> void decodeUrlEncoded(
                const char* start,
                const char* end,
                std::multimap<
                    std::basic_string<charT>,
                    std::basic_string<charT>>& output,
                const char fieldSeparator='&');

        //! Convert a string with percent escaped byte values to their values
        /*!
         * Since converting a percent escaped string to actual values can only
         * make it shorter, it is safe to assume that the return value will
         * always be smaller than size. It is thereby a safe move to make the
         * destination block of memory the same size as the source.
         *
         * @param[in] start Iterator to the first character in the percent
         *                  escaped string
         * @param[in] end Iterator to +1 the last character in the percent
         *                escaped string
         * @param[out] destination Pointer to the section of memory to write the
         *                         converted string to
         * @return Iterator to +1 the last character written
         * @tparam InputIt Input iterator used for the destination.
         * @tparam OutputIt Output iterator used for the source.
         */
        template<class InputIt, class OutputIt>
        InputIt percentEscapedToRealBytes(
                OutputIt start,
                OutputIt end,
                InputIt destination);

        //! List of characters in order for Base64 encoding.
        extern const std::array<const char, 64> base64Characters;

        //! Convert a binary container of data to a Base64 encoded container.
        /*!
         * If destination is a fixed size container, it should have a size of
         * at least ((end-start-1)/3 + 1)*4 not including null terminators if
         * used and assuming integer arithmetic.
         *
         * @param[in] start Iterator to start of binary data.
         * @param[in] end Iterator to end of binary data.
         * @param[out] destination Iterator to start of Base64 destination.
         *
         * @tparam In Input iterator type. Should be dereferenced to type char.
         * @tparam Out Output iterator type. Should be dereferenced to type
         *             char.
         */
        template<class In, class Out>
        void base64Encode(In start, In end, Out destination);

        //! Convert a Base64 encoded container to a binary container.
        /*!
         * If destination is a fixed size container, it should have a size of
         * at least (end-start)*3/4 not including null terminators if used.
         *
         * @param[in] start Iterator to start of Base64 data.
         * @param[in] end Iterator to end of Base64 data.
         * @param[out] destination Iterator to start of binary destination.
         *
         * @tparam In Input iterator type. Should be dereferenced to type char.
         * @tparam Out Output iterator type. Should be dereferenced to type
         *             char.
         *
         * @return Iterator to last position written+1 (The normal end()
         *                  iterator). If the return value equals destination,
         *                  an error occurred.
         */
        template<class In, class Out>
        Out base64Decode(In start, In end, Out destination);

        //! Defines ID values for HTTP sessions.
        /*!
         * @date    March 14, 2016
         * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
         */
        class SessionId
        {
        public:
            //! Size in bytes of the ID data
            static const int size=24;

        private:
            //! ID data
            std::array<unsigned char, size> m_data;

            //! Contains the time this session was last used
            mutable std::time_t m_timestamp;

            //! Set to true once the random number generator has been seeded
            static bool s_seeded;

            template<class T> friend class Sessions;
        public:
            //! This constructor initializes the ID data to a random value
            SessionId();

            SessionId(const SessionId& x):
                m_timestamp(x.m_timestamp)
            {
                std::copy(x.m_data.begin(), x.m_data.end(), m_data.begin());
            }

            const SessionId& operator=(const SessionId& x)
            {
                std::copy(x.m_data.begin(), x.m_data.end(), m_data.begin());
                m_timestamp=x.m_timestamp;
                return *this;
            }

            //! Assign the ID data with a base64 encoded string
            /*!
             * Note that only size*4/3 bytes will be read from the string.
             *
             * @param data_ Iterator set at begin of base64 encoded string
             */
            template<class charT> const SessionId& operator=(charT* data);

            //! Initialize the ID data with a base64 encoded string
            /*!
             * Note that only size*4/3 bytes will be read from the string.
             *
             * @param data_ Iterator set at begin of base64 encoded string
             */
            template<class charT> SessionId(charT* data)
            {
                *this=data;
            }

            template<class charT, class Traits>
            friend std::basic_ostream<charT, Traits>& operator<<(
                    std::basic_ostream<charT, Traits>& os,
                    const SessionId& x);

            bool operator<(const SessionId& x) const
            {
                return std::memcmp(
                        m_data.data(),
                        x.m_data.data(),
                        SessionId::size)<0;
            }

            bool operator==(const SessionId& x) const
            {
                return std::memcmp(
                        m_data.data(),
                        x.m_data.data(),
                        SessionId::size)==0;
            }

            //! Resets the last access timestamp to the current time.
            void refresh() const
            {
                m_timestamp = std::time(nullptr);
            }
        };

        //! Output the ID data in base64 encoding
        template<class charT, class Traits>
        std::basic_ostream<charT, Traits>& operator<<(
                std::basic_ostream<charT, Traits>& os,
                const SessionId& x)
        {
            base64Encode(
                    x.m_data.begin(),
                    x.m_data.end(),
                    std::ostream_iterator<charT, charT, Traits>(os));
            return os;
        }

        //! Container for HTTP sessions
        /*!
         *  In almost all ways this class behaves like an std::map. The sole
         *  addition is a mechanism for clearing out expired sessions based on
         *  a keep alive time and a frequency of deletion. The first part of
         *  the std::pair<> is a SessionId object, and the second is an object
         *  of class T (passed as the template parameter.
         *
         * @tparam T Class containing session data.
         *
         * @date    March 11, 2016
         * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
         */
        template<class T> class Sessions: public std::map<SessionId, T>
        {
        private:
            //! Amount of seconds to keep sessions around for.
            const unsigned int keepAlive;

            //! How often the container should find old sessions and purge.
            const unsigned int cleanupFrequency;

            //! The time that the next session cleanup should be done.
            std::time_t cleanupTime;
        public:
            typedef typename std::map<SessionId, T>::iterator iterator;

            typedef typename std::map<SessionId, T>::const_iterator const_iterator;

            //! Constructor takes session keep alive times and cleanup frequency.
            /*!
             * @param[in] keepAlive_ Amount of seconds a session will stay alive
             *                       for.
             * @param[in] cleanupFrequency_ How often (in seconds) the container
             *                              should find old sessions and delete
             *                              them.
             */
            Sessions(
                    unsigned int keepAlive_,
                    unsigned int cleanupFrequency_):
                keepAlive(keepAlive_),
                cleanupFrequency(cleanupFrequency_),
                cleanupTime(std::time(nullptr)+cleanupFrequency)
            {}

            //! Clean out old sessions.
            /*!
             * Calling this function will clear out any expired sessions from
             * the container. This function must be called for any cleanup to
             * take place, however calling may not actually cause a cleanup.
             * The amount of seconds specified in cleanupFrequency must have
             * expired since the last cleanup for it to actually take place.
             */
            void cleanup();

            //! Generates a new session pair with a random ID value
            /*!
             * @param[in] value_ Value to place into the data section.
             *
             * @return Iterator pointing to the newly created session.
             */
            iterator generate(const T& value_ = T());
        };
    }
}

template<class T> void Fastcgipp::Http::Sessions<T>::cleanup()
{
    if(std::time(nullptr) < cleanupTime)
        return;
    std::time_t oldest(std::time(nullptr)-keepAlive);
    iterator it = this->begin();
    while(it != this->end())
    {
        if(it->first.timestamp < oldest)
            it = erase(it);
        else
            ++it;
    }
    cleanupTime=std::time(nullptr)+cleanupFrequency;
}

template<class In, class Out>
Out Fastcgipp::Http::base64Decode(In start, In end, Out destination)
{
    Out dest=destination;

    for(int buffer, bitPos=-8, padStart; start!=end || bitPos>-6; ++dest)
    {
        if(bitPos==-8)
        {
            bitPos=18;
            padStart=-9;
            buffer=0;
            while(bitPos!=-6)
            {
                if(start==end) return destination;
                int value=*start++;
                if(value >= 'A' && 'Z' >= value) value -= 'A';
                else if(value >= 'a' && 'z' >= value) value -= 'a' - 26;
                else if(value >= '0' && '9' >= value) value -= '0' - 52;
                else if(value == '+') value = 62;
                else if(value == '/') value = 63;
                else if(value == '=') { padStart=bitPos; break; }
                else return destination;

                buffer |= value << bitPos;
                bitPos-=6;
            }
            bitPos=16;
        }

        *dest = (buffer >> bitPos) & 0xff;
        bitPos-=8;
        if(padStart>=bitPos)
        {
            if( (padStart-bitPos)/6 )
                return dest;
            else
                return ++dest;
        }
    }

    return dest;
}

template<class In, class Out>
void Fastcgipp::Http::base64Encode(In start, In end, Out destination)
{
    for(int buffer, bitPos=-6, padded; start!=end || bitPos>-6; ++destination)
    {
        if(bitPos==-6)
        {
            bitPos=16;
            buffer=0;
            padded=-6;
            while(bitPos!=-8)
            {
                if(start!=end)
                    buffer |= (uint32_t)*(unsigned char*)start++ << bitPos;
                else padded+=6;
                bitPos-=8;
            }
            bitPos=18;
        }

        if(padded == bitPos)
        {
            *destination='=';
            padded-=6;
        }
        else *destination=base64Characters[ (buffer >> bitPos)&0x3f ];
        bitPos -= 6;
    }
}

template<class T>
typename Fastcgipp::Http::Sessions<T>::iterator
Fastcgipp::Http::Sessions<T>::generate(const T& value_)
{
    std::pair<iterator,bool> retVal;
    retVal.second=false;
    while(!retVal.second)
        retVal=insert(std::pair<SessionId, T>(SessionId(), value_));
    return retVal.first;
}

#endif
