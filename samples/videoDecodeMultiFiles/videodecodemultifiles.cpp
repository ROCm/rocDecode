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
#include <fstream>
#include <sstream>
#include <chrono>
#include <deque>
#include <sys/stat.h>
#include <libgen.h>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
#else
    #include <experimental/filesystem>
#endif
#include "video_demuxer.h"
#include "roc_video_dec.h"

typedef struct {
    std::string in_file;
    std::string out_file;
    bool b_force_zero_latency;
    bool b_extract_sei_messages;
    Rect crop_rect;
    Rect *p_crop_rect;
    int dump_output_frames;
} FileInfo;

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File List - required (text file containing all files to decode in below format)" << std::endl
    << "example.txt:" << std::endl
    << "infile input1.[mp4/mov...] (Input file path)" << std::endl
    << "outfile output1.yuv (Output file path)" << std::endl
    << "z 0 (force_zero_latency - Decoded frames will be flushed out for display immediately; default: 0)" << std::endl
    << "sei 0 (extract SEI messages; default: 0)" << std::endl
    << "crop l,t,r,b (crop rectangle for output (not used when using interopped decoded frame); default: 0)" << std::endl
    << "infile input2.[mp4/mov...]" << std::endl
    << "outfile output2.yuv" << std::endl
    << "...." << std::endl
    << "...." << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl;
    exit(0);
}

void ParseCommandLine(std::deque<FileInfo> *multi_file_data, int &device_id, int argc, char *argv[]) {

    FileInfo file_data;
    std::string file_list_path;

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
            file_list_path = argv[i];
            continue;
        }
        if (!strcmp(argv[i], "-d")) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            device_id = atoi(argv[i]);
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    // Parse the input filelist
    std::ifstream filestream(file_list_path);
    std::string line;
    char* str;
    char param[256];
    char value[256];
    int file_idx = 0;

    while (std::getline(filestream, line)) {
        str = (char *)line.c_str();
        sscanf(str,"%s %s", param, value);
        if (!strcmp(param, "infile")) {
            if (file_idx > 0) {
                multi_file_data->push_back(file_data);
            }
            file_data.in_file = value;
            file_idx++;
            file_data.b_force_zero_latency = false;
            file_data.b_extract_sei_messages = false;
            file_data.dump_output_frames = 0;
            file_data.crop_rect = {0, 0, 0, 0};
        }
        else if (!strcmp(param, "outfile")) {
            file_data.out_file = value;
            file_data.dump_output_frames = 1;
        }
        else if (!strcmp(param, "z")) {
            file_data.b_force_zero_latency = atoi(value) ? true : false;
        }
        else if (!strcmp(param, "sei")) {
            file_data.b_extract_sei_messages = atoi(value) ? true : false;
        }
        else if (!strcmp(param, "crop")) {
            sscanf(value, "%d,%d,%d,%d", &file_data.crop_rect.l, &file_data.crop_rect.t, &file_data.crop_rect.r, &file_data.crop_rect.b);
            if ((file_data.crop_rect.r - file_data.crop_rect.l) % 2 == 1 || (file_data.crop_rect.b - file_data.crop_rect.t) % 2 == 1) {
                std::cout << "Cropping rect must have width and height of even numbers" << std::endl;
                exit(1);
            }
            file_data.p_crop_rect = &file_data.crop_rect;
        }
    }
    if (file_idx > 0) {
        multi_file_data->push_back(file_data);
    }
}

int main(int argc, char **argv) {

    std::deque<FileInfo> multi_file_data;
    FileInfo file_data;
    int device_id = 0;
    OutputSurfaceMemoryType mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;        // set to internal

    ParseCommandLine (&multi_file_data, device_id, argc, argv);

    try {
        while (!multi_file_data.empty()) {
            file_data = multi_file_data.front();
            multi_file_data.pop_front();
            VideoDemuxer demuxer(file_data.in_file.c_str());
            rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer.GetCodecID());
            RocVideoDecoder viddec(device_id, mem_type, rocdec_codec_id, false, file_data.b_force_zero_latency, file_data.p_crop_rect, file_data.b_extract_sei_messages);

            std::string device_name, gcn_arch_name;
            int pci_bus_id, pci_domain_id, pci_device_id;

            viddec.GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
            std::cout << "info: Using GPU device " << device_id << " - " << device_name << "[" << gcn_arch_name << "] on PCI bus " <<
            std::setfill('0') << std::setw(2) << std::right << std::hex << pci_bus_id << ":" << std::setfill('0') << std::setw(2) <<
            std::right << std::hex << pci_domain_id << "." << pci_device_id << std::dec << std::endl;
            std::cout << "info: decoding started, please wait!" << std::endl;

            int n_video_bytes = 0, n_frame_returned = 0, n_frame = 0;
            uint8_t *pvideo = nullptr;
            int pkg_flags = 0;
            uint8_t *pframe = nullptr;
            int64_t pts = 0;
            OutputSurfaceInfo *surf_info;
            uint32_t width, height;
            double total_dec_time = 0;

            do {
                auto start_time = std::chrono::high_resolution_clock::now();
                demuxer.Demux(&pvideo, &n_video_bytes, &pts);
                // Treat 0 bitstream size as end of stream indicator
                if (n_video_bytes == 0) {
                    pkg_flags |= ROCDEC_PKT_ENDOFSTREAM;
                }
                n_frame_returned = viddec.DecodeFrame(pvideo, n_video_bytes, pkg_flags, pts);
                auto end_time = std::chrono::high_resolution_clock::now();
                auto time_per_frame = std::chrono::duration<double, std::milli>(end_time - start_time).count();
                total_dec_time += time_per_frame;
                if (!n_frame && !viddec.GetOutputSurfaceInfo(&surf_info)) {
                    std::cerr << "Error: Failed to get Output Surface Info!" << std::endl;
                    break;
                }
                for (int i = 0; i < n_frame_returned; i++) {
                    pframe = viddec.GetFrame(&pts);
                    if (file_data.dump_output_frames) {
                        viddec.SaveFrameToFile(file_data.out_file, pframe, surf_info);
                    }
                    // release frame
                    viddec.ReleaseFrame(pts);
                }
                n_frame += n_frame_returned;
            } while (n_video_bytes);

            std::cout << "info: Total frame decoded: " << n_frame << std::endl;
            if (!file_data.dump_output_frames) {
                std::cout << "info: avg decoding time per frame (ms): " << total_dec_time / n_frame << std::endl;
                std::cout << "info: avg FPS: " << (n_frame / total_dec_time) * 1000 << std::endl;
            }
        }
    } catch (const std::exception &ex) {
        std::cout << ex.what() << std::endl;
        exit(1);
    }

    return 0;
}
