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

#include <iostream>
#include <iomanip>
#include <fstream>
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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "video_demuxer.h"
#include "roc_video_dec.h"
#include "video_post_process.h"

std::vector<std::string> st_output_format_name = {"native", "bgr", "bgr48", "rgb", "rgb48", "bgra", "bgra64", "rgba", "rgba64"};

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-of Output Format name - (native, bgr, bgr48, rgb, rgb48, bgra, bgra64, rgba, rgba64; converts native YUV frame to RGB image format; optional; default: 0" << std::endl
    << "-resize WxH - (where W is resize width and H is resize height) optional; default: no resize " << std::endl
    << "-crop crop rectangle for output (not used when using interopped decoded frame); optional; default: 0" << std::endl;

    exit(0);
}

constexpr int frame_buffers_size = 2;
std::queue<uint8_t*> frame_queue[frame_buffers_size];
std::mutex mutex[frame_buffers_size];
std::condition_variable cv[frame_buffers_size];

void ColorSpaceConversionThread(std::atomic<bool>& continue_processing, bool convert_to_rgb, Dim *p_resize_dim, OutputSurfaceInfo **surf_info, OutputSurfaceInfo **res_surf_info,
        OutputFormatEnum e_output_format, uint8_t *p_rgb_dev_mem, uint8_t *p_resize_dev_mem, bool dump_output_frames,
        std::string &output_file_path, RocVideoDecoder &viddec, VideoPostProcess &post_proc, bool b_generate_md5) {

    size_t rgb_image_size, resize_image_size;
    hipError_t hip_status = hipSuccess;
    int current_frame_index = 0;
    uint8_t *frame;

    while (continue_processing || !frame_queue[current_frame_index].empty()) {
        OutputSurfaceInfo *p_surf_info;
        uint8_t *out_frame;
        {
            std::unique_lock<std::mutex> lock(mutex[current_frame_index]);
            cv[current_frame_index].wait(lock, [&] {return !frame_queue[current_frame_index].empty() || !continue_processing;});
            if (!continue_processing && frame_queue[current_frame_index].empty()) {
                break;
            }
            p_surf_info = *surf_info;
            // Get the current frame at the curren_buffer index for processing
            frame = frame_queue[current_frame_index].front();
            frame_queue[current_frame_index].pop();
            out_frame = frame;
        }
        if (p_resize_dim->w && p_resize_dim->h && *res_surf_info) {
            // check if the resize dims are different from output dims
            // resize is needed since output dims are different from resize dims
            // TODO:: the below code assumes NV12/P016 for decoded output surface. Modify to take other surface formats in future
            if (((*surf_info)->output_width != p_resize_dim->w) || ((*surf_info)->output_height != p_resize_dim->h)) {
                resize_image_size = p_resize_dim->w * (p_resize_dim->h + (p_resize_dim->h >> 1)) * (*surf_info)->bytes_per_pixel;
                if (p_resize_dev_mem == nullptr && resize_image_size > 0) {
                    hip_status = hipMalloc(&p_resize_dev_mem, resize_image_size);
                    if (hip_status != hipSuccess) {
                        std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hip_status << std::endl;
                        return;
                    }
                 }
                 // call resize kernel
                 if ((*surf_info)->bytes_per_pixel == 2) {
                    ResizeP016(p_resize_dev_mem, p_resize_dim->w * 2, p_resize_dim->w, p_resize_dim->h, frame, (*surf_info)->output_pitch, (*surf_info)->output_width,
                        (*surf_info)->output_height, (frame + (*surf_info)->output_vstride * (*surf_info)->output_pitch), nullptr, viddec.GetStream());
                 } else {                        
                    ResizeNv12(p_resize_dev_mem, p_resize_dim->w, p_resize_dim->w, p_resize_dim->h, frame, (*surf_info)->output_pitch, (*surf_info)->output_width,
                         (*surf_info)->output_height, (frame + (*surf_info)->output_vstride * (*surf_info)->output_pitch), nullptr, viddec.GetStream());
                 }
                (*res_surf_info)->output_width = p_resize_dim->w;
                (*res_surf_info)->output_height = p_resize_dim->h;
                (*res_surf_info)->output_pitch = p_resize_dim->w * (*surf_info)->bytes_per_pixel;
                (*res_surf_info)->output_vstride = p_resize_dim->h;
                (*res_surf_info)->output_surface_size_in_bytes = (*res_surf_info)->output_pitch * (p_resize_dim->h + (p_resize_dim->h >> 1));
                (*res_surf_info)->mem_type = OUT_SURFACE_MEM_DEV_COPIED;
                p_surf_info = *res_surf_info;
                out_frame = p_resize_dev_mem;
            }
        }

        if (convert_to_rgb) {
            uint32_t rgb_stride = post_proc.GetRgbStride(e_output_format, p_surf_info);
            rgb_image_size = p_surf_info->output_height * rgb_stride;
            if (p_rgb_dev_mem == nullptr) {
                hip_status = hipMalloc(&p_rgb_dev_mem, rgb_image_size);
                if (hip_status != hipSuccess) {
                    std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hip_status << std::endl;
                    return;
                }
            }
            post_proc.ColorConvertYUV2RGB(out_frame, p_surf_info, p_rgb_dev_mem, e_output_format, viddec.GetStream());
        }
        if (dump_output_frames) {
            if (convert_to_rgb)
                viddec.SaveFrameToFile(output_file_path, p_rgb_dev_mem, p_surf_info, rgb_image_size);
            else
                viddec.SaveFrameToFile(output_file_path, out_frame, p_surf_info);
        }
        if(b_generate_md5 && convert_to_rgb){
            viddec.UpdateMd5ForDataBuffer(p_rgb_dev_mem, rgb_image_size);
        }
        

        cv[current_frame_index].notify_one();
        current_frame_index = (current_frame_index + 1) % frame_buffers_size;
    }
}

