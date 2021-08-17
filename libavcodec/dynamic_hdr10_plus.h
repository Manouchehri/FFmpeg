/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_DYNAMIC_HDR10_PLUS_H
#define AVCODEC_DYNAMIC_HDR10_PLUS_H

#include "libavutil/hdr_dynamic_metadata.h"

/**
 * Parse the user data registered ITU-T T.35 to AVbuffer (AVDynamicHDRPlus).
 * @param s A pointer containing the decoded AVDynamicHDRPlus structure.
 * @param data The byte array containing the raw ITU-T T.35 data.
 * @param size Size of the data array in bytes.
 *
 * @return 0 if succeed. Otherwise, returns the appropriate AVERROR.
 */
int ff_parse_itu_t_t35_to_dynamic_hdr10_plus(AVDynamicHDRPlus* s, const uint8_t* data,
					     int size);

/**
 * Parse the user data registered ITU-T T.35 with header to AVDynamicHDRPlus. At first check
 * the header if the provider code is SMPTE-2094-40. Then will parse the data to AVDynamicHDRPlus.
 * @param s A pointer containing the decoded AVDynamicHDRPlus structure.
 * @param data The byte array containing the raw ITU-T T.35 data with header.
 * @param size Size of the data array in bytes.
 *
 * @return 0 if succeed. Otherwise, returns the appropriate AVERROR.
 */
int ff_parse_full_itu_t_t35_to_dynamic_hdr10_plus(AVDynamicHDRPlus* s, const uint8_t* data,
					     int size);

/**
 * Get the size of buffer required to save the encoded bit stream of
 * AVDynamicHDRPlus in the form of the user data registered ITU-T T.35.
 * @param s The AVDynamicHDRPlus structure.
 *
 * @return The size of bit stream required for encoding. 0 if the data is invalid.
 */
int ff_itu_t_t35_buffer_size(const AVDynamicHDRPlus* s);

/**
 * Encode and write AVDynamicHDRPlus to the user data registered ITU-T T.3 with header (containing the provider code).
 * @param s A pointer containing the AVDynamicHDRPlus structure.
 * @param data The byte array containing the raw ITU-T T.35 data with header.
 * @param size The size of the raw ITU-T T.35 data.
 *
 * @return 0 if succeed. Otherwise, returns the appropriate AVERROR.
 */
int ff_write_dynamic_hdr10_plus_to_full_itu_t_t35(const AVDynamicHDRPlus* s, uint8_t** data, size_t* size);

#endif /* AVCODEC_DYNAMIC_HDR10_PLUS_H */
