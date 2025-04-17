#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <hip/hip_runtime.h>
#include <rocdecode/rocdecode.h>
#include <rocdecode/rocdecode_host.h>


struct Rect {
    int left;
    int top;
    int right;
    int bottom;
};

static inline int align(int value, int alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

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

static void GetSurfaceStrideInternal(rocDecVideoSurfaceFormat surface_format, uint32_t width, uint32_t height, uint32_t *pitch, uint32_t *vstride, uint32_t &num_of_chroma_planes) {

    switch (surface_format) {
    case rocDecVideoSurfaceFormat_NV12:
        pitch[0] = align(width, 256);
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 1;
        break;
    case rocDecVideoSurfaceFormat_P016:
        pitch[0] = align(width, 128) * 2;
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 1;
        break;
    case rocDecVideoSurfaceFormat_YUV444:
        pitch[0] = align(width, 256);
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 2;
        break;
    case rocDecVideoSurfaceFormat_YUV444_16Bit:
        pitch[0] = align(width, 128) * 2;
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 2;
        break;
    case rocDecVideoSurfaceFormat_YUV420:
        pitch[0] = align(width, 256);
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 2;
        break;
    case rocDecVideoSurfaceFormat_YUV420_16Bit:
        pitch[0] = align(width, 128) * 2;
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 2;
        break;
    case rocDecVideoSurfaceFormat_YUV422:
        pitch[0] = align(width, 256);
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 2;
        break;
    case rocDecVideoSurfaceFormat_YUV422_16Bit:
        pitch[0] = align(width, 128) * 2;
        pitch[1] = pitch[0];
        *vstride = align(height, 16);
        num_of_chroma_planes = 2;
        break;
    }
    return;
}

inline float GetChromaHeightFactor(rocDecVideoSurfaceFormat surface_format) {
    float factor = 0.5;
    switch (surface_format) {
    case rocDecVideoSurfaceFormat_NV12:
    case rocDecVideoSurfaceFormat_P016:
    case rocDecVideoSurfaceFormat_YUV420:
    case rocDecVideoSurfaceFormat_YUV420_16Bit:
        factor = 0.5;
        break;
    case rocDecVideoSurfaceFormat_YUV422:
    case rocDecVideoSurfaceFormat_YUV422_16Bit:
    case rocDecVideoSurfaceFormat_YUV444:
    case rocDecVideoSurfaceFormat_YUV444_16Bit:
        factor = 1.0;
        break;
    }

    return factor;
};


// only 2 types of memory mode is supported in this sample for simplicity.
// please refer to VideoDecode sample for all memory types support
typedef enum OutputSurfaceMemoryType_enum {
    OUT_SURFACE_MEM_DEV_INTERNAL = 0,      /**<  Internal interopped decoded surface memory(original mapped decoded surface) */
    OUT_SURFACE_MEM_HOST = 2,        /**<  decoded output will be in host memory (true for host based decoding) **/
} OutputSurfaceMemoryType;

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
    int dump_decoded_frames;
    std::string output_file_path;
    OutputSurfaceMemoryType mem_type;
    rocDecVideoSurfaceFormat surf_format;
    rocDecVideoSurfaceFormat video_chroma_format;
    uint32_t coded_width, coded_height;
    uint32_t bytes_per_pixel;
    uint64_t output_surface_size_in_bytes;
    bool is_decoder_reconfigured;
    Rect disp_rect;
    FILE *fp_out;
    DecoderInfo() : dec_device_id(0), decoder(nullptr), bit_depth(8), dump_decoded_frames(0), mem_type{OUT_SURFACE_MEM_DEV_INTERNAL},
                    surf_format{rocDecVideoSurfaceFormat_NV12}, video_chroma_format{rocDecVideoSurfaceFormat_NV12},
                    output_file_path{nullptr}, is_decoder_reconfigured{false}, fp_out{nullptr} {}
};

