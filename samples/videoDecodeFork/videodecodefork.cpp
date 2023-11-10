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
#include <sys/mman.h>
#include <sys/wait.h>
#include <libgen.h>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
#else
    #include <experimental/filesystem>
#endif
#include "video_demuxer.hpp"
#include "roc_video_dec.h"

static int *n_total;

void DecProc(RocVideoDecoder *p_dec, VideoDemuxer *demuxer, int *pn_frame) {
    int n_video_bytes = 0, n_frame_returned = 0, n_frame = 0;
    uint8_t *p_video = nullptr;
    int64_t pts = 0;

    do {
        demuxer->Demux(&p_video, &n_video_bytes, &pts);
        n_frame_returned = p_dec->DecodeFrame(p_video, n_video_bytes, 0, pts);
        n_frame += n_frame_returned;
    } while (n_video_bytes);
    *pn_frame = n_frame;
}

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-t Number of threads (>= 1) - optional; default: 4" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string input_file_path;
    int n_fork = 4;
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
            n_fork = atoi(argv[i]);
            if (n_fork <= 0) {
                ShowHelpAndExit(argv[i]);
            }
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    try {
        std::vector<std::unique_ptr<VideoDemuxer>> v_demuxer;
        std::vector<std::unique_ptr<RocVideoDecoder>> v_viddec;
        std::vector<int> v_device_id(n_fork);

        // TODO: Change this block to use VCN query API 
        int num_devices = 0;
        hipError_t hip_status = hipSuccess;
        hip_status = hipGetDeviceCount(&num_devices);
        if (hip_status != hipSuccess) {
            std::cout << "ERROR: hipGetDeviceCount failed! (" << hip_status << ")" << std::endl;
            return 1;
        }

        int sd = (num_devices >= 2) ? 1 : 0;

        for (int i = 0; i < n_fork; i++) {
            std::unique_ptr<VideoDemuxer> demuxer(new VideoDemuxer(input_file_path.c_str()));
            rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer->GetCodecID());
            v_device_id[i] = (i % 2 == 0) ? 0 : sd;
            std::unique_ptr<RocVideoDecoder> dec(new RocVideoDecoder(v_device_id[i], mem_type, rocdec_codec_id, false, true, p_crop_rect));
            v_demuxer.push_back(std::move(demuxer));
            v_viddec.push_back(std::move(dec));
        }

        std::vector<int> v_frame;
        v_frame.resize(n_fork, 0);

        std::string device_name, gcn_arch_name;
        int pci_bus_id, pci_domain_id, pci_device_id;

        for (int i = 0; i < n_fork; i++) {
            v_viddec[i]->GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
            std::cout << "info: stream " << i << " using GPU device " << v_device_id[i] << " - " << device_name << "[" << gcn_arch_name << "] on PCI bus " <<
            std::setfill('0') << std::setw(2) << std::right << std::hex << pci_bus_id << ":" << std::setfill('0') << std::setw(2) <<
            std::right << std::hex << pci_domain_id << "." << pci_device_id << std::dec << std::endl;
            std::cout << "info: decoding started for thread " << i << " ,please wait!" << std::endl;
        }

        int pid_status;
        pid_t pids[n_fork];
        n_total = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        *n_total = 0;

        auto start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < n_fork; i++) {
            if ((pids[i] = fork()) < 0) {
                std::cout << "ERROR: failed to create fork" << std::endl;
                abort();
            }
            else if (pids[i] == 0) {
                DecProc(v_viddec[i].get(), v_demuxer[i].get(), &v_frame[i]);
                *n_total += v_frame[i];
                _exit(0);
            }
        }

        for(int i = 0; i < n_fork; i++) {	
            waitpid(pids[i], &pid_status, 0);
            if (!WIFEXITED(pid_status)) {
                std::cout << "child with " << pids[i] << " exited with status " << WEXITSTATUS(pid_status) << std::endl;
            }
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Calculate average decoding time
        double total_decoding_time = 0.0;
        total_decoding_time = duration;
        double average_decoding_time = total_decoding_time / *n_total;

        std::cout << "info: Total Frames Decoded: " << *n_total << std::endl;
        std::cout << "info: avg decoding time per frame (ms): " << average_decoding_time  << std::endl;
        std::cout << "info: avg FPS: " << 1000 / average_decoding_time << std::endl;
        munmap(n_total, sizeof(int));
    } catch (const std::exception &ex) {
      std::cout << ex.what() << std::endl;
      exit(1);
    }

    return 0;
}