int main(int argc, char **argv) {

    std::string input_file_path, output_file_path, md5_file_path;
    std::fstream ref_md5_file;
    bool b_generate_md5 = false;
    bool b_md5_check = false;
    bool dump_output_frames = false;
    bool convert_to_rgb = false;
    int device_id = 0;
    Rect crop_rect = {};
    Dim resize_dim = {};
    Rect *p_crop_rect = nullptr;
    size_t rgb_image_size;
    uint32_t rgb_image_stride;
    hipError_t hip_status = hipSuccess;
    uint8_t *p_rgb_dev_mem = nullptr;
    uint8_t *p_resize_dev_mem = nullptr;
    OutputSurfaceMemoryType mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;
    OutputFormatEnum e_output_format = native; 
    int rgb_width;
    uint8_t* frame_buffers[frame_buffers_size] = {0};
    int current_frame_index = 0;
    

    // Parse command-line arguments
    if(argc <= 1) {
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
            dump_output_frames = true;
            continue;
        }
        if (!strcmp(argv[i], "-d")) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            device_id = atoi(argv[i]);
            continue;
        }
        if (!strcmp(argv[i], "-crop")) {
            if (++i == argc || 4 != sscanf(argv[i], "%d,%d,%d,%d", &crop_rect.left, &crop_rect.top, &crop_rect.right, &crop_rect.bottom)) {
                ShowHelpAndExit("-crop");
            }
            if ((crop_rect.right - crop_rect.left) % 2 == 1 || (crop_rect.bottom - crop_rect.top) % 2 == 1) {
                std::cout << "output crop rectangle must have width and height of even numbers" << std::endl;
                exit(1);
            }
            p_crop_rect = &crop_rect;
            continue;
        }
        if (!strcmp(argv[i], "-resize")) {
            if (++i == argc || 2 != sscanf(argv[i], "%dx%d", &resize_dim.w, &resize_dim.h)) {
                ShowHelpAndExit("-resize");
            }
            if (resize_dim.w % 2 == 1 || resize_dim.h % 2 == 1) {
                std::cout << "Resizing dimensions must have width and height of even numbers" << std::endl;
                exit(1);
            }
            continue;
        }
        if (!strcmp(argv[i], "-of")) {
            if (++i == argc) {
                ShowHelpAndExit("-of");
            }
            auto it = std::find(st_output_format_name.begin(), st_output_format_name.end(), argv[i]);
            if (it == st_output_format_name.end()) {
                ShowHelpAndExit("-of");
            }
            e_output_format = (OutputFormatEnum)(it - st_output_format_name.begin());
            continue;
        }
        if (!strcmp(argv[i], "-md5")) {
            if (i == argc) {
                ShowHelpAndExit("-md5");
            }
            b_generate_md5 = true;
            continue;
        }
        if (!strcmp(argv[i], "-md5_check")) {
            if (++i == argc) {
                ShowHelpAndExit("-md5_check");
            }
            b_generate_md5 = true;
            b_md5_check = true;
            md5_file_path = argv[i];
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    try {
        VideoDemuxer demuxer(input_file_path.c_str());
        rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer.GetCodecID());
        RocVideoDecoder viddec(device_id, mem_type, rocdec_codec_id, false, p_crop_rect);
        VideoPostProcess post_process;

        std::string device_name, gcn_arch_name;
        int pci_bus_id, pci_domain_id, pci_device_id;
        hipStream_t stream = viddec.GetStream();

        viddec.GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
        std::cout << "info: Using GPU device " << device_id << " " << device_name << "[" << gcn_arch_name << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pci_bus_id << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pci_domain_id << "." << pci_device_id << std::dec << std::endl;
        std::cout << "info: decoding started, please wait!" << std::endl;

        if (b_generate_md5) {
            viddec.InitMd5();
        }
        if (b_md5_check) {
            ref_md5_file.open(md5_file_path.c_str(), std::ios::in);            
        }

        int n_video_bytes = 0, n_frames_returned = 0, n_frame = 0;
        uint8_t *p_video = nullptr;
        uint8_t *p_frame = nullptr;
        int64_t pts = 0;
        OutputSurfaceInfo *surf_info;
        OutputSurfaceInfo *resize_surf_info = nullptr;
        uint32_t width, height;
        double total_dec_time = 0;
        convert_to_rgb = e_output_format != native;
        std::atomic<bool> continue_processing(true);
        std::thread color_space_conversion_thread(ColorSpaceConversionThread, std::ref(continue_processing), std::ref(convert_to_rgb), &resize_dim, &surf_info, &resize_surf_info, std::ref(e_output_format),
                                    std::ref(p_rgb_dev_mem), std::ref(p_resize_dev_mem), std::ref(dump_output_frames), std::ref(output_file_path), std::ref(viddec), std::ref(post_process), b_generate_md5);

        auto startTime = std::chrono::high_resolution_clock::now();
        do {
            demuxer.Demux(&p_video, &n_video_bytes, &pts);
            n_frames_returned = viddec.DecodeFrame(p_video, n_video_bytes, 0, pts);
            if (!n_frame && !viddec.GetOutputSurfaceInfo(&surf_info)) {
                std::cerr << "Error: Failed to get Output Image Info!" << std::endl;
                break;
            }
            if (resize_dim.w && resize_dim.h && !resize_surf_info) {
                resize_surf_info = new OutputSurfaceInfo;
                memcpy(resize_surf_info, surf_info, sizeof(OutputSurfaceInfo));
            }

            int last_index = 0;
            for (int i = 0; i < n_frames_returned; i++) {
                p_frame = viddec.GetFrame(&pts);
                // allocate extra device memories to use double-buffering for keeping two decoded frames
                if (frame_buffers[0] == nullptr) {
                    for (int i = 0; i < frame_buffers_size; i++) {
                        HIP_API_CALL(hipMalloc(&frame_buffers[i], surf_info->output_surface_size_in_bytes));
                    }
                }

                {
                    std::unique_lock<std::mutex> lock(mutex[current_frame_index]);
                    cv[current_frame_index].wait(lock, [&] {return frame_queue[current_frame_index].empty();});
                    // copy the decoded frame into the frame_buffers at current_frame_index
                    HIP_API_CALL(hipMemcpyDtoDAsync(frame_buffers[current_frame_index], p_frame, surf_info->output_surface_size_in_bytes, viddec.GetStream()));
                    frame_queue[current_frame_index].push(frame_buffers[current_frame_index]);
                }

                viddec.ReleaseFrame(pts);
                cv[current_frame_index].notify_one(); // Notify the ColorSpaceConversionThread that a frame is available for post-processing
                current_frame_index = (current_frame_index + 1) % frame_buffers_size; // update the current_frame_index to the next index in the frame_buffers
            }

            n_frame += n_frames_returned;
        } while (n_video_bytes);

        {
            std::unique_lock<std::mutex> lock(mutex[current_frame_index]);
            //Signal ColorSpaceConversionThread to stop
            continue_processing = false;
            lock.unlock();
            cv[current_frame_index].notify_one();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto time_per_frame = std::chrono::duration<double, std::milli>(end_time - startTime).count();
        total_dec_time += time_per_frame;

        color_space_conversion_thread.join();

        if (p_rgb_dev_mem != nullptr) {
            hip_status = hipFree(p_rgb_dev_mem);
            if (hip_status != hipSuccess) {
                std::cout << "ERROR: hipFree failed! (" << hip_status << ")" << std::endl;
                return -1;
            }
        }
        for (int i = 0; i < frame_buffers_size; i++) {
            hip_status = hipFree(frame_buffers[i]);
            if (hip_status != hipSuccess) {
                std::cout << "ERROR: hipFree failed! (" << hip_status << ")" << std::endl;
            }
        }

        std::cout << "info: Total frame decoded: " << n_frame << std::endl;
        if (!dump_output_frames) {
            std::string info_message = "info: avg decoding time per frame (ms): ";
            if (convert_to_rgb) {
                info_message = "info: avg decoding and post processing time per frame (ms): ";
            }
            std::cout << info_message << total_dec_time / n_frame << std::endl;
            std::cout << "info: avg FPS: " << (n_frame / total_dec_time) * 1000 << std::endl;
        }
        if (resize_surf_info != nullptr) {
            delete resize_surf_info;
        }
        if (b_generate_md5) {
            uint8_t *digest;
            viddec.FinalizeMd5(&digest);
            std::cout << "MD5 message digest: ";
            for (int i = 0; i < 16; i++) {
                std::cout << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(digest[i]);
            }
            std::cout << std::endl;

            if (b_md5_check) {
                char ref_md5_string[33], c2[2];
                uint8_t ref_md5[16];
                std::string str;

                for (int i = 0; i < 16; i++) {
                    int c;
                    ref_md5_file.get(c2[0]);
                    ref_md5_file.get(c2[1]);
                    str = c2;
                    c = std::stoi(str, nullptr, 16);
                    ref_md5[i] = c;
                }
                if (memcmp(digest, ref_md5, 16) == 0) {
                    std::cout << "MD5 digest matches the reference MD5 digest: " << std::endl;
                } else {
                    std::cout << "MD5 digest does not match the reference MD5 digest: " << std::endl;
                }
                ref_md5_file.seekg(0, std::ios_base::beg);
                ref_md5_file.getline(ref_md5_string, 33);
                std::cout << ref_md5_string << std::endl;
                ref_md5_file.close();
            }
        }
    } catch (const std::exception &ex) {
        std::cout << ex.what() << std::endl;
        exit(1);
    }

    return 0;
}