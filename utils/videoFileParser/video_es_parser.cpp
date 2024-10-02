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

#include <string.h>
//#include "../../src/commons.h"
#include "video_es_parser.h"
#include "../../src/parser/hevc_defines.h"
#include "../../src/parser/avc_defines.h"
#include "../../src/parser/av1_defines.h"
#include "../../src/parser/roc_video_parser.h"

RocVideoESParser::RocVideoESParser(const char *input_file_path) {
    p_stream_file_ = fopen(input_file_path, "rb");
    if ( !p_stream_file_) {
        ERR("Failed to open the bitstream file.");
    }
    bs_ring_ = static_cast<uint8_t*>(malloc(BS_RING_SIZE));
    end_of_file_ = false;
    end_of_stream_ = false;
    read_ptr_ = 0;
    write_ptr_ = 0;
    pic_data_.assign(INIT_PIC_DATA_SIZE, 0);
    pic_data_size_ = 0;
    curr_pic_end_ = 0;
    next_pic_start_ = 0;
    num_pictures_ = 0;
    curr_byte_offset_ = read_ptr_;
    num_start_code_ = 0;
    curr_start_code_offset_ = 0;
    next_start_code_offset_ = 0;
    obu_byte_offset_ = 0;
    obu_size_ = 0;
    num_td_obus_ = 0;
    num_temp_units_ = 0;
}

RocVideoESParser::~RocVideoESParser() {
    if (p_stream_file_) {
        fclose(p_stream_file_);
    }
    if (bs_ring_) {
        free(bs_ring_);
    }
}

int RocVideoESParser::GetDataSizeInRB() {
    if (read_ptr_ == write_ptr_) {
        return 0;
    } else if (read_ptr_ < write_ptr_) {
        return write_ptr_ - read_ptr_;
    } else {
        return BS_RING_SIZE - read_ptr_ + write_ptr_;
    }
}

int RocVideoESParser::FetchBitStream()
{
    int free_space;
    int read_size;
    int total_read_size = 0;
    //int offset;

    // A full ring has BS_RING_SIZE - 1 bytes
    free_space = BS_RING_SIZE - 1 - GetDataSizeInRB();
    if (free_space == 0) {
        return 0;
    }
    
    // First fill the ending part of the ring
    if (write_ptr_ >= read_ptr_) {
        int fill_space = BS_RING_SIZE - (write_ptr_ == 0 ? 1 : write_ptr_);
        read_size = fread(&bs_ring_[write_ptr_], 1, fill_space, p_stream_file_);
        if (read_size > 0) {
            write_ptr_ += read_size;
        }
        if (read_size < fill_space) {
            end_of_file_ = true;
        }
        total_read_size += read_size;
        if (end_of_file_) {
            return total_read_size;
        }
    }
    free_space -= read_size;
    if (free_space == 0) {
        return total_read_size;
    }
        
    // Continue filling the beginning part of the ring
    //offset = (write_ptr_ + 1) % BS_RING_SIZE;
    if (read_ptr_ > 0) {
        read_size = fread(&bs_ring_[0], 1, free_space, p_stream_file_);
        if (read_size > 0) {
            write_ptr_ = read_size;
        }
        if (read_size < free_space) {
            end_of_file_ = true;
        }
        total_read_size += read_size;
    }
    return total_read_size;
}

bool RocVideoESParser::GetByte(int offset, uint8_t *data) {
    offset = offset % BS_RING_SIZE;
    if (offset == write_ptr_) {
        if (FetchBitStream() == 0) {
            end_of_stream_ = true;
            return false;
        }
    }
    *data = bs_ring_[offset];
    return true;
}

void RocVideoESParser::SetReadPointer(int value) {
    read_ptr_ = value % BS_RING_SIZE;
    //printf("read_ptr = %d, write_ptr = %d\n", read_ptr_, write_ptr_); // Jefftest
}

