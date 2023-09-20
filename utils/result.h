/*
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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

/*!
 * \file
 * \brief The Parser results.
 *
 * \defgroup group_parser_result Parser result definitions.
 * \brief Bit Stream Parser Result codes.
 */

#ifndef RESULT_H
#define RESULT_H

typedef enum PARSER_RESULT {
    PARSER_OK                                   = 0,
    PARSER_FAIL                                    ,

// common errors
    PARSER_UNEXPECTED                              ,

    PARSER_ACCESS_DENIED                           ,
    PARSER_INVALID_ARG                             ,
    PARSER_OUT_OF_RANGE                            ,

    PARSER_OUT_OF_MEMORY                           ,
    PARSER_INVALID_POINTER                         ,

    PARSER_NO_INTERFACE                            ,
    PARSER_NOT_IMPLEMENTED                         ,
    PARSER_NOT_SUPPORTED                           ,
    PARSER_NOT_FOUND                               ,

    PARSER_ALREADY_INITIALIZED                     ,
    PARSER_NOT_INITIALIZED                         ,

    PARSER_INVALID_FORMAT                          ,// invalid data format

    PARSER_WRONG_STATE                             ,
    PARSER_FILE_NOT_OPEN                           ,// cannot open file
    PARSER_STREAM_NOT_ALLOCATED                    ,

// device common codes
    PARSER_NO_DEVICE                               ,

// component common codes

    //result codes
    PARSER_EOF                                     ,
    PARSER_REPEAT                                  ,
    PARSER_INPUT_FULL                              ,//returned by AMFComponent::SubmitInput if input queue is full
    PARSER_RESOLUTION_CHANGED                      ,//resolution changed client needs to Drain/Terminate/Init
    PARSER_RESOLUTION_UPDATED                      ,//resolution changed in adaptive mode. New ROI will be set on output on newly decoded frames

    //error codes
    PARSER_INVALID_DATA_TYPE                       ,//invalid data type
    PARSER_INVALID_RESOLUTION                      ,//invalid resolution (width or height)
    PARSER_CODEC_NOT_SUPPORTED                     ,//codec not supported
    PARSER_SURFACE_FORMAT_NOT_SUPPORTED            ,//surface format not supported
} PARSER_RESULT;

#endif /* RESULT_H */