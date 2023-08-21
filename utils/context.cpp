/*amf_ptsdure
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

#include "context.h"

ParserContext::ParserContext () {
    ParserBuffer *pNewBuffer;
}

PARSER_RESULT ParserContext::AllocBuffer(PARSER_MEMORY_TYPE type, size_t size/*, ParserBuffer** ppBuffer*/) {
    PARSER_RESULT res = PARSER_OK;
    /*switch(type) { 
        case PARSER_MEMORY_HOST: {
            ParserBuffer* pNewBuffer;
            if (ppBuffer != NULL) {
                ppBuffer = &pNewBuffer;
                (*ppBuffer)->SetSize(size);
            }
            res = PARSER_OK;
        }
        break;
        case PARSER_MEMORY_HIP: {
            res = PARSER_NOT_IMPLEMENTED;
        }
        break;
        case PARSER_MEMORY_UNKNOWN:{
            res = PARSER_NOT_IMPLEMENTED;
        }
        break;
        default: {
            res = PARSER_INVALID_ARG;
        }
        break;       
    }*/
    return res;
}

PARSER_RESULT ParserContext::Terminate() {
    return PARSER_OK;
}