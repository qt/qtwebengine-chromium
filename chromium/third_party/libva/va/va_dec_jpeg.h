/*
 * Copyright (c) 2007-2012 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL INTEL AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file va_dec_jpeg.h
 * \brief The JPEG decoding API
 *
 * This file contains the \ref api_dec_jpeg "JPEG decoding API".
 */

#ifndef VA_DEC_JPEG_H
#define VA_DEC_JPEG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <va/va.h>

/**
 * \defgroup api_dec_jpeg JPEG decoding API
 *
 * This JPEG decoding API supports Baseline profile only.
 *
 * @{
 */

/**
 * \brief Picture parameter for JPEG decoding.
 *
 * This structure holds information from the frame header, along with
 * definitions from additional segments.
 */
typedef struct _VAPictureParameterBufferJPEGBaseline {
    /** \brief Picture width in pixels. */
    unsigned short      picture_width;
    /** \brief Picture height in pixels. */
    unsigned short      picture_height;

    struct {
        /** \brief Component identifier (Ci). */
        unsigned char   component_id;
        /** \brief Horizontal sampling factor (Hi). */
        unsigned char   h_sampling_factor;
        /** \brief Vertical sampling factor (Vi). */
        unsigned char   v_sampling_factor;
        /* \brief Quantization table selector (Tqi). */
        unsigned char   quantiser_table_selector;
    }                   components[255];
    /** \brief Number of components in frame (Nf). */
    unsigned char       num_components;
} VAPictureParameterBufferJPEGBaseline;

/**
 * \brief Quantization table for JPEG decoding.
 *
 * This structure holds the complete quantization tables. This is an
 * aggregation of all quantization table (DQT) segments maintained by
 * the application. i.e. up to 4 quantization tables are stored in
 * there for baseline profile.
 *
 * The #load_quantization_table array can be used as a hint to notify
 * the VA driver implementation about which table(s) actually changed
 * since the last submission of this buffer.
 */
typedef struct _VAIQMatrixBufferJPEGBaseline {
    /** \brief Specifies which #quantiser_table is valid. */
    unsigned char       load_quantiser_table[4];
    /** \brief Quanziation tables indexed by table identifier (Tqi). */
    unsigned char       quantiser_table[4][64];
} VAIQMatrixBufferJPEGBaseline;

/**
 * \brief Huffman table for JPEG decoding.
 *
 * This structure holds the complete Huffman tables. This is an
 * aggregation of all Huffman table (DHT) segments maintained by the
 * application. i.e. up to 2 Huffman tables are stored in there for
 * baseline profile.
 *
 * The #load_huffman_table array can be used as a hint to notify the
 * VA driver implementation about which table(s) actually changed
 * since the last submission of this buffer.
 */
typedef struct _VAHuffmanTableBufferJPEGBaseline {
    /** \brief Specifies which #huffman_table is valid. */
    unsigned char       load_huffman_table[2];
    /** \brief Huffman tables indexed by table identifier (Th). */
    struct {
        /** @name DC table (up to 12 categories) */
        /**@{*/
        /** \brief Number of Huffman codes of length i + 1 (Li). */
        unsigned char   num_dc_codes[16];
        /** \brief Value associated with each Huffman code (Vij). */
        unsigned char   dc_values[12];
        /**@}*/
        /** @name AC table (2 special codes + up to 16 * 10 codes) */
        /**@{*/
        /** \brief Number of Huffman codes of length i + 1 (Li). */
        unsigned char   num_ac_codes[16];
        /** \brief Value associated with each Huffman code (Vij). */
        unsigned char   ac_values[162];
        /** \brief Padding to 4-byte boundaries. Must be set to zero. */
        unsigned char   pad[2];
        /**@}*/
    }                   huffman_table[2];
} VAHuffmanTableBufferJPEGBaseline;

/**
 * \brief Slice parameter for JPEG decoding.
 *
 * This structure holds information from the scan header, along with
 * definitions from additional segments. The associated slice data
 * buffer holds all entropy coded segments (ECS) in the scan.
 */
typedef struct _VASliceParameterBufferJPEGBaseline {
    /** @name Codec-independent Slice Parameter Buffer base. */
    /**@{*/
    /** \brief Number of bytes in the slice data buffer for this slice. */
    unsigned int        slice_data_size;
    /** \brief The offset to the first byte of the first MCU. */
    unsigned int        slice_data_offset;
    /** \brief Slice data buffer flags. See \c VA_SLICE_DATA_FLAG_xxx. */
    unsigned int        slice_data_flag;
    /**@}*/

    /** \brief Scan horizontal position. */
    unsigned int        slice_horizontal_position;
    /** \brief Scan vertical position. */
    unsigned int        slice_vertical_position;

    struct {
        /** \brief Scan component selector (Csj). */
        unsigned char   component_selector;
        /** \brief DC entropy coding table selector (Tdj). */
        unsigned char   dc_table_selector;
        /** \brief AC entropy coding table selector (Taj). */
        unsigned char   ac_table_selector;
    }                   components[4];
    /** \brief Number of components in scan (Ns). */
    unsigned char       num_components;

    /** \brief Restart interval definition (Ri). */
    unsigned short      restart_interval;
    /** \brief Number of MCUs in a scan. */
    unsigned int        num_mcus;
} VASliceParameterBufferJPEGBaseline;

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* VA_DEC_JPEG_H */
