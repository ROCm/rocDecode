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

#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <vector>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include <libgen.h>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
#else
    #include <experimental/filesystem>
#endif
#include "video_demuxer.h"
#include "roc_video_dec.h"

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-z force_zero_latency (force_zero_latency, Decoded frames will be flushed out for display immediately); optional;" << std::endl
    << "-sei extract SEI messages; optional;" << std::endl
    << "-crop crop rectangle for output (not used when using interopped decoded frame); optional; default: 0" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string input_file_path, output_file_path;
    int dump_output_frames = 0;
    int device_id = 0;
    bool b_force_zero_latency = false;     // false by default: enabling this option might affect decoding performance
    bool b_extract_sei_messages = false;
    Rect crop_rect = {};
    Rect *p_crop_rect = nullptr;
    OutputSurfaceMemoryType mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;        // set to internal
    // Parse command-line arguments
    if(argc < 1) {
        ShowHelpAndExit();
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            ShowHelpAndExit();
        }
        if (!strcmp(argv[i], "-i")) {
            if (++i == argc) {
                ShowHelpAndExit("-i");
            }
            input_file_path = argv[i];
            continue;
        }
        if (!strcmp(argv[i], "-o")) {
            if (++i == argc) {
                ShowHelpAndExit("-o");
            }
            output_file_path = argv[i];
            dump_output_frames = 1;
            continue;
        }
        if (!strcmp(argv[i], "-d")) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            device_id = atoi(argv[i]);
            continue;
        }
        if (!strcmp(argv[i], "-z")) {
            if (i == argc) {
                ShowHelpAndExit("-z");
            }
            b_force_zero_latency = true;
            continue;
        }
        if (!strcmp(argv[i], "-sei")) {
            if (i == argc) {
                ShowHelpAndExit("-sei");
            }
            b_extract_sei_messages = true;
            continue;
        }
        if (!strcmp(argv[i], "-crop")) {
            if (++i == argc || 4 != sscanf(argv[i], "%d,%d,%d,%d", &crop_rect.l, &crop_rect.t, &crop_rect.r, &crop_rect.b)) {
                ShowHelpAndExit("-crop");
            }
            if ((crop_rect.r - crop_rect.l) % 2 == 1 || (crop_rect.b - crop_rect.t) % 2 == 1) {
                std::cout << "output crop rectangle must have width and height of even numbers" << std::endl;
                exit(1);
            }
            p_crop_rect = &crop_rect;
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }
    try {
        VideoDemuxer demuxer(input_file_path.c_str());
        rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer.GetCodecID());
        RocVideoDecoder viddec(device_id, mem_type, rocdec_codec_id, false, b_force_zero_latency, p_crop_rect, b_extract_sei_messages);

        std::string device_name, gcn_arch_name;
        int pci_bus_id, pci_domain_id, pci_device_id;

        viddec.GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
        std::cout << "info: Using GPU device " << device_id << " - " << device_name << "[" << gcn_arch_name << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pci_bus_id << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pci_domain_id << "." << pci_device_id << std::dec << std::endl;
        std::cout << "info: decoding started, please wait!" << std::endl;

        int n_video_bytes = 0, n_frame_returned = 0, n_frame = 0;
        uint8_t *pvideo = nullptr;
        uint8_t *pframe = nullptr;
        int64_t pts = 0;
        OutputSurfaceInfo *surf_info;
        uint32_t width, height;
        double total_dec_time = 0;

        do {
            auto start_time = std::chrono::high_resolution_clock::now();
            demuxer.Demux(&pvideo, &n_video_bytes, &pts);
            n_frame_returned = viddec.DecodeFrame(pvideo, n_video_bytes, 0, pts);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto time_per_frame = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            total_dec_time += time_per_frame;
            if (!n_frame && !viddec.GetOutputSurfaceInfo(&surf_info)) {
                std::cerr << "Error: Failed to get Output Surface Info!" << std::endl;
                break;
            }
            for (int i = 0; i < n_frame_returned; i++) {
                pframe = viddec.GetFrame(&pts);
                if (dump_output_frames) {
                    viddec.SaveFrameToFile(output_file_path, pframe, surf_info);
                }
                // release frame
                viddec.ReleaseFrame(pts);
            }
            n_frame += n_frame_returned;
        } while (n_video_bytes);

        std::cout << "info: Total frame decoded: " << n_frame << std::endl;
        if (!dump_output_frames) {
            std::cout << "info: avg decoding time per frame (ms): " << total_dec_time / n_frame << std::endl;
            std::cout << "info: avg FPS: " << (n_frame / total_dec_time) * 1000 << std::endl;
        }
    } catch (const std::exception &ex) {
      std::cout << ex.what() << std::endl;
      exit(1);
    }

    return 0;
}