bool RocVideoESParser::FindStartCode() {
    uint8_t three_bytes[3];
    //bool start_code_found = false;
    int i;

    //nal_unit_size_ = 0;
    curr_start_code_offset_ = next_start_code_offset_;

    // Search for the next start code
    while (!end_of_stream_) {
        for (i = 0; i < 3; i++) {
            if (GetByte(curr_byte_offset_ + i, three_bytes + i) == false) {
                break;
            }
        }
        if (i < 3) {
            break;
        }

        if (three_bytes[0] == 0 && three_bytes[1] == 0 && three_bytes[2] == 0x01) {
            //curr_start_code_offset_ = next_start_code_offset_;  // save the current start code offset
            //start_code_found = true;
            num_start_code_++;
            //printf("%d start codes found.\n", num_start_code_); // Jefftest
            next_start_code_offset_ = curr_byte_offset_;
            // Move the pointer 3 bytes forward
            curr_byte_offset_ = (curr_byte_offset_ + 3) % BS_RING_SIZE;

            // For the very first NAL unit, search for the next start code (or reach the end of frame)
            if (num_start_code_ == 1) {
                //start_code_found = false;
                curr_start_code_offset_ = next_start_code_offset_;
                continue;
            } else {
                break;
            }
        }
        curr_byte_offset_ = (curr_byte_offset_ + 1) % BS_RING_SIZE;
    }
    printf("FindStartCode(): curr_start_code_offset_ = %d, next_start_code_offset_ = %d\n", curr_start_code_offset_, next_start_code_offset_); // Jefftest
    if (num_start_code_ == 0) {
        // No NAL unit in the bitstream
        return false;
    } else {
        return true;
    }
}

void RocVideoESParser::CopyNalUnitFromRing() {
    int nal_start, nal_end_plus_1;
    int nal_size;
    nal_start = curr_start_code_offset_;
    if (curr_start_code_offset_ != next_start_code_offset_) {
        nal_end_plus_1 = next_start_code_offset_;
    } else {
        nal_end_plus_1 = write_ptr_; // end of stream
    }
    //printf("CopyNalUnitFromRing(): nal_start = %d, nal_end_plus_1 = %d, curr_byte_offset_ = %d\n", nal_start, nal_end_plus_1, curr_byte_offset_); // Jefftest
    if (nal_end_plus_1 >= nal_start) {
        nal_size = nal_end_plus_1 - nal_start;
        if ((pic_data_size_ + nal_size) > pic_data_.size()) {
            pic_data_.resize(pic_data_.size() + nal_size);
        }
        memcpy(&pic_data_[pic_data_size_], &bs_ring_[nal_start], nal_size);
    } else { // wrap around
        nal_size = BS_RING_SIZE - nal_start + nal_end_plus_1;
        if ((pic_data_size_ + nal_size) > pic_data_.size()) {
            pic_data_.resize(pic_data_.size() + nal_size);
        }
        memcpy(&pic_data_[pic_data_size_], &bs_ring_[nal_start], BS_RING_SIZE - nal_start);
        memcpy(&pic_data_[pic_data_size_ + BS_RING_SIZE - nal_start], &bs_ring_[0], nal_end_plus_1);
    }
    printf("CopyNalUnitFromRing(): pic_data_size_ = %d, nal_size = %d\n", pic_data_size_, nal_size); // Jefftest
    pic_data_size_ += nal_size;
    SetReadPointer(nal_end_plus_1);
}

void RocVideoESParser::CheckHevcNalForSlice(int start_code_offset, int *slice_flag, int *first_slice_flag) {
    uint8_t nal_header_byte;
    GetByte(start_code_offset + 3, &nal_header_byte);
    uint8_t nal_unit_type = (nal_header_byte >> 1) & 0x3F;
    switch (nal_unit_type) {
        case NAL_UNIT_CODED_SLICE_TRAIL_R:
        case NAL_UNIT_CODED_SLICE_TRAIL_N:
        case NAL_UNIT_CODED_SLICE_TLA_R:
        case NAL_UNIT_CODED_SLICE_TSA_N:
        case NAL_UNIT_CODED_SLICE_STSA_R:
        case NAL_UNIT_CODED_SLICE_STSA_N:
        case NAL_UNIT_CODED_SLICE_BLA_W_LP:
        case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
        case NAL_UNIT_CODED_SLICE_BLA_N_LP:
        case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
        case NAL_UNIT_CODED_SLICE_IDR_N_LP:
        case NAL_UNIT_CODED_SLICE_CRA_NUT:
        case NAL_UNIT_CODED_SLICE_RADL_N:
        case NAL_UNIT_CODED_SLICE_RADL_R:
        case NAL_UNIT_CODED_SLICE_RASL_N:
        case NAL_UNIT_CODED_SLICE_RASL_R: {
            printf("NAL_UNIT_SLICE ...............\n"); // Jefftest
            *slice_flag = 1;
            uint8_t slice_byte;
            GetByte(start_code_offset + 5, &slice_byte);
            *first_slice_flag = slice_byte >> 7; // first_slice_segment_in_pic_flag
            break;
        }

        default:
            *slice_flag = 0;
            *first_slice_flag = 0;
            printf("NAL type = %d\n", nal_unit_type); // Jefftest
            break;
    }
}

