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
#include "video_demuxer.hpp"
#include "roc_video_dec.h"

void DecProc(RocVideoDecoder *p_dec, VideoDemuxer *demuxer, int *pn_frame, double *pn_fps) {
    int n_video_bytes = 0, n_frame_returned = 0, n_frame = 0;
    uint8_t *p_video = nullptr;
    int64_t pts = 0;
    double total_dec_time = 0.0;
    auto start_time = std::chrono::high_resolution_clock::now();

    do {
        demuxer->Demux(&p_video, &n_video_bytes, &pts);
        n_frame_returned = p_dec->DecodeFrame(p_video, n_video_bytes, 0, pts);
        n_frame += n_frame_returned;
    } while (n_video_bytes);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto time_per_frame = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    // Calculate average decoding time
    total_dec_time = time_per_frame;
    double average_decoding_time = total_dec_time / n_frame;
    double n_fps = 1000 / average_decoding_time;
    *pn_fps = n_fps;
}

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-t Number of threads (>= 1) - optional; default: 4" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string input_file_path;
    int device_id = 0;
    int n_thread = 4;
    Rect *p_crop_rect = nullptr;
    OUTPUT_SURF_MEMORY_TYPE mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;        // set to internal
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
        if (!strcmp(argv[i], "-t")) {
            if (++i == argc) {
                ShowHelpAndExit("-t");
            }
            n_thread = atoi(argv[i]);
            if (n_thread <= 0) {
                ShowHelpAndExit(argv[i]);
            }
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }
    std::vector<std::unique_ptr<VideoDemuxer>> v_demuxer;
    std::vector<std::unique_ptr<RocVideoDecoder>> v_viddec;
    std::vector<int> v_device_id(n_thread);

    // TODO: Change this block to use VCN query API 
    int num_devices = 0;
    hipError_t hip_status = hipSuccess;
    hip_status = hipGetDeviceCount(&num_devices);
    if (hip_status != hipSuccess) {
        std::cout << "ERROR: hipGetDeviceCount failed! (" << hip_status << ")" << std::endl;
        return 1;
    }

    int sd = (num_devices >= 2) ? 1 : 0;

    for (int i = 0; i < n_thread; i++) {
        std::unique_ptr<VideoDemuxer> demuxer(new VideoDemuxer(input_file_path.c_str()));
        rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer->GetCodecID());
        v_device_id[i] = (i % 2 == 0) ? 0 : sd;
        std::unique_ptr<RocVideoDecoder> dec(new RocVideoDecoder(v_device_id[i], mem_type, rocdec_codec_id, false, true, p_crop_rect));
        v_demuxer.push_back(std::move(demuxer));
        v_viddec.push_back(std::move(dec));
    }

    float total_fps = 0;
    std::vector<std::thread> v_thread;
    std::vector<double> v_fps;
    std::vector<int> v_frame;
    v_fps.resize(n_thread, 0);
    v_frame.resize(n_thread, 0);
    int n_total = 0;
    OutputSurfaceInfo *p_surf_info;

    std::string device_name, gcn_arch_name;
    int pci_bus_id, pci_domain_id, pci_device_id;

    for (int i = 0; i < n_thread; i++) {
        v_viddec[i]->GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
        std::cout << "info: stream " << i << " using GPU device " << v_device_id[i] << " - " << device_name << "[" << gcn_arch_name << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pci_bus_id << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pci_domain_id << "." << pci_device_id << std::dec << std::endl;
        std::cout << "info: decoding started for thread " << i << " ,please wait!" << std::endl;
    }
    
    for (int i = 0; i < n_thread; i++) {
        v_thread.push_back(std::thread(DecProc, v_viddec[i].get(), v_demuxer[i].get(), &v_frame[i], &v_fps[i]));
    }

    for (int i = 0; i < n_thread; i++) {
        v_thread[i].join();
        total_fps += v_fps[i];
        n_total += v_frame[i];
    }

    if (!v_viddec[0]->GetOutputSurfaceInfo(&p_surf_info)) {
        std::cerr << "Error: Failed to get Output Surface Info!" << std::endl;
       return -1;
    }

    std::cout << "info: Video codec format: " << v_viddec[0]->GetCodecFmtName(v_viddec[0]->GetCodecId()) << std::endl;
    std::cout << "info: Video size: [ " << p_surf_info->output_width << ", " << p_surf_info->output_height << " ]" << std::endl;
    std::cout << "info: Video surface format: " << v_viddec[0]->GetSurfaceFmtName(p_surf_info->surface_format) << std::endl;
    std::cout << "info: Video Bit depth: " << p_surf_info->bit_depth << std::endl;
    std::cout << "info: Total frame decoded: " << n_total  << std::endl;
    std::cout << "info: avg decoding time per frame (ms): " << 1000 / total_fps << std::endl;
    std::cout << "info: avg FPS: " << total_fps  << std::endl;
    
    return 0;
}
