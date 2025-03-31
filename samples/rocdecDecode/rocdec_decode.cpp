#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <hip/hip_runtime.h>
#include <rocdecode/rocdecode.h>
#include "rocdecode_host.h"

struct Rect {
    int left;
    int top;
    int right;
    int bottom;
};

__attribute__((visibility("hidden"))) inline bool is_error(rocDecStatus status)
{
    return status != ROCDEC_SUCCESS;
}

__attribute__((visibility("hidden"))) inline const char* error_string(rocDecStatus status)
{
    return rocDecGetErrorName(status);
}

template <typename Status, typename... Args>
__attribute__((visibility("hidden"))) inline void report_error(
    Status status, const char* function_name, const char* file_name, int line, Args&&... args)
{
    ((std::cerr << "ERROR: " << error_string(status) << "; " << function_name << "; "
                << file_name << ":" << line)
     << ... << std::forward<Args>(args))
        << std::endl;
    std::abort();
}

//hardcoding for testing
#define MAX_WIDTH 2912
#define MAX_HEIGHT 1888


#define CHECK(callable, ...)                                                             \
    do                                                                                   \
    {                                                                                    \
        auto status__ = callable; /* invoke the callable and assign the return status */ \
        /*std::cout << #callable << " returned " << (int)status__ << std::endl;*/            \
        if (is_error(status__))                                                          \
        {                                                                                \
            report_error(status__, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__);     \
        }                                                                                \
    } while (false)

struct DecoderInfo {
    int dec_device_id;
    rocDecDecoderHandle decoder;
    RocdecVideoParser parser;
    std::uint32_t bit_depth;
    rocDecVideoCodec rocdec_codec_id;
    DecoderInfo() : dec_device_id(0), decoder(nullptr), bit_depth(8) {}
};
    

std::vector<std::vector<uint8_t>> read_frames(std::vector<std::string>& names) {
    std::vector<std::vector<uint8_t>> frames;
    for (std::string name : names) {
        std::ifstream inputFile(name.c_str(), std::ios::binary);
        if (!inputFile) {
            std::cerr << "Error opening " << name << " for reading." << std::endl;
            std::abort();
        }

        // Determine the file size
        inputFile.seekg(0, std::ios::end);
        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        // Read the file contents into a byte array
        std::vector<uint8_t> frame(fileSize);
        if (!inputFile.read(reinterpret_cast<char*>(frame.data()), fileSize)) {
            std::cerr << "Error reading " << name << "." << std::endl;
            std::abort();
        }

        // Close the file
        inputFile.close();

        frames.push_back(std::move(frame));
    }

    return frames;
}

void init() {
}

void create_decoder(DecoderInfo& dec_info) {
    RocDecoderCreateInfo create_info = {};
    create_info.codec_type = rocDecVideoCodec_HEVC;
    create_info.max_width = MAX_WIDTH;
    create_info.max_height = MAX_HEIGHT;
    create_info.width = MAX_WIDTH;
    create_info.height = MAX_HEIGHT;
    create_info.num_decode_surfaces = 6;
    create_info.target_width = MAX_WIDTH;
    create_info.target_height = MAX_HEIGHT;
    create_info.display_rect.left = 0;
    create_info.display_rect.right = static_cast<short>(MAX_WIDTH);
    create_info.display_rect.top = 0;
    create_info.display_rect.bottom = static_cast<short>(MAX_HEIGHT);
    create_info.chroma_format = rocDecVideoChromaFormat_420;
    create_info.output_format = rocDecVideoSurfaceFormat_P016;
    create_info.bit_depth_minus_8 = 2;
    create_info.num_output_surfaces = 1;
    CHECK(rocDecCreateDecoder(&dec_info.decoder, &create_info));
}

