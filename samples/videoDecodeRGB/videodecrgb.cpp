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
#include "colorspace_kernels.h"

FILE *fpOut = nullptr;
enum OutputFormatEnum {
    native = 0, bgr, bgr48, rgb, rgb48, bgra, bgra64, rgba, rgba64
};
std::vector<std::string> st_output_format_name = {"native", "bgr", "bgr48", "rgb", "rgb48", "bgra", "bgra64", "rgba", "rgba64"};

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-of Output Format name - (native, bgr, bgr48, rgb, rgb48, bgra, bgra64, rgba, rgba64; converts native YUV frame to RGB image format; optional; default: 0" << std::endl
    << "-crop crop rectangle for output (not used when using interopped decoded frame); optional; default: 0" << std::endl;

    exit(0);
}

void DumpRGBImage(std::string outputfileName, void* pdevMem, OutputSurfaceInfo *surf_info, int rgb_image_size) {
    if (fpOut == nullptr) {
        fpOut = fopen(outputfileName.c_str(), "wb");
    }
    uint8_t *hstPtr = nullptr;
    hstPtr = new uint8_t [rgb_image_size];
    hipError_t hip_status = hipSuccess;
    hip_status = hipMemcpyDtoH((void *)hstPtr, pdevMem, rgb_image_size);
    if (hip_status != hipSuccess) {
        std::cout << "ERROR: hipMemcpyDtoH failed! (" << hip_status << ")" << std::endl;
        delete [] hstPtr;
        return;
    }
    if (fpOut) {
        fwrite(hstPtr, 1, rgb_image_size, fpOut);
    }

    if (hstPtr != nullptr) {
        delete [] hstPtr;
        hstPtr = nullptr;
    }
}

void ColorConvertYUV2RGB(uint8_t *p_src, OutputSurfaceInfo *surf_info, uint8_t *rgb_dev_mem_ptr, OutputFormatEnum e_output_format) {
    
    int  rgb_width = (surf_info->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
    // todo:: get color standard from the decoder
    if (surf_info->surface_format == rocDecVideoSurfaceFormat_YUV444) {
        if (e_output_format == bgr)
          YUV444ToColor24<BGR24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == bgra)
          YUV444ToColor32<BGRA32>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 4 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgb)
          YUV444ToColor24<RGB24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgba)
          YUV444ToColor32<RGBA32>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 4 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
    } else if (surf_info->surface_format == rocDecVideoSurfaceFormat_NV12) {
        if (e_output_format == bgr)
          Nv12ToColor24<BGR24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == bgra)
          Nv12ToColor32<BGRA32>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 4 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgb)
          Nv12ToColor24<RGB24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgba)
          Nv12ToColor32<RGBA32>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 4 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
    }
    if (surf_info->surface_format == rocDecVideoSurfaceFormat_YUV444_16Bit) {
        if (e_output_format == bgr)
          YUV444P16ToColor24<BGR24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgb)
          YUV444P16ToColor24<RGB24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == bgr48)
          YUV444P16ToColor48<BGR48>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 6 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgb48)
          YUV444P16ToColor48<RGB48>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 6 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == bgra64)
          YUV444P16ToColor64<BGRA64>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 8 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgba64)
          YUV444P16ToColor64<RGBA64>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 8 * rgb_width, surf_info->output_width, 
                                surf_info->output_height, surf_info->output_vstride, 0);
    } else if (surf_info->surface_format == rocDecVideoSurfaceFormat_P016) {
        if (e_output_format == bgr)
          P016ToColor24<BGR24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgb)
          P016ToColor24<RGB24>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 3 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == bgr48)
          P016ToColor48<BGR48>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 6 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgb48)
          P016ToColor48<RGB48>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 6 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == bgra64)
          P016ToColor64<BGRA64>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 8 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
        else if (e_output_format == rgba64)
          P016ToColor64<RGBA64>(p_src, surf_info->output_pitch, static_cast<uint8_t *>(rgb_dev_mem_ptr), 8 * rgb_width, surf_info->output_width, 
                              surf_info->output_height, surf_info->output_vstride, 0);
    }
}

