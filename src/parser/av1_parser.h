/*
Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#pragma once

#include "av1_defines.h"
#include "roc_video_parser.h"

class Av1VideoParser : public RocVideoParser {
public:
    /*! \brief Av1VideoParser constructor
     */
    Av1VideoParser();

    /*! \brief Av1VideoParser destructor
     */
    virtual ~Av1VideoParser();

    /*! \brief Function to Initialize the parser
     * \param [in] p_params Input of <tt>RocdecParserParams</tt> with codec type to initialize parser.
     * \return <tt>rocDecStatus</tt> Returns success on completion, else error code for failure
     */
    virtual rocDecStatus Initialize(RocdecParserParams *p_params);

    /*! \brief Function to Parse video data: Typically called from application when a demuxed picture is ready to be parsed
     * \param [in] p_data Pointer to picture data of type <tt>RocdecSourceDataPacket</tt>
     * @return <tt>rocDecStatus</tt> Returns success on completion, else error_code for failure
     */
    virtual rocDecStatus ParseVideoData(RocdecSourceDataPacket *p_data);

    /*! \brief function to uninitialize AV1 parser
     * @return rocDecStatus 
     */
    virtual rocDecStatus UnInitialize();     // derived method
};