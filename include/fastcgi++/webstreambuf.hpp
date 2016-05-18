/*!
 * @file       webstreambuf.hpp
 * @brief      Declares the WebStreambuf stuff
 * @author     Eddie Carle &lt;eddie@isatec.ca&gt;
 * @date       May 2, 2016
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

#ifndef FASTCGIPP_WEBSTREAMBUF_HPP
#define FASTCGIPP_WEBSTREAMBUF_HPP

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
     * using Fastcgipp::Encoding;
     * out << Encoding::HTML << "<not html&>" << Encoding::NONE << "<htmltag>";
     * @endcode
     *
     * When output encoding is set to NONE, no character translation takes place.
     * HTML and URL encoding is described by the following table.
     *
     * <b>HTML</b>
     * <table>
     *  <tr>
     *      <td><b>Input</b></td>
     *      <td><b>Output</b></td>
     *  </tr>
     *  <tr>
     *      <td>&quot;</td>
     *      <td>&amp;quot;</td>
     *  </tr>
     *  <tr>
     *      <td>&gt;</td>
     *      <td>&amp;gt;</td>
     *  </tr>
     *  <tr>
     *      <td>&lt;</td>
     *      <td>&amp;lt;</td>
     *  </tr>
     *  <tr>
     *      <td>&amp;</td>
     *      <td>&amp;amp;</td>
     *  </tr>
     *  <tr>
     *      <td>&apos;</td>
     *      <td>&amp;apos;</td>
     *  </tr>
     * </table>
     *
     * <b>URL</b>
     * <table>
     *  <tr>
     *      <td><b>Input</b></td>
     *      <td><b>Output</b></td>
     *  </tr>
     *  <tr>
     *      <td>!</td>
     *      <td>\%21</td>
     *  </tr>
     *  <tr>
     *      <td>]</td>
     *      <td>\%5D</td>
     *  </tr>
     *  <tr>
     *      <td>[</td>
     *      <td>\%5B</td>
     *  </tr>
     *  <tr>
     *      <td>#</td>
     *      <td>\%23</td>
     *  </tr>
     *  <tr>
     *      <td>?</td>
     *      <td>\%3F</td>
     *  </tr>
     *  <tr>
     *      <td>/</td>
     *      <td>\%2F</td>
     *  </tr>
     *  <tr>
     *      <td>,</td>
     *      <td>\%2C</td>
     *  </tr>
     *  <tr>
     *      <td>$</td>
     *      <td>\%24</td>
     *  </tr>
     *  <tr>
     *      <td>+</td>
     *      <td>\%2B</td>
     *  </tr>
     *  <tr>
     *      <td>=</td>
     *      <td>\%3D</td>
     *  </tr>
     *  <tr>
     *      <td>&amp;</td>
     *      <td>\%26</td>
     *  </tr>
     *  <tr>
     *      <td>\@</td>
     *      <td>\%40</td>
     *  </tr>
     *  <tr>
     *      <td>:</td>
     *      <td>\%3A</td>
     *  </tr>
     *  <tr>
     *      <td>;</td>
     *      <td>\%3B</td>
     *  </tr>
     *  <tr>
     *      <td>)</td>
     *      <td>\%29</td>
     *  </tr>
     *  <tr>
     *      <td>(</td>
     *      <td>\%28</td>
     *  </tr>
     *  <tr>
     *      <td>'</td>
     *      <td>\%27</td>
     *  </tr>
     *  <tr>
     *      <td>*</td>
     *      <td>\%2A</td>
     *  </tr>
     *  <tr>
     *      <td>&lt;</td>
     *      <td>\%3C</td>
     *  </tr>
     *  <tr>
     *      <td>&gt;</td>
     *      <td>\%3E</td>
     *  </tr>
     *  <tr>
     *      <td>&quot;</td>
     *      <td>\%22</td>
     *  </tr>
     *  <tr>
     *      <td>*space*</td>
     *      <td>\%20</td>
     *  </tr>
     *  <tr>
     *      <td>\%</td>
     *      <td>\%25</td>
     *  </tr>
     * </table>
     *
     * @date    May 2, 2016
     * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
     */
    enum class Encoding
    {
        NONE,
        HTML,
        URL
    };

    template<class charT, class traits>
    std::basic_ostream<charT, traits>& operator<<(
            std::basic_ostream<charT, traits>& os,
            const Encoding& encoding);

    //! Stream buffer class for output of HTML/Web stream
    /*!
     * This class is derived from std::basic_streambuf<charT, traits>. It simply
     * adds the ability to handle the Encoding modifier.
     *
     * @tparam charT Character type (char or wchar_t)
     * @tparam traits Character traits
     *
     * @date    May 2, 2016
     * @author  Eddie Carle &lt;eddie@isatec.ca&gt;
     */
    template <class charT, class traits = std::char_traits<charT>>
    class WebStreambuf: public std::basic_streambuf<charT, traits>
    {
        typedef typename std::basic_streambuf<charT, traits>::char_type char_type;

        //! Needed for html encoding of stream data
        static const std::map<charT, const std::basic_string<charT>> htmlCharacters;

        //! Needed for url encoding of stream data
        static const std::map<charT, const std::basic_string<charT>> urlCharacters;

        //! Derived from std::basic_streambuf<charT, traits>
        std::streamsize xsputn(const char_type *s, std::streamsize n);

        //! Output encoding for stream buffer
        Encoding m_encoding;

        friend std::basic_ostream<charT, traits>& operator<< <charT, traits>(
                std::basic_ostream<charT, traits>& os,
                const Encoding& encoding);
    public:
        WebStreambuf():
            m_encoding(Encoding::NONE)
        {}
    };
}

#endif
