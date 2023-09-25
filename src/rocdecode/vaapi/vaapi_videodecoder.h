/*
Copyright (c) 2023 - 2023 Advanced Micro Devices, Inc. All rights reserved.

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


// class implementing highlevel vaapi decoder
/***********************************************************************************************************/
//! requirement is as follows
//!
//! In order to minimize decode latencies, there should be always at least enough pictures (min 2) in the decode
//! queue at any time, in order to make sure that all VCN decode engines are always busy.
//! In addition to the reqular create and destroy: the decoder needs to have a task-Q for submitting decoding jobs from the high level
//!
//! Overall data flow:
//!  - GetCaps(...)
//!  - CreateVideoDecoder(...)
//!  - For each picture:
//!    + submitDecodeTask(0)        /* submit first frame for decoding */
//!    + ...                        /* submit next frame for decoding */
//!    + submitDecodeTask(N)        /* N is determined based on available HW decode engines in the system */
//!
//!    + QueryStatus(N-4)           /* Query the decode status of N-4 frame */
//!    + MapVideoFrame(N-4)
//!    + do some processing in HIP
//!    + UnmapVideoFrame(N-4)
//!    + submitDecodeTask(N+1)
//!    + MapVideoFrame(N-3)
//!    + ...
//!  - DestroyVideoDecoder(...)
//!
//! NOTE:
//! - The decoder has to maintain a Q for decode jobs with associated picture buffers
//! - An intenal thread has to pick up the next available job: If none wait for the Q to fill
/***********************************************************************************************************/

class vaapiVideoDecoder {
public:

};