int ROCDECAPI handle_video_sequence_host(void* user_data, RocdecVideoFormat* format) {
    // std::cout << "handle_video_sequence is called" << std::endl;
    DecoderInfo *p_dec_info = static_cast<DecoderInfo *>(user_data);
    RocdecReconfigureDecoderInfo reconfig_params = {};
    reconfig_params.width = format->coded_width;
    reconfig_params.height = format->coded_height;
    reconfig_params.num_decode_surfaces = 6;
    reconfig_params.target_width = format->coded_width;
    reconfig_params.target_height = format->coded_height;
    reconfig_params.display_rect.left = 0;
    reconfig_params.display_rect.right = static_cast<short>(format->coded_width);
    reconfig_params.display_rect.top = 0;
    reconfig_params.display_rect.bottom = static_cast<short>(format->coded_height);
    CHECK(rocDecReconfigureDecoder(p_dec_info->decoder, &reconfig_params));
    return 1;
}

int ROCDECAPI handle_picture_display_host(void* user_data, void* disp_info) {
    // std::cout << "handle_picture_display is called" << std::endl;
    DecoderInfo *p_dec_info = static_cast<DecoderInfo *>(user_data);
    RocdecParserDispInfo *p_disp_info = static_cast<RocdecParserDispInfo *>(disp_info);
    RocdecProcParams params = {};
    params.progressive_frame = p_disp_info->progressive_frame;
    params.top_field_first = p_disp_info->top_field_first;
    void* frame_mem_ptr[3] = {0};
    uint32_t pitch[3] = {0};
    CHECK(rocDecGetVideoFrameHost(p_dec_info->decoder, p_disp_info->picture_index, frame_mem_ptr, pitch, &params));
    return 1;
}

void create_host_decoder(DecoderInfo& dec_info) {
    RocDecoderHostCreateInfo create_info = {};
    create_info.codec_type = rocDecVideoCodec_HEVC;
    create_info.num_decode_threads = 0;     // default
    create_info.max_width = MAX_WIDTH;
    create_info.max_height = MAX_HEIGHT;
    create_info.width = MAX_WIDTH;
    create_info.height = MAX_HEIGHT;
    create_info.target_width = MAX_WIDTH;
    create_info.target_height = MAX_HEIGHT;
    create_info.display_rect.left = 0;
    create_info.display_rect.right = static_cast<short>(MAX_WIDTH);
    create_info.display_rect.top = 0;
    create_info.display_rect.bottom = static_cast<short>(MAX_HEIGHT);
    create_info.chroma_format = rocDecVideoChromaFormat_420;
    create_info.output_format = rocDecVideoSurfaceFormat_P016;
    create_info.bit_depth_minus_8 = 2;
    create_info.num_output_surfaces = 1;
    create_info.user_data = &dec_info;
    create_info.pfn_sequence_callback = handle_video_sequence_host;
    create_info.pfn_display_picture = handle_picture_display_host;
    CHECK(rocDecCreateDecoderHost(&dec_info.decoder, &create_info));
}

int ROCDECAPI handle_video_sequence(void* user_data, RocdecVideoFormat* format) {
    // std::cout << "handle_video_sequence is called" << std::endl;
    DecoderInfo *p_dec_info = static_cast<DecoderInfo *>(user_data);
    RocdecReconfigureDecoderInfo reconfig_params = {};
    reconfig_params.width = format->coded_width;
    reconfig_params.height = format->coded_height;
    reconfig_params.num_decode_surfaces = 6;
    reconfig_params.target_width = format->coded_width;
    reconfig_params.target_height = format->coded_height;
    reconfig_params.display_rect.left = 0;
    reconfig_params.display_rect.right = static_cast<short>(format->coded_width);
    reconfig_params.display_rect.top = 0;
    reconfig_params.display_rect.bottom = static_cast<short>(format->coded_height);
    CHECK(rocDecReconfigureDecoder(p_dec_info->decoder, &reconfig_params));
    return 1;
}

int ROCDECAPI handle_picture_decode(void* user_data, RocdecPicParams* params) {
    // std::cout << "handle_picture_decode is called" << std::endl;
    DecoderInfo *p_dec_info = static_cast<DecoderInfo *>(user_data);
    CHECK(rocDecDecodeFrame(p_dec_info->decoder, params));
    return 1;
}

int ROCDECAPI handle_picture_display(void* user_data, RocdecParserDispInfo* disp_info) {
    // std::cout << "handle_picture_display is called" << std::endl;
    DecoderInfo *p_dec_info = static_cast<DecoderInfo *>(user_data);
    RocdecProcParams params = {};
    params.progressive_frame = disp_info->progressive_frame;
    params.top_field_first = disp_info->top_field_first;
    void* dev_mem_ptr[3] = {0};
    uint32_t pitch[3] = {0};
    CHECK(rocDecGetVideoFrame(p_dec_info->decoder, disp_info->picture_index, dev_mem_ptr, pitch, &params));
    return 1;
}


