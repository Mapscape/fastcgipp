//! \file protocol.hpp Defines FastCGI protocol
/***************************************************************************
* Copyright (C) 2007 Eddie Carle [eddie@erctech.org]                       *
*                                                                          *
* This file is part of fastcgi++.                                          *
*                                                                          *
* fastcgi++ is free software: you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as  published   *
* by the Free Software Foundation, either version 3 of the License, or (at *
* your option) any later version.                                          *
*                                                                          *
* fastcgi++ is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or    *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public     *
* License for more details.                                                *
*                                                                          *
* You should have received a copy of the GNU Lesser General Public License *
* along with fastcgi++.  If not, see <http://www.gnu.org/licenses/>.       *
****************************************************************************/


#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <memory>
#include <type_traits>
#include <cstdint>
#include <algorithm>

#include "fastcgi++/message.hpp"
#include "fastcgi++/sockets.hpp"

//! Topmost namespace for the fastcgi++ library
namespace Fastcgipp
{
    //! Defines the fastcgi++ version
    extern const char version[];

    //! Defines aspects of the FastCGI %Protocol
    /*!
     * The %Protocol namespace defines the data structures and constants used
     * by the FastCGI protocol version 1. All data has been modelled after the
     * official FastCGI protocol specification located at
     * http://www.fastcgi.com/devkit/doc/fcgi-spec.html
     */
    namespace Protocol
    {
        //! The internal ID of a FastCGI request
        typedef uint16_t FcgiId;
        
        //! A unique identifier for each FastCGI request
        /*!
         * Because each FastCGI request has both a RequestID and a Socket
         * associated with it, this class defines an ID value that encompasses
         * both.
         */
        struct RequestId
        {
            //! Construct from an FcgiId and a Socket
            /*!
             * The constructor builds upon a RequestID and the Socket it is
             * communicating through.
             *
             * @param [in] id The internal FastCGI request ID.
             * @param [in] socket The associated socket.
             */
            RequestId(
                    FcgiId id,
                    const Socket& socket):
                m_socket(socket),
                m_id(id)
            {} 

            RequestId(const RequestId& x):
                m_socket(x.m_socket),
                m_id(x.m_id)
            {}
            
            //! Associated socket
            const Socket m_socket;

            //! Internal FastCGI request ID
            const FcgiId m_id;

            //! We need this to allow the objects to be in sorted containers.
            bool operator<(const RequestId& x) const
            {
                if(m_socket < x.m_socket)
                    return true;
                else if(m_socket == x.m_socket)
                    return m_id < x.m_id;
                return false;
            }

            //! We need this to allow the objects to be in sorted containers.
            bool operator==(const RequestId& x) const
            {
                return m_socket == x.m_socket && m_id == x.m_id;
            }
        };
        
        //! Defines the types of records within the FastCGI protocol
        enum class RecordType: uint8_t
        {
            BEGIN_REQUEST=1,
            ABORT_REQUEST=2,
            END_REQUEST=3,
            PARAMS=4,
            IN=5,
            OUT=6,
            ERR=7,
            DATA=8,
            GET_VALUES=9,
            GET_VALUES_RESULT=10,
            UNKNOWN_TYPE=11
        };
        
        //! The version of the FastCGI protocol that this adheres to
        const int version=1;

        //! All FastCGI records will be a multiple of this many bytes
        const int chunkSize=8;

        //! Defines the possible roles a FastCGI application may play
        enum class Role: uint16_t
        {
            RESPONDER=1,
            AUTHORIZER=2,
            FILTER=3
        };

        //! Possible statuses a request may declare when complete
        enum class ProtocolStatus: uint8_t
        {
            REQUEST_COMPLETE=0,
            CANT_MPX_CONN=1,
            OVERLOADED=2,
            UNKNOWN_ROLE=3
        };

        //! Allows raw storage of types in big endian format
        /*!
         * This templated class allows any integral based (enumerations
         * included) type to be stored in big endian format but maintain type
         * compatibility.
         */
        template<typename T> class BigEndian
        {
        private:
            typedef typename std::make_unsigned<T>::type BaseType;

            //! The raw data of the big endian integer
            unsigned char m_data[sizeof(T)];

            //! Set the internal data to the passed parameter.
            void set(T x)
            {
                BaseType& y = *(BaseType*)&x;

                for(unsigned int i=0; i<sizeof(T); ++i)
                    m_data[i] = (unsigned char)(0xff&(y>>8*(sizeof(T)-1-i)));
            }

        public:
            BigEndian& operator=(T x)
            {
                set(x);
                return *this;
            }

            BigEndian(T x)
            {
                set(x);
            }

            BigEndian()
            {}

            operator T() const
            {
                return read(m_data);
            }

            static T read(const unsigned char* source)
            {
                BaseType x=0;

                for(unsigned int i=0; i<sizeof(T); ++i)
                    x |= ((BaseType)(*(source+i)) << 8*(sizeof(T)-1-i));

                return *(T*)&x;
            }

            static T read(const char* source)
            {
                return read((const unsigned char*)source);
            }
        };

        //! Data structure used as the header for FastCGI records
        /*!
         * This structure defines the header used in FastCGI records. It can be
         * casted to and from raw 8 byte blocks of data and transmitted/received
         * as is.
         */
        struct Header
        {
            //! FastCGI version number
            uint8_t version;

            //! Record type
            RecordType type;

            //! Request ID
            BigEndian<FcgiId> fcgiId;

            //! Content length
            BigEndian<uint16_t> contentLength;

            //! Length of record padding
            uint8_t paddingLength;

            //! Reseved for future use and header padding
            uint8_t reserved;
        };

