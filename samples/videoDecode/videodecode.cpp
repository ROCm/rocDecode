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
#include <fstream>
#include <cstring>
#include <string>
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
#include "common.h"

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-f Number of decoded frames - specify the number of pictures to be decoded; optional" << std::endl
    << "-z force_zero_latency (force_zero_latency, Decoded frames will be flushed out for display immediately); optional;" << std::endl
    << "-disp_delay -specify the number of frames to be delayed for display; optional;" << std::endl
    << "-sei extract SEI messages; optional;" << std::endl
    << "-md5 generate MD5 message digest on the decoded YUV image sequence; optional;" << std::endl
    << "-md5_check MD5 File Path - generate MD5 message digest on the decoded YUV image sequence and compare to the reference MD5 string in a file; optional;" << std::endl
    << "-crop crop rectangle for output (not used when using interopped decoded frame); optional; default: 0" << std::endl
    << "-m output_surface_memory_type - decoded surface memory; optional; default - 0"
    << " [0 : OUT_SURFACE_MEM_DEV_INTERNAL/ 1 : OUT_SURFACE_MEM_DEV_COPIED/ 2 : OUT_SURFACE_MEM_HOST_COPIED/ 3 : OUT_SURFACE_MEM_NOT_MAPPED]" << std::endl
    << "-seek_criteria - Demux seek criteria & value - optional; default - 0,0; "
    << "[0: no seek; 1: SEEK_CRITERIA_FRAME_NUM, frame number; 2: SEEK_CRITERIA_TIME_STAMP, frame number (time calculated internally)]" << std::endl
    << "-seek_mode - Seek to previous key frame or exact - optional; default - 0"
    << "[0: SEEK_MODE_PREV_KEY_FRAME; 1: SEEK_MODE_EXACT_FRAME]" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string input_file_path, output_file_path, md5_file_path;
    std::fstream ref_md5_file;
    int dump_output_frames = 0;
    int device_id = 0;
    int disp_delay = 0;
    bool b_force_zero_latency = false;     // false by default: enabling this option might affect decoding performance
    bool b_extract_sei_messages = false;
    bool b_generate_md5 = false;
    bool b_md5_check = false;
    bool b_flush_frames_during_reconfig = true;
    Rect crop_rect = {};
    Rect *p_crop_rect = nullptr;
    OutputSurfaceMemoryType mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;      // set to internal
    ReconfigParams reconfig_params = { 0 };
    ReconfigDumpFileStruct reconfig_user_struct = { 0 };
    uint32_t num_decoded_frames = 0;  // default value is 0, meaning decode the entire stream
    // seek options
    uint64_t seek_to_frame = 0;
    int seek_criteria = 0, seek_mode = 0;

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
        if (!strcmp(argv[i], "-disp_delay")) {
            if (++i == argc) {
                ShowHelpAndExit("-disp_delay");
            }
            disp_delay = atoi(argv[i]);
            continue;
        }
        if (!strcmp(argv[i], "-f")) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            num_decoded_frames = atoi(argv[i]);
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
        if (!strcmp(argv[i], "-m")) {
            if (++i == argc) {
                ShowHelpAndExit("-m");
            }
            mem_type = static_cast<OutputSurfaceMemoryType>(atoi(argv[i]));
            continue;
        }
        if (!strcmp(argv[i], "flush")) {
            b_flush_frames_during_reconfig = atoi(argv[i]) ? true : false;
            continue;
        }
        if (!strcmp(argv[i], "-seek_criteria")) {
            if (++i == argc || 2 != sscanf(argv[i], "%d,%lu", &seek_criteria, &seek_to_frame)) {
                ShowHelpAndExit("-seek_criteria");
            }
            if (0 > seek_criteria || seek_criteria >= 3)
                ShowHelpAndExit("-seek_criteria");
            continue;
        }
        if (!strcmp(argv[i], "-seek_mode")) {
            if (++i == argc) {
                ShowHelpAndExit("-seek_mode");
            }
            seek_mode = atoi(argv[i]);
            if (seek_mode != 0 && seek_mode != 1)
                ShowHelpAndExit("-seek_mode");
            continue;
        }

        ShowHelpAndExit(argv[i]);
    }

    try {
        std::size_t found_file = input_file_path.find_last_of('/');
        std::cout << "info: Input file: " << input_file_path.substr(found_file + 1) << std::endl;
        VideoDemuxer demuxer(input_file_path.c_str());
        VideoSeekContext video_seek_ctx;
        rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer.GetCodecID());
        RocVideoDecoder viddec(device_id, mem_type, rocdec_codec_id, b_force_zero_latency, p_crop_rect, b_extract_sei_messages, disp_delay);
        if(!viddec.CodecSupported(device_id, rocdec_codec_id, demuxer.GetBitDepth())) {
            std::cerr << "GPU doesn't support codec!" << std::endl;
            return 0;
        }        
        std::string device_name, gcn_arch_name;
        int pci_bus_id, pci_domain_id, pci_device_id;

        viddec.GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
        std::cout << "info: Using GPU device " << device_id << " - " << device_name << "[" << gcn_arch_name << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pci_bus_id << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pci_domain_id << "." << pci_device_id << std::dec << std::endl;
        std::cout << "info: decoding started, please wait!" << std::endl;

        int n_video_bytes = 0, n_frame_returned = 0, n_frame = 0;
        int n_pic_decoded = 0, decoded_pics = 0;
        uint8_t *pvideo = nullptr;
        int pkg_flags = 0;
        uint8_t *pframe = nullptr;
        int64_t pts = 0;
        OutputSurfaceInfo *surf_info;
        uint32_t width, height;
        double total_dec_time = 0;
        bool first_frame = true;
        // initialize reconfigure params: the following is configured to dump to output which is relevant for this sample
        reconfig_params.p_fn_reconfigure_flush = ReconfigureFlushCallback;
        reconfig_user_struct.b_dump_frames_to_file = dump_output_frames;
        reconfig_user_struct.output_file_name = output_file_path;
        if (dump_output_frames) {
            reconfig_params.reconfig_flush_mode = RECONFIG_FLUSH_MODE_DUMP_TO_FILE;
        } else if (b_generate_md5) {
            reconfig_params.reconfig_flush_mode = RECONFIG_FLUSH_MODE_CALCULATE_MD5;
        } else {
            reconfig_params.reconfig_flush_mode = RECONFIG_FLUSH_MODE_NONE;
        }
        reconfig_params.p_reconfig_user_struct = &reconfig_user_struct;

        if (b_generate_md5) {
            viddec.InitMd5();
        }
        viddec.SetReconfigParams(&reconfig_params);

        do {
            auto start_time = std::chrono::high_resolution_clock::now();
            if (seek_criteria == 1 && first_frame) {
                // use VideoSeekContext class to seek to given frame number
                video_seek_ctx.seek_frame_ = seek_to_frame;
                video_seek_ctx.seek_crit_ = SEEK_CRITERIA_FRAME_NUM;
                video_seek_ctx.seek_mode_ = (seek_mode ? SEEK_MODE_EXACT_FRAME : SEEK_MODE_PREV_KEY_FRAME);
                demuxer.Seek(video_seek_ctx, &pvideo, &n_video_bytes);
                pts = video_seek_ctx.out_frame_pts_;
                std::cout << "info: Number of frames that were decoded during seek - " << video_seek_ctx.num_frames_decoded_ << std::endl;
                first_frame = false;
            } else if (seek_criteria == 2 && first_frame) {
                // use VideoSeekContext class to seek to given timestamp
                video_seek_ctx.seek_frame_ = seek_to_frame;
                video_seek_ctx.seek_crit_ = SEEK_CRITERIA_TIME_STAMP;
                video_seek_ctx.seek_mode_ = (seek_mode ? SEEK_MODE_EXACT_FRAME : SEEK_MODE_PREV_KEY_FRAME);
                demuxer.Seek(video_seek_ctx, &pvideo, &n_video_bytes);
                pts = video_seek_ctx.out_frame_pts_;
                std::cout << "info: Duration of frame found after seek - " << video_seek_ctx.out_frame_duration_ << " ms" << std::endl;
                first_frame = false;
            } else {
                demuxer.Demux(&pvideo, &n_video_bytes, &pts);
            }
            // Treat 0 bitstream size as end of stream indicator
            if (n_video_bytes == 0) {
                pkg_flags |= ROCDEC_PKT_ENDOFSTREAM;
            }
            n_frame_returned = viddec.DecodeFrame(pvideo, n_video_bytes, pkg_flags, pts, &decoded_pics);

            if (!n_frame && !viddec.GetOutputSurfaceInfo(&surf_info)) {
                std::cerr << "Error: Failed to get Output Surface Info!" << std::endl;
                break;
            }
            for (int i = 0; i < n_frame_returned; i++) {
                pframe = viddec.GetFrame(&pts);
                if (b_generate_md5) {
                    viddec.UpdateMd5ForFrame(pframe, surf_info);
                }
                if (dump_output_frames && mem_type != OUT_SURFACE_MEM_NOT_MAPPED) {
                    viddec.SaveFrameToFile(output_file_path, pframe, surf_info);
                }
                // release frame
                viddec.ReleaseFrame(pts);
            }
            auto end_time = std::chrono::high_resolution_clock::now();
            auto time_per_decode = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            total_dec_time += time_per_decode;
            n_frame += n_frame_returned;
            n_pic_decoded += decoded_pics;
            if (num_decoded_frames && num_decoded_frames <= n_frame) {
                break;
            }

        } while (n_video_bytes);
        
        n_frame += viddec.GetNumOfFlushedFrames();
        std::cout << "info: Total pictures decoded: " << n_pic_decoded << std::endl;
        std::cout << "info: Total frames output/displayed: " << n_frame << std::endl;
        if (!dump_output_frames) {
            std::cout << "info: avg decoding time per picture: " << total_dec_time / n_pic_decoded << " ms" <<std::endl;
            std::cout << "info: avg decode FPS: " << (n_pic_decoded / total_dec_time) * 1000 << std::endl;
            std::cout << "info: avg output/display time per frame: " << total_dec_time / n_frame << " ms" <<std::endl;
            std::cout << "info: avg output/display FPS: " << (n_frame / total_dec_time) * 1000 << std::endl;
        } else {
            if (mem_type == OUT_SURFACE_MEM_NOT_MAPPED) {
                std::cout << "info: saving frames with -m 3 option is not supported!" << std::endl;
            } else {
                std::cout << "info: saved frames into " << output_file_path << std::endl;
            }
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
                std::string ref_md5_string(33, 0);
                uint8_t ref_md5[16];
                ref_md5_file.open(md5_file_path.c_str(), std::ios::in);
                if ((ref_md5_file.rdstate() & std::ifstream::failbit) != 0) {
                    std::cerr << "Failed to open MD5 file." << std::endl;
                    return 1;
                }
                ref_md5_file.getline(ref_md5_string.data(), ref_md5_string.length());
                if ((ref_md5_file.rdstate() & std::ifstream::failbit) != 0) {
                    std::cerr << "Failed to read MD5 digest string." << std::endl;
                    return 1;
                }
                for (int i = 0; i < 16; i++) {
                    std::string part = ref_md5_string.substr(i * 2, 2);
                    ref_md5[i] = std::stoi(part, nullptr, 16);
                }
                if (memcmp(digest, ref_md5, 16) == 0) {
                    std::cout << "MD5 digest matches the reference MD5 digest: ";
                } else {
                    std::cout << "MD5 digest does not match the reference MD5 digest: ";
                }
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