void RocVideoESParser::CheckAvcNalForSlice(int start_code_offset, int *slice_flag, int *first_slice_flag) {
    uint8_t nal_header_byte;
    GetByte(start_code_offset + 3, &nal_header_byte);
    uint8_t nal_unit_type = nal_header_byte & 0x1F;
    switch (nal_unit_type) {
        case kAvcNalTypeSlice_IDR:
        case kAvcNalTypeSlice_Non_IDR:
        case kAvcNalTypeSlice_Data_Partition_A:
        case kAvcNalTypeSlice_Data_Partition_B:
        case kAvcNalTypeSlice_Data_Partition_C: {
            printf("NAL_UNIT_SLICE ...............\n"); // Jefftest
            *slice_flag = 1;
            uint8_t slice_bytes[4]; // 4 bytes is enough to parse the Exp-Golomb codes for first_mb_in_slice
            for (int i = 0; i < 4; i++) {
                GetByte(start_code_offset + 4 + i, &slice_bytes[i]);
            }
            size_t offset = 0;
            int first_mb_in_slice = Parser::ExpGolomb::ReadUe(slice_bytes, offset);
            printf("slice_byte 0 = %x, first_mb_in_slice = %d\n", slice_bytes[0], first_mb_in_slice); // Jefftest
            *first_slice_flag = first_mb_in_slice == 0;
            break;
        }

        default:
            *slice_flag = 0;
            *first_slice_flag = 0;
            printf("NAL type = %d\n", nal_unit_type); // Jefftest
            break;
    }
}

int RocVideoESParser::GetPicDataAvcHevc(uint8_t **p_pic_data, int *pic_size) {
    int slice_nal_flag;
    int first_slice_flag = 0;
    int num_slices = 0;
    
    curr_pic_end_ = 0;
    // Check if we have already got some NAL units for the current picture from processing of the last picture
    if (next_pic_start_ > 0 && next_pic_start_ < pic_data_size_) {
        memcpy(&pic_data_[0], &pic_data_[next_pic_start_], pic_data_size_ - next_pic_start_);
        pic_data_size_ = pic_data_size_ - next_pic_start_;
        curr_pic_end_ = pic_data_size_;
        printf("curr_pic_end_ = %d, pic_data_size_ = %d\n", curr_pic_end_, pic_data_size_); // Jefftest
        next_pic_start_ = 0;
    } else {
        pic_data_size_ = 0;
        next_pic_start_ = 0;
    }

    while (!end_of_stream_) {
        if (!FindStartCode()) {
            ERR("No start code in the bitstream.");
            break;
        }
        CopyNalUnitFromRing();
        //CheckHevcNalForSlice(curr_start_code_offset_, &slice_nal_flag, &first_slice_flag);
        CheckAvcNalForSlice(curr_start_code_offset_, &slice_nal_flag, &first_slice_flag);
        if (slice_nal_flag) {
            num_slices++;
            curr_pic_end_ = pic_data_size_; // update the current picture data end
        }

        if (curr_start_code_offset_ == next_start_code_offset_) {
            break; // end of stream
        } else if (num_slices) {
            //CheckHevcNalForSlice(next_start_code_offset_, &slice_nal_flag, &first_slice_flag); // peek the next NAL
            CheckAvcNalForSlice(next_start_code_offset_, &slice_nal_flag, &first_slice_flag); // peek the next NAL
            if (slice_nal_flag && first_slice_flag) {
                // Between two pictures, we can have non-slice NAL units which are associated with the next picutre
                if (curr_pic_end_ < pic_data_size_) {
                    next_pic_start_ = curr_pic_end_;
                }
                break; // hit the first slice of the next picture
            }
        }
    }

    *p_pic_data = pic_data_.data();
    if (num_slices) {
        num_pictures_++;
        *pic_size = curr_pic_end_;
    } else {
        *pic_size = 0;
    }
    printf("pic_size = %d, num_pictures = %d\n", *pic_size, num_pictures_); // Jefftest
    return 0;
}

