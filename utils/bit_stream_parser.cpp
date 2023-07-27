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

#include "bit_stream_parser.h"
#include "bit_stream_parser_h265.h"

BitStreamParser::~BitStreamParser() {}

BitStreamParserPtr BitStreamParser::Create(DataStream* pStream, BitStreamType type, int nSize, int64_t pts){
    BitStreamParserPtr pParser;

    switch(type)
    {
    case BitStreamH264AnnexB:
        //pParser = BitStreamParserPtr(CreateH264Parser(pStream, pContext));
        ERR ( STR ("Error: ") + TOSTR(static_cast<int>(PARSER_NOT_IMPLEMENTED)));
        break;
    case BitStream265AnnexB:
        pParser = BitStreamParserPtr(CreateHEVCParser(pStream, nSize, pts));
        break;
    default:
        ERR ( STR ("Error: ") + TOSTR(static_cast<int>(PARSER_NOT_SUPPORTED)));
    }
    return pParser;
}