void create_parser(DecoderInfo& dec_info) {
    RocdecParserParams params = {};
    params.codec_type = rocDecVideoCodec_HEVC;
    params.max_num_decode_surfaces = 6;
    params.max_display_delay = 0;
    params.user_data = &dec_info;
    params.pfn_sequence_callback = handle_video_sequence;
    params.pfn_decode_picture = handle_picture_decode;
    params.pfn_display_picture = handle_picture_display;
    CHECK(rocDecCreateVideoParser(&dec_info.parser, &params));
}

void decode_frames(DecoderInfo& dec_info, const std::vector<std::vector<uint8_t>>& frames) {
    for (int i=0; i<frames.size(); ++i) {
        std::cout << "Parsing frame " << i << std::endl;
        RocdecSourceDataPacket packet = {};
        packet.payload_size = frames[i].size();
        packet.payload = frames[i].data();
        packet.flags = ROCDEC_PKT_ENDOFPICTURE;
        CHECK(rocDecParseVideoData(dec_info.parser, &packet));
    }
}

void destroy_decoder(DecoderInfo& dec_info) {
    CHECK(rocDecDestroyDecoder(dec_info.decoder));
}

void destroy_parser(DecoderInfo& dec_info) {
    CHECK(rocDecDestroyVideoParser(dec_info.parser));
}

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-b backend (0 for GPU, 1 CPU-FFMpeg); optional; default: 0" << std::endl
    << "-n Number of iteration - specify the number of iterations for performance evaluation; optional; default: 1" << std::endl
    << "-m output_surface_memory_type - decoded surface memory; optional; default - 0"
    << " [0 : OUT_SURFACE_MEM_DEV_INTERNAL/ 1 : OUT_SURFACE_MEM_DEV_COPIED/ 2 : OUT_SURFACE_MEM_HOST_COPIED/ 3 : OUT_SURFACE_MEM_NOT_MAPPED]" << std::endl;
    exit(0);
}


int main(int argc, char** argv) {

    std::string input_file_path, output_file_path;
    int dump_output_frames = 0;
    int device_id = 0;
    int backend = 0;
    int num_iterations = 1; 
    bool b_extract_sei_messages = false;
    bool b_flush_frames_during_reconfig = true;
    Rect crop_rect = {};
    Rect *p_crop_rect = nullptr;
    std::vector<std::string> input_file_names;

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
            if (std::filesystem::is_directory(input_file_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(input_file_path)) {
                    input_file_names.push_back(entry.path());
                 }                    
            } else {
                input_file_names.push_back(input_file_path);
            }
            std::cout << "Read " << input_file_names.size() << " frames from disk." << std::endl;
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
        if (!strcmp(argv[i], "-b")) {
            if (++i == argc) {
                ShowHelpAndExit("-b");
            }
            backend = atoi(argv[i]);
            continue;
        }

        if (!strcmp(argv[i], "-d")) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            device_id = atoi(argv[i]);
            continue;
        }
        if (!strcmp(argv[i], "-n")) {
            if (++i == argc) {
                ShowHelpAndExit("-n");
            }
            num_iterations = atoi(argv[i]);
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
        ShowHelpAndExit(argv[i]);
    }

    DecoderInfo dec_info;    
    init();
    if (!backend) {
        create_parser(dec_info);
        create_decoder(dec_info);
    } else {
        create_host_decoder(dec_info);
    }
    auto input_frames = read_frames(input_file_names);
    decode_frames(dec_info, input_frames);  // warmup
    auto start = std::chrono::high_resolution_clock::now();
    for (int i=0; i<num_iterations; i++)
        decode_frames(dec_info, input_frames);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Decoding time: " << elapsed << " microseconds" << std::endl;
    destroy_decoder(dec_info);
    destroy_parser(dec_info);
    std::cout << "Success." << std::endl << std::endl << std::endl;
    return 0;
}