        //! The body for FastCGI records with a RecordType of BEGIN_REQUEST
        /*!
         * This structure defines the body used in FastCGI BEGIN_REQUEST
         * records. It can be casted from raw 8 byte blocks of data and received
         * as is. A BEGIN_REQUEST record is received when the other side wished
         * to make a new request.
         */
        struct BeginRequest
        {
            //! Flag bit representing the keep alive value
            static const int keepConnBit = 1;

            //!Get keep alive value from the record body
            /*!
             * If this value is false, the socket should be closed on our side when the request is complete.
             * If true, the other side will close the socket when done and potentially reuse the socket and
             * multiplex other requests on it.
             *
             * @return Boolean value as to whether or not the connection is kept alive
             */
            bool getKeepConn() const { return flags & keepConnBit; }

            //! Role
            BigEndian<Role> role;
            
            //! Flag value
            uint8_t flags;

            //! Reseved for future use and body padding
            uint8_t reserved[5];
        };

        //! The body for FastCGI records with a RecordType of UNKNOWN_TYPE
        /*!
         * This structure defines the body used in FastCGI UNKNOWN_TYPE records.
         * It can be casted to raw 8 byte blocks of data and transmitted as is.
         * An UNKNOWN_TYPE record is sent as a reply to record types that are
         * not recognized.
         */
        struct UnknownType
        {
            //! Unknown record type
            RecordType type;

            //! Reseved for future use and body padding
            uint8_t reserved[7];
        };

        //! The body for FastCGI records with a RecordType of END_REQUEST
        /*!
         * This structure defines the body used in FastCGI END_REQUEST records.
         * It can be casted to raw 8 byte blocks of data and transmitted as is.
         * An END_REQUEST record is sent when this side wishes to terminate a
         * request. This can be simply because it is complete or because of a
         * problem.
         */
        struct EndRequest
        {
            //! Return value
            BigEndian<int32_t> appStatus;

            //! Requests Status
            ProtocolStatus protocolStatus;

            //! Reseved for future use and body padding
            uint8_t reserved[3];
        };

        //! Process the body of a FastCGI parameter record
        /*!
         * Takes the body of a FastCGI record of type PARAMETER and parses it.
         * You end up with a pointer/size for both the name and value of the
         * parameter.
         *
         * @param[in] data Pointer to the record body
         * @param[in] dataSize Size of data pointed to by data
         * @param[out] name Reference to a pointer that will be pointed to the
         *                  first byte of the parameter name
         * @param[out] nameSize Reference to a value to will be given the size
         *                      in bytes of the parameter name
         * @param[out] value Reference to a pointer that will be pointed to the
         *                   first byte of the parameter value
         * @param[out] valueSize Reference to a value to will be given the size
         *                       in bytes of the parameter value
         */
        void processParamHeader(
                const char* data,
                size_t dataSize,
                const char*& name,
                size_t& nameSize,
                const char*& value,
                size_t& valueSize);
        
        //! For the reply of FastCGI management records of type GET_VALUES
        /*!
         * This class template is an efficient tool for replying to GET_VALUES
         * management records. The structure represents a complete record
         * (body+header) of a name-value pair to be sent as a reply to a
         * management value query. The templating allows the structure to be
         * exactly the size that is needed so it can be casted to raw data and
         * transmitted as is. Note that the name and value lengths are left as
         * single bytes so they are limited in range from 0-127.
         *
         * @tparam NAMELENGTH Length of name in bytes (0-127). Null terminator
         *                    not included.
         * @tparam VALUELENGTH Length of value in bytes (0-127). Null terminator
         *                     not included.
         * @tparam PADDINGLENGTH Length of padding at the end of the record.
         *                       This is needed to keep the record size a
         *                       multiple of chunkSize.
         */
        template<int NAMELENGTH, int VALUELENGTH, int PADDINGLENGTH>
        struct ManagementReply
        {
        private:
            //! Management records header
            Header header;

            //! Length in bytes of name
            uint8_t nameLength;

            //! Length in bytes of value
            uint8_t valueLength;

            //! Name data
            uint8_t name[NAMELENGTH];

            //! Value data
            uint8_t value[VALUELENGTH];

            //! Padding data
            uint8_t padding[PADDINGLENGTH];

        public:
            //! Construct the record based on the name data and value data
            /*!
             * A full record is constructed from the name-value data. After
             * construction the structure can be casted to raw data and
             * transmitted as is. The size of the data arrays pointed to by
             * name_ and value_ are assumed to correspond with the NAMELENGTH
             * and PADDINGLENGTH template parameters passed to the class.
             *
             * @param[in] name_ Pointer to name data
             * @param[in] value_ Pointer to value data
             */
            ManagementReply(
                    const char* name_,
                    const char* value_):
                nameLength(NAMELENGTH),
                valueLength(VALUELENGTH)
            {
                std::copy(name_, name_+NAMELENGTH, name);
                std::copy(value_, value_+VALUELENGTH, value);
                header.version = version;
                header.type = RecordType::GET_VALUES_RESULT;
                header.fcgiId = 0;
                header.contentLength = NAMELENGTH+VALUELENGTH;
                header.paddingLength= PADDINGLENGTH;
            }
        };

        //! Reply record that will be sent when asked the maximum allowed file descriptors open at a time
        extern const ManagementReply<14, 2, 8> maxConnsReply;

        //! Reply record that will be sent when asked the maximum allowed requests at a time
        extern const ManagementReply<13, 2, 1> maxReqsReply;

        //! Reply record that will be sent when asked if requests can be multiplexed over a single connections
        extern const ManagementReply<15, 1, 8> mpxsConnsReply;
    }
}

#endif