void save_frame_to_file(DecoderInfo *p_dec_info, void *surf_mem, uint32_t *pitch, uint32_t vpitch, uint32_t num_chroma_planes) {

    uint8_t *hst_ptr = nullptr;
    if (p_dec_info->mem_type == OUT_SURFACE_MEM_DEV_INTERNAL) {
        uint64_t output_image_size = p_dec_info->output_surface_size_in_bytes;
        if (hst_ptr == nullptr) {
            hst_ptr = new uint8_t [output_image_size];
        }
        hipError_t hip_status = hipSuccess;
        hip_status = hipMemcpyDtoH((void *)hst_ptr, surf_mem, output_image_size);
        if (hip_status != hipSuccess) {
            std::cerr << "ERROR: hipMemcpyDtoH failed! (" << hipGetErrorName(hip_status) << ")" << std::endl;
            delete [] hst_ptr;
            return;
        }
    } else
        hst_ptr = static_cast<uint8_t *> (surf_mem);

    if (p_dec_info->is_decoder_reconfigured) {
        if (p_dec_info->fp_out) {
            fclose(p_dec_info->fp_out);
            p_dec_info->fp_out = nullptr;
        }
        p_dec_info->is_decoder_reconfigured = false;
    }

    if (p_dec_info->fp_out == nullptr && !p_dec_info->output_file_path.empty()) {
        p_dec_info->fp_out = fopen(p_dec_info->output_file_path.c_str(), "wb");
    }

    if (p_dec_info->fp_out) {
        uint8_t *tmp_hst_ptr = hst_ptr;
        if (p_dec_info->mem_type == OUT_SURFACE_MEM_DEV_INTERNAL) {
            tmp_hst_ptr += (p_dec_info->disp_rect.top * pitch[0]) + (p_dec_info->disp_rect.left * p_dec_info->bytes_per_pixel);
        }
        int img_width = p_dec_info->disp_rect.right - p_dec_info->disp_rect.left;
        int img_height = p_dec_info->disp_rect.top - p_dec_info->disp_rect.bottom;
        int output_stride =  pitch[0];
        if (img_width * p_dec_info->bytes_per_pixel == output_stride && img_height == vpitch) {
            fwrite(tmp_hst_ptr, 1, p_dec_info->output_surface_size_in_bytes, p_dec_info->fp_out);
        } else {
            uint32_t width = img_width * p_dec_info->bytes_per_pixel;
            if (p_dec_info->bit_depth <= 16) {
                for (int i = 0; i < img_height; i++) {
                    fwrite(tmp_hst_ptr, 1, width, p_dec_info->fp_out);
                    tmp_hst_ptr += output_stride;
                }
                // dump chroma
                uint8_t *uv_hst_ptr = hst_ptr + output_stride * vpitch;
                uint32_t chroma_height = static_cast<int>(GetChromaHeightFactor(p_dec_info->surf_format) * img_height);
                if (p_dec_info->mem_type == OUT_SURFACE_MEM_DEV_INTERNAL) {
                    uv_hst_ptr += (num_chroma_planes == 1) ? ((p_dec_info->disp_rect.top >> 1) * output_stride) + (p_dec_info->disp_rect.left * p_dec_info->bytes_per_pixel):
                            ((p_dec_info->disp_rect.top  * output_stride) + (p_dec_info->disp_rect.left  * p_dec_info->bytes_per_pixel));
                }
                for (int i = 0; i < chroma_height; i++) {
                    fwrite(uv_hst_ptr, 1, width, p_dec_info->fp_out);
                    uv_hst_ptr += output_stride;
                }
                if (num_chroma_planes == 2) {
                    uv_hst_ptr = hst_ptr + output_stride * static_cast<int>(vpitch + GetChromaHeightFactor(p_dec_info->surf_format) * vpitch);
                    if (p_dec_info->mem_type == OUT_SURFACE_MEM_DEV_INTERNAL) {
                        uv_hst_ptr += (p_dec_info->disp_rect.top  * output_stride) + (p_dec_info->disp_rect.left  * p_dec_info->bytes_per_pixel);
                    }
                    for (int i = 0; i < chroma_height; i++) {
                        fwrite(uv_hst_ptr, 1, width, p_dec_info->fp_out);
                        uv_hst_ptr += output_stride;
                    }
                }
            }
        }
    }

    if (hst_ptr != nullptr) {
        delete [] hst_ptr;
    }
}