int main(int argc, char **argv) {

    std::string input_file_path, output_file_path;
    int dump_output_frames = 0;
    int convert_to_rgb = 0;
    int device_id = 0;
    Rect crop_rect = {};
    Rect *p_crop_rect = nullptr;
    size_t rgb_image_size;
    uint32_t rgb_image_stride;
    hipError_t hip_status = hipSuccess;
    uint8_t *p_rgb_dev_mem= nullptr;
    OutputSurfaceMemoryType mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;        // set to internal
    OutputFormatEnum e_output_format = native; 
    int rgb_width;

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
        if (!strcmp(argv[i], "-of")) {
            if (++i == argc) {
                ShowHelpAndExit("-of");
            }
            auto it = find(st_output_format_name.begin(), st_output_format_name.end(), argv[i]);
            if (it == st_output_format_name.end()) {
                ShowHelpAndExit("-of");
            }
            e_output_format = (OutputFormatEnum)(it-st_output_format_name.begin());
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    try {
        VideoDemuxer demuxer(input_file_path.c_str());
        rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer.GetCodecID());
        RocVideoDecoder viddec(device_id, mem_type, rocdec_codec_id, false, true, p_crop_rect);

        std::string device_name, gcn_arch_name;
        int pci_bus_id, pci_domain_id, pci_device_id;
        hipStream_t stream = viddec.GetStream();

        viddec.GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
        std::cout << "info: Using GPU device " << device_id << " " << device_name << "[" << gcn_arch_name << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pci_bus_id << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pci_domain_id << "." << pci_device_id << std::dec << std::endl;
        std::cout << "info: decoding started, please wait!" << std::endl;

        int n_video_bytes = 0, n_frames_returned = 0, n_frame = 0;
        uint8_t *p_video = nullptr;
        uint8_t *p_frame = nullptr;
        int64_t pts = 0;
        OutputSurfaceInfo *surf_info;
        uint32_t width, height;
        double total_dec_time = 0;
        convert_to_rgb = e_output_format != native;

        do {
            auto startTime = std::chrono::high_resolution_clock::now();
            demuxer.Demux(&p_video, &n_video_bytes, &pts);
            n_frames_returned = viddec.DecodeFrame(p_video, n_video_bytes, 0, pts);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto time_per_frame = std::chrono::duration<double, std::milli>(end_time - startTime).count();
            total_dec_time += time_per_frame;
            if (!n_frame && !viddec.GetOutputSurfaceInfo(&surf_info)){
                std::cerr << "Error: Failed to get Output Image Info!" << std::endl;
                break;
            }

            for (int i = 0; i < n_frames_returned; i++) {
                p_frame = viddec.GetFrame(&pts);
                if (convert_to_rgb) {
                    if (surf_info->bit_depth == 8) {
                        rgb_width = (surf_info->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                        rgb_image_size = ((e_output_format == bgr) || (e_output_format == rgb)) ? rgb_width * surf_info->output_height * 3 : rgb_width * surf_info->output_height * 4;
                    } else {    // 16bit
                        rgb_width = (surf_info->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                        rgb_image_size = ((e_output_format == bgr) || (e_output_format == rgb)) ? rgb_width * surf_info->output_height * 3 : ((e_output_format == bgr48) || (e_output_format == rgb48)) ? 
                                                              rgb_width * surf_info->output_height * 6 : rgb_width * surf_info->output_height * 8;
                    }
                    if (p_rgb_dev_mem == nullptr) {
                        hip_status = hipMalloc(&p_rgb_dev_mem, rgb_image_size);
                        if (hip_status != hipSuccess) {
                            std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hip_status << std::endl;
                            return -1;
                        }
                    }
                    ColorConvertYUV2RGB(p_frame, surf_info, p_rgb_dev_mem, e_output_format);
                }
                if (dump_output_frames) {
                    if (convert_to_rgb)
                        DumpRGBImage(output_file_path, p_rgb_dev_mem, surf_info, rgb_image_size);
                    else
                        viddec.SaveFrameToFile(output_file_path, p_frame, surf_info);
                }
                // release frame
                viddec.ReleaseFrame(pts);
            }
            n_frame += n_frames_returned;
        } while (n_video_bytes);

        if (p_rgb_dev_mem != nullptr) {
            hip_status = hipFree(p_rgb_dev_mem);
            if (hip_status != hipSuccess) {
                std::cout << "ERROR: hipFree failed! (" << hip_status << ")" << std::endl;
                return -1;
            }
        }
        if (fpOut) {
          fclose(fpOut);
          fpOut = nullptr;
        }

        std::cout << "info: Video codec format: " << viddec.GetCodecFmtName(viddec.GetCodecId()) << std::endl;
        std::cout << "info: Video size: [ " << surf_info->output_width << ", " << surf_info->output_height << " ]" << std::endl;
        std::cout << "info: Video surface format: " << viddec.GetSurfaceFmtName(surf_info->surface_format) << std::endl;
        std::cout << "info: Video Bit depth: " << surf_info->bit_depth << std::endl;
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
