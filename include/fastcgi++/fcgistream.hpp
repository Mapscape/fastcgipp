/*!
 * @file       fcgistream.hpp
 * @brief      Declares the fastcgi++ stream
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       May 1, 2016
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

#ifndef FASTCGIPP_FCGISTREAM_HPP
#define FASTCGIPP_FCGISTREAM_HPP

#include "fastcgi++/protocol.hpp"

#include <map>
#include <streambuf>

//! Topmost namespace for the fastcgi++ library
namespace Fastcgipp
{
	//! Stream manipulator for setting output encoding.
	/*!
	 * This simple stream manipulator can set the output encoding of Fcgistream
	 * objects by
	 *
	 * @code
     * out << Fastcgipp::encoding(Fastcgipp::encoding::HTML);
     * @endcode
     *
	 * When output encoding is set to NONE, no character translation takes place.
	 * HTML and URL encoding is described by the following table. 
	 *
	 * <b>HTML</b>
	 * <table>
	 * 	<tr>
	 * 		<td><b>Input</b></td>
	 * 		<td><b>Output</b></td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&quot;</td>
	 * 		<td>&amp;quot;</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&gt;</td>
	 * 		<td>&amp;gt;</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&lt;</td>
	 * 		<td>&amp;lt;</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&amp;</td>
	 * 		<td>&amp;amp;</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&apos;</td>
	 * 		<td>&amp;apos;</td>
	 * 	</tr>
	 * </table>
	 *
	 * <b>URL</b>
	 * <table>
	 * 	<tr>
	 * 		<td><b>Input</b></td>
	 * 		<td><b>Output</b></td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>!</td>
	 * 		<td>\%21</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>]</td>
	 * 		<td>\%5D</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>[</td>
	 * 		<td>\%5B</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>#</td>
	 * 		<td>\%23</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>?</td>
	 * 		<td>\%3F</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>/</td>
	 * 		<td>\%2F</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>,</td>
	 * 		<td>\%2C</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>$</td>
	 * 		<td>\%24</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>+</td>
	 * 		<td>\%2B</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>=</td>
	 * 		<td>\%3D</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&amp;</td>
	 * 		<td>\%26</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>\@</td>
	 * 		<td>\%40</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>:</td>
	 * 		<td>\%3A</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>;</td>
	 * 		<td>\%3B</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>)</td>
	 * 		<td>\%29</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>(</td>
	 * 		<td>\%28</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>'</td>
	 * 		<td>\%27</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>*</td>
	 * 		<td>\%2A</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&lt;</td>
	 * 		<td>\%3C</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&gt;</td>
	 * 		<td>\%3E</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>&quot;</td>
	 * 		<td>\%22</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>*space*</td>
	 * 		<td>\%20</td>
	 * 	</tr>
	 * 	<tr>
	 * 		<td>\%</td>
	 * 		<td>\%25</td>
	 * 	</tr>
	 * </table>
     *
     * @date    May 1, 2016
     * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
	 */
	struct encoding
	{
        enum OutputEncoding
        {
            NONE,
            HTML,
            URL
        };
		const OutputEncoding type;
		encoding(OutputEncoding type_): type(type_) {}
	};

    //! Stream buffer class for output of client data through FastCGI
    /*!
     * This class is derived from std::basic_streambuf<charT, traits>. It acts
     * just the same as any stream buffer does with the added feature of the
     * dump() function plus the ability to handle the encoding modifier.
     *
     * @tparam charT Character type (char or wchar_t)
     * @tparam traits Character traits
     *
     * @date    May 1, 2016
     * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
     */
    template <class charT, class traits>
    class Fcgibuf: public std::basic_streambuf<charT, traits>
    {
    public:
        //! Constructor
        /*!
         * Sets FastCGI related member data necessary for operation of the
         * stream buffer.
         *
         * @param[in] id Complete ID associated with the request
         * @param[in] type Type of output stream (ERR or OUT)
         * @param[in] send_ Function to send record with
         */
        Fcgibuf(
                const Protocol::RequestId& id,
                const Protocol::RecordType& type,
                const std::function<void(const Socket&, std::vector<char>&&)>&
                    send_):
            m_id(id),
            m_type(type),
            send(send_),
            m_encoding(encoding::NONE)
        {
            setp(m_buffer, m_buffer+s_buffSize);
        }

        virtual ~Fcgibuf()
        {
            try
            {
                sync();
            }
            catch(...)
            {}
        }

        //! Dumps raw data directly into the FastCGI protocol
        /*!
         * This function exists as a mechanism to dump raw data out the stream
         * bypassing the stream buffer or any code conversion mechanisms. If the
         * user has any binary data to send, this is the function to do it with.
         *
         * @param[in] data Pointer to first byte of data to send
         * @param[in] size Size in bytes of data to be sent
         */
        void dump(char* data, size_t size)
        {
            m_buffer.dump(data, size);
        }

        //! Dumps an input stream directly into the FastCGI protocol
        /*!
         * This function exists as a mechanism to dump a raw input stream out
         * this stream bypassing the stream buffer or any code conversion
         * mechanisms. Typically this would be a filestream associated with an
         * image or something. The stream is transmitted until an EOF.
         *
         * @param[in] stream Reference to input stream that should be
         *                   transmitted.
         */
        void dump(std::basic_istream<char>& stream);

        //! Set the output encoding of the stream buffer
        void encoding(encoding::OutputEncoding enc)
        {
            m_encoding = enc;
        }

    private:
        typedef typename std::basic_streambuf<charT, traits>::int_type int_type;
        typedef typename std::basic_streambuf<charT, traits>::traits_type traits_type;
        typedef typename std::basic_streambuf<charT, traits>::char_type char_type;

        //! Needed for html encoding of stream data
        static const std::map<charT, const std::basic_string<charT>> htmlCharacters;

        //! Needed for url encoding of stream data
        static const std::map<charT, const std::basic_string<charT>> urlCharacters;

        int_type overflow(int_type c = traits_type::eof());

        int sync()
        {
            return emptyBuffer()?0:-1;
        }

        std::streamsize xsputn(const char_type *s, std::streamsize n);

        //! Code converts, packages and transmits all data in the stream buffer
        bool emptyBuffer();

        //! Size of the internal stream buffer
        static const int s_buffSize = 8192;

        //! The buffer
        char_type m_buffer[s_buffSize];

        //! ID associated with the request
        const Protocol::RequestId m_id;

        //! Type of output stream (ERR or OUT)
        const Protocol::RecordType m_type;

        //! Function to actually send the record
        const std::function<void(const Socket&, std::vector<char>&&)> send;

        //! Output encoding for stream buffer
        encoding::OutputEncoding m_encoding;
    };

    //! Stream class for output of client data through FastCGI
    /*!
     * This class is derived from std::basic_ostream<charT, traits>. It acts just
     * the same as any stream does with the added feature of the dump() function.
     *
     * @tparam charT Character type (char or wchar_t)
     * @tparam traits Character traits
     *
     * @date    May 1, 2016
     * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
     */
    template <class charT, class traits>
    class Fcgistream: public std::basic_ostream<charT, traits>
    {
    public:
        //! Arguments passed directly to Fcgibuf::Fcgibuf()
        Fcgistream(
                const Protocol::RequestId& id,
                const Protocol::RecordType& type,
                const std::function<void(const Socket&, std::vector<char>&&)>&
                    send):
            std::basic_ostream<charT, traits>(&m_buffer),
            m_buffer(id, type, send)
        {}
        
        //! Dumps raw data directly into the FastCGI protocol
        /*!
         * This function exists as a mechanism to dump raw data out the stream
         * bypassing the stream buffer or any code conversion mechanisms. If the
         * user has any binary data to send, this is the function to do it with.
         *
         * @param[in] data Pointer to first byte of data to send
         * @param[in] size Size in bytes of data to be sent
         */
        void dump(char* data, size_t size)
        {
            m_buffer.dump(data, size);
        }

        //! Dumps an input stream directly into the FastCGI protocol
        /*!
         * This function exists as a mechanism to dump a raw input stream out
         * this stream bypassing the stream buffer or any code conversion
         * mechanisms. Typically this would be a filestream associated with an
         * image or something. The stream is transmitted until an EOF.
         *
         * @param[in] stream Reference to input stream that should be transmitted.
         */
        void dump(std::basic_istream<char>& stream)
        {
            m_buffer.dump(stream);
        }

        //! Set the output encoding of the stream
        void encoding(encoding::OutputEncoding enc)
        {
            m_buffer.encoding(enc);
        }

    private:
        //! Stream buffer object
        Fcgibuf<charT, traits> m_buffer;
    };

	template<class charT, class Traits>
    std::basic_ostream<charT, Traits>& operator<<(
            std::basic_ostream<charT, Traits>& os,
            const encoding& enc);
}

#endif
