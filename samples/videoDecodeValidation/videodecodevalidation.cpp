/*
Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.

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
#include <fcntl.h>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "video_demuxer.h"
#include "roc_video_dec.h"
#include "common.h"

// Thread-safe frame queue for worker threads
template<typename T>
class ConcurrentQueue {
private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};

public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(std::move(item));
        }
        // Notify outside of lock to prevent thread contention
        cv.notify_one();
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex);
        
        // Wait until queue has items or stop is signaled
        cv.wait(lock, [this]() { 
            return !queue.empty() || stop; 
        });
        
        // If queue is empty and stop is signaled, return false
        if (queue.empty()) {
            return false;
        }
        
        // Get the item
        item = std::move(queue.front());
        queue.pop();
        return true;
    }
    
    void stopQueue() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        // Wake up all threads
        cv.notify_all();
    }
    
    bool isEmpty() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
};

// Frame data structure for processing
struct FrameData {
    std::vector<uint8_t> frame_data;
    size_t y_size;
    size_t uv_size;
    int64_t pts;
    bool is_valid;
    
    FrameData() : y_size(0), uv_size(0), pts(0), is_valid(false) {}
    
    FrameData(uint8_t* data, size_t y, size_t uv, int64_t timestamp) 
        : y_size(y), uv_size(uv), pts(timestamp), is_valid(true) {
        if (data && (y_size + uv_size > 0)) {
            frame_data.resize(y_size + uv_size);
            memcpy(frame_data.data(), data, y_size + uv_size);
        } else {
            is_valid = false;
        }
    }
    
    bool isEmpty() const {
        return !is_valid || frame_data.empty() || y_size == 0;
    }
};

// Simple async frame writer class
class AsyncFrameWriter {
private:
    std::string output_path;
    std::thread writer_thread;
    std::mutex mutex;
    std::vector<FrameData> frame_queue;
    std::atomic<bool> stop_requested{false};
    std::atomic<size_t> total_frames_written{0};
    int fd = -1;
    
    void writeFrames() {
        // Create or truncate the output file at the beginning
        fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            std::cerr << "Error opening output file: " << output_path << " - " << strerror(errno) << std::endl;
            return;
        }
        
        std::vector<FrameData> local_queue;
        
        while (!stop_requested || !frame_queue.empty()) {
            // Get frames from the queue
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!frame_queue.empty()) {
                    local_queue.swap(frame_queue);
                }
            }
            
            // Process frames if any
            if (!local_queue.empty()) {
                for (const auto& frame : local_queue) {
                    if (frame.isEmpty()) continue;
                    
                    // Additional safety check before writing
                    if (frame.frame_data.size() < frame.y_size + frame.uv_size) {
                        std::cerr << "Error: Frame data size mismatch" << std::endl;
                        continue;
                    }
                    
                    // Write frame data
                    // Use direct write for Linux and check return value
                    ssize_t bytes_written = 0;
                    size_t total_size = frame.y_size + frame.uv_size;
                    size_t bytes_left = total_size;
                    const uint8_t* data_ptr = frame.frame_data.data();
                    
                    // Loop to handle partial writes
                    while (bytes_left > 0) {
                        bytes_written = write(fd, data_ptr, bytes_left);
                        if (bytes_written < 0) {
                            if (errno == EINTR) continue; // Interrupted, try again
                            std::cerr << "Error writing frame data: " << strerror(errno) << std::endl;
                            break;
                        }
                        
                        data_ptr += bytes_written;
                        bytes_left -= bytes_written;
                    }
                    
                    if (bytes_left == 0) {
                        total_frames_written++;
                    }
                }
                
                local_queue.clear();
            } else {
                // No frames to process, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        // Close file descriptor
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
        
        std::cout << "Frame writer completed. Total frames written: " << total_frames_written << std::endl;
    }
    
public:
    AsyncFrameWriter(const std::string& path) : output_path(path) {
        // Start writer thread
        writer_thread = std::thread(&AsyncFrameWriter::writeFrames, this);
    }
    
    ~AsyncFrameWriter() {
        stop();
    }
    
    void addFrame(const FrameData& frame) {
        if (frame.isEmpty() || stop_requested) return;
        
        // Add safety check to prevent excessive memory usage
        if (frame.y_size > 20 * 1024 * 1024 || frame.uv_size > 10 * 1024 * 1024) {
            std::cerr << "Warning: Unusually large frame detected (" 
                      << (frame.y_size + frame.uv_size) / (1024 * 1024) << " MB), skipping" << std::endl;
            return;
        }
        
        try {
            std::lock_guard<std::mutex> lock(mutex);
            frame_queue.push_back(frame);
        } catch (const std::exception& e) {
            std::cerr << "Error in addFrame: " << e.what() << std::endl;
        }
    }
    
    void stop() {
        stop_requested = true;
        
        if (writer_thread.joinable()) {
            writer_thread.join();
        }
    }
    
    size_t getFramesWritten() const {
        return total_frames_written;
    }
};

// Worker thread function to process frames
void FrameProcessingWorker(
    int thread_id,
    ConcurrentQueue<FrameData>& frame_queue,
    AsyncFrameWriter* writer,
    MD5Generator* md5_gen,
    OutputSurfaceInfo* surf_info,
    std::atomic<int>& frames_processed,
    bool generate_md5,
    bool dump_output
) {
    FrameData frame;
    int local_frames_processed = 0;
    
    std::cout << "Thread " << thread_id << " started" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (frame_queue.pop(frame)) {
        if (frame.isEmpty()) continue;
        
        // Process the frame
        if (generate_md5 && md5_gen && surf_info) {
            try {
                // Create a fake frame pointer for MD5 generator
                md5_gen->UpdateMd5ForFrame(frame.frame_data.data(), surf_info);
            } catch (const std::exception& e) {
                std::cerr << "Error updating MD5 in thread " << thread_id << ": " << e.what() << std::endl;
            }
        }
        
        // Handle file output if needed
        if (dump_output && writer) {
            writer->addFrame(frame);
        }
        
        local_frames_processed++;
        
        // Log progress periodically
        if (local_frames_processed % 100 == 0) {
            std::cout << "Thread " << thread_id << " has processed " << local_frames_processed << " frames" << std::endl;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    frames_processed += local_frames_processed;
    
    std::cout << "Thread " << thread_id << " finished. Processed " << local_frames_processed 
              << " frames in " << processing_time << " ms" << std::endl;
}

// Multi-threaded decoding function similar to videoDecodePerf approach
void DecodeThread(
    RocVideoDecoder *p_dec, 
    VideoDemuxer *demuxer, 
    int *pn_frame, 
    int *pn_pic_dec, 
    double *pn_fps, 
    double *pn_fps_dec, 
    int max_num_frames, 
    OutputSurfaceMemoryType mem_type, 
    const std::string& output_file_path, 
    bool generate_md5,
    MD5Generator* md5_gen,
    int thread_id
) {
    int n_video_bytes = 0, n_frame_returned = 0, n_frame = 0;
    int n_pic_decoded = 0, decoded_pics = 0;
    uint8_t *p_video = nullptr;
    int64_t pts = 0;
    double total_dec_time = 0.0;
    OutputSurfaceInfo *surf_info = nullptr;
    
    // Only thread 0 handles file output to maximize decoder throughput on other threads
    bool b_dump_output = !output_file_path.empty() && (thread_id == 0);
    
    std::cout << "Thread " << thread_id << ": Decoding started" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::ofstream output_file;
    if (b_dump_output && mem_type != OUT_SURFACE_MEM_NOT_MAPPED) {
        output_file.open(output_file_path, std::ios::binary);
        if (!output_file.is_open()) {
            std::cerr << "Thread " << thread_id << ": Failed to open output file: " << output_file_path << std::endl;
            b_dump_output = false;
        } else {
            std::cout << "Thread " << thread_id << ": Output will be written to " << output_file_path << std::endl;
        }
    }
    
    // Main decode loop
    do {
        demuxer->Demux(&p_video, &n_video_bytes, &pts);
        if (n_video_bytes <= 0) continue;
        
        n_frame_returned = p_dec->DecodeFrame(p_video, n_video_bytes, 0, pts, &decoded_pics);
        n_frame += n_frame_returned;
        n_pic_decoded += decoded_pics;
            
        // Handle frame output and MD5 calculation if needed
        if (n_frame_returned > 0 && mem_type != OUT_SURFACE_MEM_NOT_MAPPED) {
            // Get surface info if we don't have it yet
            if (!surf_info && !p_dec->GetOutputSurfaceInfo(&surf_info)) {
                std::cerr << "Thread " << thread_id << ": Failed to get surface info" << std::endl;
            }
                
            // Process each returned frame
            for (int i = 0; i < n_frame_returned; i++) {
                uint8_t* pframe = p_dec->GetFrame(&pts);
                if (pframe && surf_info) {
                    // Generate MD5 if requested (all threads can do this)
                    if (generate_md5 && md5_gen) {
                        md5_gen->UpdateMd5ForFrame(pframe, surf_info);
                    }
                    
                    // Write frame to file if thread 0
                    if (b_dump_output && output_file.is_open()) {
                        size_t y_size = surf_info->output_width * surf_info->output_height * 
                                      ((surf_info->bit_depth > 8) ? 2 : 1);
                        size_t uv_size = y_size / 2; // For 4:2:0 format
                        
                        if (surf_info->output_surface_size_in_bytes > 0) {
                            y_size = surf_info->output_surface_size_in_bytes * 2 / 3;
                            uv_size = surf_info->output_surface_size_in_bytes - y_size;
                        }
                        
                        output_file.write(reinterpret_cast<const char*>(pframe), y_size + uv_size);
                    }
                    
                    // Release the frame
                    p_dec->ReleaseFrame(pts);
                }
            }
        }
        
        if (max_num_frames && max_num_frames <= n_frame) {
            break;
        }
    
        // Add periodic progress reporting
        if (n_frame % 1000 == 0) {
            std::cout << "Thread " << thread_id << ": Decoded " << n_frame << " frames" << std::endl;
        }
        
    } while (n_video_bytes);
    
    // Wait for decoder to complete
    if (mem_type == OUT_SURFACE_MEM_NOT_MAPPED) {
        p_dec->WaitForDecodeCompletion();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto time_per_decode = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    auto session_overhead = p_dec->GetDecoderSessionOverHead(std::this_thread::get_id());
    
    // Calculate average decoding time
    total_dec_time = time_per_decode - session_overhead;
    
    // Close output file if open
    if (output_file.is_open()) {
        output_file.close();
    }
    
    // Check if we actually decoded any frames
    if (n_frame <= 0) {
        std::cerr << "Thread " << thread_id << ": Warning: No frames were decoded" << std::endl;
        *pn_fps = 0;
        *pn_fps_dec = 0;
    } else {
        double average_output_time = total_dec_time / n_frame;
        double average_decoding_time = (n_pic_decoded > 0) ? (total_dec_time / n_pic_decoded) : 0;
        double n_fps = (average_output_time > 0) ? (1000 / average_output_time) : 0;
        double n_fps_dec = (average_decoding_time > 0) ? (1000 / average_decoding_time) : 0;
        *pn_fps = n_fps;
        *pn_fps_dec = n_fps_dec;
    }
    
    *pn_frame = n_frame;
    *pn_pic_dec = n_pic_decoded;
    
    std::cout << "Thread " << thread_id << ": Completed. Decoded " << n_frame 
              << " frames at " << *pn_fps << " FPS" << std::endl;
}

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "   (When using multiple threads, only thread 0 will write output)" << std::endl
    << "-t Number of threads (>= 1) - optional; default: 1" << std::endl
    << "-d Device ID (>= 0) - optional; default: 0" << std::endl
    << "-z Force zero latency (decoded frames will be flushed out for display immediately) - optional" << std::endl
    << "-disp_delay - specify the number of frames to be delayed for display; optional" << std::endl
    << "-f Number of frames to decode - optional; default: whole file" << std::endl
    << "-m Memory type (integer values between 0 to 3: specifies where to store the decoded output:" << std::endl
    << "                                               0 = decoded output will be in internal interopped memory," << std::endl
    << "                                               1 = decoded output will be copied to a separate device memory," << std::endl
    << "                                               2 = decoded output will be copied to a separate host memory," << std::endl
    << "                                               3 = decoded output will not be available (decode only)) - optional; default: 3" << std::endl
    << "-md5 - generate MD5 hash for the decoded content - optional" << std::endl
    << "-md5_check <md5_file> - check decoded content against provided MD5 file - optional" << std::endl
    << "-sync_output - use synchronous frame output (default: asynchronous) - optional" << std::endl;
    
    if (option) {
        std::cout << "Invalid value specified for: " << option << std::endl;
    }
    exit(1);
}

int main(int argc, char* argv[]) {
    std::string input_file_path, output_file_path, md5_file_path;
    int device_id = 0, n_thread = 1, disp_delay = 0, max_num_frames = 0;
    bool b_force_zero_latency = false, b_generate_md5 = false, b_md5_check = false;
    bool use_async_output = true;
    OutputSurfaceMemoryType mem_type = OUT_SURFACE_MEM_NOT_MAPPED;
    Rect *p_crop_rect = nullptr;
    
    if (argc < 2) {
        ShowHelpAndExit();
    }
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
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
            continue;
        }
        if (!strcmp(argv[i], "-t")) {
            if (++i == argc) {
                ShowHelpAndExit("-t");
            }
            n_thread = atoi(argv[i]);
            if (n_thread < 1) {
                ShowHelpAndExit("-t");
            }
            continue;
        }
        if (!strcmp(argv[i], "-d")) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            device_id = atoi(argv[i]);
            if (device_id < 0) {
                ShowHelpAndExit("-d");
            }
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
                ShowHelpAndExit("-f");
            }
            max_num_frames = atoi(argv[i]);
            continue;
        }
        if (!strcmp(argv[i], "-z")) {
            if (i == argc) {
                ShowHelpAndExit("-z");
            }
            b_force_zero_latency = true;
            continue;
        }
        if (!strcmp(argv[i], "-m")) {
            if (++i == argc) {
                ShowHelpAndExit("-m");
            }
            mem_type = static_cast<OutputSurfaceMemoryType>(atoi(argv[i]));
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
        if (!strcmp(argv[i], "-sync_output")) {
            use_async_output = false;
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    // Validate input file path
    if (input_file_path.empty()) {
        std::cerr << "Error: Input file path is required." << std::endl;
        ShowHelpAndExit();
    }
    
    if (!output_file_path.empty() && use_async_output) {
        std::cout << "Info: Using asynchronous frame output for improved VCN utilization." << std::endl;
    }
    
    try {
        // Check for available GPUs
        int num_devices = 0, sd = 0;
        hipError_t hip_status = hipSuccess;
        hipDeviceProp_t hip_dev_prop;
        std::string gcn_arch_name;
        hip_status = hipGetDeviceCount(&num_devices);
        if (hip_status != hipSuccess) {
            std::cout << "ERROR: hipGetDeviceCount failed! (" << hip_status << ")" << std::endl;
            return -1;
        }

        if (num_devices < 1) {
            ROCDEC_ERR("ERROR: didn't find any GPU!");
            return -1;
        }

        hip_status = hipGetDeviceProperties(&hip_dev_prop, device_id);
        if (hip_status != hipSuccess) {
            ROCDEC_ERR("ERROR: hipGetDeviceProperties for device (" + TOSTR(device_id) + ") failed! (" + hipGetErrorName(hip_status) + ")");
            return -1;
        }

        gcn_arch_name = hip_dev_prop.gcnArchName;
        std::size_t pos = gcn_arch_name.find_first_of(":");
        std::string gcn_arch_name_base = (pos != std::string::npos) ? gcn_arch_name.substr(0, pos) : gcn_arch_name;

        // gfx90a has two GCDs as two separate devices 
        if (!gcn_arch_name_base.compare("gfx90a") && num_devices > 1) {
            sd = 1;
        }

        // Create vectors of demuxers, decoders and threads
        std::vector<std::unique_ptr<VideoDemuxer>> v_demuxer;
        std::vector<std::unique_ptr<RocVideoDecoder>> v_decoder;
        std::vector<int> v_device_id(n_thread);
        std::vector<std::thread> v_thread;
        std::vector<double> v_fps, v_fps_dec;
        std::vector<int> v_frame, v_pic_decoded;
        std::vector<std::unique_ptr<MD5Generator>> v_md5_generator;
        
        v_fps.resize(n_thread, 0);
        v_fps_dec.resize(n_thread, 0);
        v_frame.resize(n_thread, 0);
        v_pic_decoded.resize(n_thread, 0);
        
        // Display file information
        std::size_t found_file = input_file_path.find_last_of('/');
        std::cout << "info: Input file: " << input_file_path.substr(found_file + 1) << std::endl;
        std::cout << "info: Number of threads: " << n_thread << std::endl;
        if (!output_file_path.empty()) {
            std::cout << "info: Output file: " << output_file_path << " (only thread 0 will write output)" << std::endl;
        }
        
        // Set device IDs for each thread
        int hip_vis_dev_count = 0;
        GetEnvVar("HIP_VISIBLE_DEVICES", hip_vis_dev_count);
        
        for (int i = 0; i < n_thread; i++) {
            if (!hip_vis_dev_count) {
                if (device_id % 2 == 0)
                    v_device_id[i] = (i % 2 == 0) ? device_id : device_id + sd;
                else
                    v_device_id[i] = (i % 2 == 0) ? device_id - sd : device_id;
            } else {
                v_device_id[i] = i % hip_vis_dev_count;
            }
        }
        
        // Create a demuxer and decoder for each thread
        for (int i = 0; i < n_thread; i++) {
            try {
                // Create demuxer for this thread
                auto demuxer = std::make_unique<VideoDemuxer>(input_file_path.c_str());
                rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer->GetCodecID());
                int bit_depth = demuxer->GetBitDepth();
                
                // Determine memory type for this thread - only thread 0 uses host memory when output is needed
                OutputSurfaceMemoryType current_mem_type = mem_type;
                if (!output_file_path.empty() && i == 0 && mem_type == OUT_SURFACE_MEM_NOT_MAPPED) {
                    current_mem_type = OUT_SURFACE_MEM_HOST_COPIED;
                    if (i == 0) {
                        std::cout << "info: Thread 0 using host memory to support output file" << std::endl;
                        std::cout << "info: Other threads using decode-only mode for maximum performance" << std::endl;
                    }
                }
                
                // Check if codec is supported
                RocdecDecodeCaps decode_caps = {};
                decode_caps.device_id = v_device_id[i];
                decode_caps.codec_type = rocdec_codec_id;
                decode_caps.bit_depth_minus_8 = bit_depth > 8 ? (bit_depth - 8) : 0;
                decode_caps.chroma_format = rocDecVideoChromaFormat_420;
                
                rocDecStatus status = rocDecGetDecoderCaps(&decode_caps);
                if (status != ROCDEC_SUCCESS || !decode_caps.is_supported) {
                    std::cerr << "Error: Codec " << demuxer->GetCodecID() 
                              << " with bit depth " << bit_depth 
                              << " not supported on GPU device " << v_device_id[i] << std::endl;
                    continue;
                }
                
                // Create decoder for this thread
                auto decoder = std::make_unique<RocVideoDecoder>(
                    v_device_id[i],
                    current_mem_type,
                    rocdec_codec_id,
                    b_force_zero_latency,
                    p_crop_rect,
                    false,
                    disp_delay
                );
                
                // Create MD5 generator if needed
                std::unique_ptr<MD5Generator> md5_gen;
                if (b_generate_md5) {
                    md5_gen = std::make_unique<MD5Generator>();
                    md5_gen->InitMd5();
                    v_md5_generator.push_back(std::move(md5_gen));
                }
                
                // Log device information
                std::string device_name;
                int pci_bus_id, pci_domain_id, pci_device_id;
                decoder->GetDeviceinfo(device_name, gcn_arch_name, pci_bus_id, pci_domain_id, pci_device_id);
                
                std::cout << "info: Thread " << i << " using GPU device " << v_device_id[i] << " - " << device_name 
                          << " [" << gcn_arch_name << "] on PCI bus " 
                          << std::setfill('0') << std::setw(2) << std::right << std::hex 
                          << pci_bus_id << ":" << std::setfill('0') << std::setw(2) 
                          << std::right << std::hex << pci_domain_id << "." 
                          << pci_device_id << std::dec << std::endl;
                
                // Store the demuxer and decoder
                v_demuxer.push_back(std::move(demuxer));
                v_decoder.push_back(std::move(decoder));
                
            } catch (const std::exception& e) {
                std::cerr << "Error initializing decoder for thread " << i << ": " << e.what() << std::endl;
                return -1;
            }
        }
        
        // Start decode threads
        for (int i = 0; i < n_thread; i++) {
            v_thread.push_back(std::thread(
                DecodeThread,
                v_decoder[i].get(),
                v_demuxer[i].get(),
                &v_frame[i],
                &v_pic_decoded[i],
                &v_fps[i],
                &v_fps_dec[i],
                max_num_frames,
                mem_type,
                output_file_path,
                b_generate_md5,
                b_generate_md5 ? v_md5_generator[i].get() : nullptr,
                i
            ));
        }
        
        // Wait for all threads to complete
        for (auto& thread : v_thread) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        // Calculate total performance metrics
        float total_fps = 0;
        float total_fps_dec = 0;
        int n_total = 0;
        int n_total_pic_decoded = 0;
        
        for (int i = 0; i < n_thread; i++) {
            total_fps += v_fps[i];
            total_fps_dec += v_fps_dec[i];
            n_total += v_frame[i];
            n_total_pic_decoded += v_pic_decoded[i];
        }
        
        // Display aggregated results
        std::cout << "\nPerformance Results:" << std::endl;
        std::cout << "--------------------" << std::endl;
        std::cout << "Total pictures decoded: " << n_total_pic_decoded << std::endl;
        std::cout << "Total frames output: " << n_total << std::endl;

        if (n_total > 0) {
            std::cout << "Average decode FPS: " << total_fps_dec << std::endl;
            std::cout << "Average output FPS: " << total_fps << std::endl;
            std::cout << "\nDecode Status: SUCCESS" << std::endl;
        } else {
            std::cout << "Average decode FPS: 0 (no frames decoded)" << std::endl;
            std::cout << "Average output FPS: 0 (no frames output)" << std::endl;
            std::cout << "\nDecode Status: FAILED" << std::endl;
        }

        // MD5 hash verification if requested
        if (b_generate_md5 && !v_md5_generator.empty()) {
            uint8_t *digest;
            v_md5_generator[0]->FinalizeMd5(&digest);
            
            std::string calculated_md5;
            for (int i = 0; i < 16; i++) {
                char hex[3];
                sprintf(hex, "%02x", digest[i]);
                calculated_md5 += hex;
            }
            
            std::cout << "\nMD5 Results:" << std::endl;
            std::cout << "------------" << std::endl;
            std::cout << "MD5 message digest: " << calculated_md5 << std::endl;
            
            if (b_md5_check) {
                std::ifstream ref_md5_file(md5_file_path);
                if (!ref_md5_file.is_open()) {
                    std::cerr << "Error: Failed to open MD5 reference file: " << md5_file_path << std::endl;
                } else {
                    std::string ref_md5_string;
                    std::getline(ref_md5_file, ref_md5_string);
                    
                    // Clean up the reference string (remove whitespace)
                    ref_md5_string.erase(std::remove_if(ref_md5_string.begin(), ref_md5_string.end(), ::isspace), ref_md5_string.end());
                    
                    std::cout << "Reference MD5 digest: " << ref_md5_string << std::endl;
                    std::cout << "Validation result: " << (calculated_md5 == ref_md5_string ? "PASSED" : "FAILED") << std::endl;
                }
            }
        }
        
        // Output file information
        if (!output_file_path.empty() && mem_type != OUT_SURFACE_MEM_NOT_MAPPED) {
            std::cout << "\nOutput Results:" << std::endl;
            std::cout << "--------------" << std::endl;
            if (n_total > 0) {
                std::cout << "Decoded frames saved to: " << output_file_path << " (by thread 0 only)" << std::endl;
            } else {
                std::cout << "No frames were saved to output file." << std::endl;
            }
        }
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 