bool RocVideoESParser::ReadObuHeaderAndSize(int *obu_type) {
    uint8_t header_byte;
    int obu_extension_flag;

    obu_size_ = 0;
    obu_byte_offset_ = curr_byte_offset_;
    // Parser header
    if (GetByte(curr_byte_offset_, &header_byte) == false) {
        return false;
    }
    *obu_type = (header_byte >> 3) & 0x0F;
    obu_extension_flag = (header_byte >> 2) & 0x01;
    curr_byte_offset_ = (curr_byte_offset_ + 1) % BS_RING_SIZE;
    obu_size_++;
    if (obu_extension_flag) {
        curr_byte_offset_ = (curr_byte_offset_ + 1) % BS_RING_SIZE;
        obu_size_++;
    }
    // Parse size
    int len;
    uint32_t value = 0;
    uint8_t data_byte;
    for (len = 0; len < 8; ++len) {
        if (GetByte(curr_byte_offset_ + len, &data_byte) == false) {
            return false;
        }
        value |= (data_byte & 0x7F) << (len * 7);
        if ((data_byte & 0x80) == 0) {
            ++len;
            break;
        }
    }
    obu_size_ += len + value;
    curr_byte_offset_ = (curr_byte_offset_ + len + value) % BS_RING_SIZE;

    return true;
}

bool RocVideoESParser::CopyObuFromRing() {
    if (obu_size_ > GetDataSizeInRB()) {
        if (FetchBitStream() == 0) {
            end_of_stream_ = true;
            return false;
        }
        if (obu_size_ > GetDataSizeInRB()) {
            return false;
        }
    }
    if ((pic_data_size_ + obu_size_) > pic_data_.size()) {
        pic_data_.resize(pic_data_.size() + obu_size_);
    }
    int obu_end_offset = (obu_byte_offset_ + obu_size_) % BS_RING_SIZE;
    if (obu_end_offset >= obu_byte_offset_) {
        memcpy(&pic_data_[pic_data_size_], &bs_ring_[obu_byte_offset_], obu_size_);
    } else {
        memcpy(&pic_data_[pic_data_size_], &bs_ring_[obu_byte_offset_], BS_RING_SIZE - obu_byte_offset_);
        memcpy(&pic_data_[pic_data_size_ + BS_RING_SIZE - obu_byte_offset_], &bs_ring_[0], obu_end_offset);
    }
    printf("CopyObuFromRing(): pic_data_size_ = %d, obu_size_ = %d\n", pic_data_size_, obu_size_); // Jefftest
    pic_data_size_ += obu_size_;
    SetReadPointer(obu_end_offset);
    return true;
}

int RocVideoESParser::GetPicDataAv1(uint8_t **p_pic_data, int *pic_size) {
    int obu_type;
    pic_data_size_ = 0;

    while (!end_of_stream_) {
        if (!ReadObuHeaderAndSize(&obu_type)) {
            break;
        }
        CopyObuFromRing();
        if (obu_type == kObuTemporalDelimiter) {
            num_td_obus_++;
        if (num_td_obus_ > 1) {
            break;
        }
        }
    }

    *p_pic_data = pic_data_.data();
    *pic_size = pic_data_size_;
    num_temp_units_++;
    return 0;
}

int RocVideoESParser::GetPicData(uint8_t **p_pic_data, int *pic_size) {
    int ret;

    ret = GetPicDataAvcHevc(p_pic_data, pic_size);
    //ret = GetPicDataAv1(p_pic_data, pic_size);
    return ret;
}