void save_frame_to_file_host(DecoderInfo *p_dec_info, void *frame_mem[], uint32_t *pitch) {

    if (p_dec_info->is_decoder_reconfigured) {
        if (p_dec_info->fp_out) {
            fclose(p_dec_info->fp_out);
            p_dec_info->fp_out = nullptr;
        }
        p_dec_info->is_decoder_reconfigured = false;
    }
    
    if (p_dec_info->fp_out == nullptr && !p_dec_info->output_file_path.empty()) {
        p_dec_info->fp_out = fopen(p_dec_info->output_file_path.c_str(), "wb");
    }

    if (p_dec_info->fp_out) {
        uint8_t *p_src_ptr_y = static_cast<uint8_t *>(frame_mem[0]) + (p_dec_info->disp_rect.top * pitch[0] + p_dec_info->disp_rect.left  * p_dec_info->bytes_per_pixel);
        if (!p_src_ptr_y) {
            std::cerr << "save_frame_to_file_host: Invalid Memory address for src/dst" << std::endl;
            return;
        }
        int img_width = p_dec_info->disp_rect.right - p_dec_info->disp_rect.left;
        int img_height = p_dec_info->disp_rect.top - p_dec_info->disp_rect.bottom;
        int output_stride =  pitch[0];

        uint32_t width = img_width * p_dec_info->bytes_per_pixel;
        if (p_dec_info->bit_depth <= 16) {
            for (int i = 0; i < img_height; i++) {
                fwrite(p_src_ptr_y, 1, width, p_dec_info->fp_out);
                p_src_ptr_y += output_stride;
            }
            // dump chroma
            uint8_t *p_src_ptr_uv = static_cast<uint8_t *>(frame_mem[1]) + ((p_dec_info->disp_rect.top >> 1) * pitch[1] + (p_dec_info->disp_rect.left >> 1) * p_dec_info->bytes_per_pixel);
            uint32_t chroma_height = static_cast<int>(GetChromaHeightFactor(p_dec_info->surf_format) * img_height);
            for (int i = 0; i < chroma_height; i++) {
                fwrite(p_src_ptr_uv, 1, (img_width>>1), p_dec_info->fp_out);
                p_src_ptr_uv += pitch[1];
            }
            if (frame_mem[2] != nullptr) {
                uint8_t *p_src_ptr_v = static_cast<uint8_t *>(frame_mem[2]) + p_dec_info->disp_rect.top * pitch[2] + (p_dec_info->disp_rect.left >> 1) * p_dec_info->bytes_per_pixel;
                for (int i = 0; i < chroma_height; i++) {
                    fwrite(p_src_ptr_v, 1, (img_width>>1), p_dec_info->fp_out);
                    p_src_ptr_v += pitch[2];
                }
            }
        }
    }
}


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

void init() {}

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
    p_dec_info->disp_rect.top = format->display_area.top;
    p_dec_info->disp_rect.bottom = format->display_area.bottom;
    p_dec_info->disp_rect.left = format->display_area.left;
    p_dec_info->disp_rect.right = format->display_area.right;
    CHECK(rocDecReconfigureDecoderHost(p_dec_info->decoder, &reconfig_params));
    p_dec_info->is_decoder_reconfigured = true;
    rocDecVideoChromaFormat video_chroma_format = format->chroma_format;
    int bitdepth_minus_8 = format->bit_depth_luma_minus8;
    if (video_chroma_format == rocDecVideoChromaFormat_420 || rocDecVideoChromaFormat_Monochrome)
        p_dec_info->surf_format = bitdepth_minus_8 ? rocDecVideoSurfaceFormat_P016 : rocDecVideoSurfaceFormat_NV12;
    else if (video_chroma_format == rocDecVideoChromaFormat_444)
        p_dec_info->surf_format = bitdepth_minus_8 ? rocDecVideoSurfaceFormat_YUV444_16Bit : rocDecVideoSurfaceFormat_YUV444;
    else if (video_chroma_format == rocDecVideoChromaFormat_422)
        p_dec_info->surf_format = bitdepth_minus_8 ? rocDecVideoSurfaceFormat_YUV422_16Bit : rocDecVideoSurfaceFormat_YUV422;

    return 1;
}

int ROCDECAPI handle_picture_display_host(void* user_data, void* disp_info) {
    // std::cout << "handle_picture_display is called" << std::endl;
    DecoderInfo *p_dec_info = static_cast<DecoderInfo *>(user_data);
    RocdecParserDispInfo *p_disp_info = static_cast<RocdecParserDispInfo *>(disp_info);
    RocdecProcParams params = {};
    params.progressive_frame = p_disp_info->progressive_frame;
    params.top_field_first = p_disp_info->top_field_first;
    void* frame_mem_ptr[3] = {nullptr};
    uint32_t pitch[3] = {0};
    CHECK(rocDecGetVideoFrameHost(p_dec_info->decoder, p_disp_info->picture_index, frame_mem_ptr, pitch, &params));
    p_dec_info->mem_type = OUT_SURFACE_MEM_HOST;
    if (p_dec_info->dump_decoded_frames) {
        save_frame_to_file_host(p_dec_info, frame_mem_ptr, pitch);
    }

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
    rocDecVideoChromaFormat video_chroma_format = format->chroma_format;
    int bitdepth_minus_8 = format->bit_depth_luma_minus8;
    if (video_chroma_format == rocDecVideoChromaFormat_420 || rocDecVideoChromaFormat_Monochrome)
        p_dec_info->surf_format = bitdepth_minus_8 ? rocDecVideoSurfaceFormat_P016 : rocDecVideoSurfaceFormat_NV12;
    else if (video_chroma_format == rocDecVideoChromaFormat_444)
        p_dec_info->surf_format = bitdepth_minus_8 ? rocDecVideoSurfaceFormat_YUV444_16Bit : rocDecVideoSurfaceFormat_YUV444;
    else if (video_chroma_format == rocDecVideoChromaFormat_422)

    p_dec_info->surf_format = bitdepth_minus_8 ? rocDecVideoSurfaceFormat_YUV422_16Bit : rocDecVideoSurfaceFormat_YUV422;
    p_dec_info->coded_width = format->coded_width;
    p_dec_info->coded_width = format->coded_height;
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
    // check if decoding is complete
    RocdecDecodeStatus dec_status;
    memset(&dec_status, 0, sizeof(dec_status));
    CHECK(rocDecGetDecodeStatus(p_dec_info->decoder, disp_info->picture_index, &dec_status));
    if ((dec_status.decode_status == rocDecodeStatus_Error || dec_status.decode_status == rocDecodeStatus_Error_Concealed)) {
        std::cerr << "Decode Error occurred for picture: " << disp_info->picture_index << std::endl;
        return 0;
    }

    RocdecProcParams params = {};
    params.progressive_frame = disp_info->progressive_frame;
    params.top_field_first = disp_info->top_field_first;
    void* dev_mem_ptr[3] = {0};
    uint32_t pitch[3] = {0};
    CHECK(rocDecGetVideoFrame(p_dec_info->decoder, disp_info->picture_index, dev_mem_ptr, pitch, &params));
    p_dec_info->mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;
    if (p_dec_info->dump_decoded_frames) {
        uint32_t vpitch, num_chroma_planes;
        GetSurfaceStrideInternal(p_dec_info->surf_format, p_dec_info->coded_width, p_dec_info->coded_height, &pitch[0], &vpitch, num_chroma_planes);
        p_dec_info->output_surface_size_in_bytes = pitch[0] * (vpitch + ((vpitch * GetChromaHeightFactor(p_dec_info->surf_format)) * num_chroma_planes));
        save_frame_to_file(p_dec_info, dev_mem_ptr, pitch, vpitch, num_chroma_planes);
    }
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
    std::vector<std::string> input_file_names;
    DecoderInfo dec_info;

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
            dec_info.output_file_path = output_file_path;
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
        ShowHelpAndExit(argv[i]);
    }

    init();
    if (!backend) {
        create_parser(dec_info);
        create_decoder(dec_info);
    } else {
        create_host_decoder(dec_info);
    }
    dec_info.dump_decoded_frames = dump_output_frames;
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
