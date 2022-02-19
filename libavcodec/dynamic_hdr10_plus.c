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

#include "dynamic_hdr10_plus.h"
#include "get_bits.h"
#include "put_bits.h"

static const uint8_t usa_country_code = 0xB5;
static const uint16_t smpte_provider_code = 0x003C;
static const uint16_t smpte2094_40_provider_oriented_code = 0x0001;
static const uint16_t smpte2094_40_application_identifier = 0x04;

static const int64_t luminance_den = 1;
static const int32_t peak_luminance_den = 15;
static const int64_t rgb_den = 100000;
static const int32_t fraction_pixel_den = 1000;
static const int32_t knee_point_den = 4095;
static const int32_t bezier_anchor_den = 1023;
static const int32_t saturation_weight_den = 8;

int ff_parse_itu_t_t35_to_dynamic_hdr10_plus(AVDynamicHDRPlus* s, const uint8_t* data,
					     int size)
{
    GetBitContext gbc, *gb = &gbc;
    int ret;

    if (!s)
        return AVERROR(ENOMEM);

    ret = init_get_bits8(gb, data, size);
    if (ret < 0)
        return ret;

    if (get_bits_left(gb) < 8)
        return AVERROR_INVALIDDATA;
     s->application_version = get_bits(gb, 8);

    if (get_bits_left(gb) < 2)
        return AVERROR_INVALIDDATA;
    s->num_windows = get_bits(gb, 2);

    if (s->num_windows < 1 || s->num_windows > 3) {
        return AVERROR_INVALIDDATA;
    }

    if (get_bits_left(gb) < ((19 * 8 + 1) * (s->num_windows - 1)))
        return AVERROR_INVALIDDATA;

    for (int w = 1; w < s->num_windows; w++) {
        // The corners are set to absolute coordinates here. They should be
        // converted to the relative coordinates (in [0, 1]) in the decoder.
        AVHDRPlusColorTransformParams* params = &s->params[w];
        params->window_upper_left_corner_x = (AVRational) { get_bits(gb, 16), 1 };
        params->window_upper_left_corner_y = (AVRational) { get_bits(gb, 16), 1 };
        params->window_lower_right_corner_x = (AVRational) { get_bits(gb, 16), 1 };
        params->window_lower_right_corner_y = (AVRational) { get_bits(gb, 16), 1 };

        params->center_of_ellipse_x = get_bits(gb, 16);
        params->center_of_ellipse_y = get_bits(gb, 16);
        params->rotation_angle = get_bits(gb, 8);
        params->semimajor_axis_internal_ellipse = get_bits(gb, 16);
        params->semimajor_axis_external_ellipse = get_bits(gb, 16);
        params->semiminor_axis_external_ellipse = get_bits(gb, 16);
        params->overlap_process_option = get_bits1(gb);
    }

    if (get_bits_left(gb) < 28)
        return AVERROR(EINVAL);

    s->targeted_system_display_maximum_luminance = (AVRational) { get_bits_long(gb, 27), luminance_den };
    s->targeted_system_display_actual_peak_luminance_flag = get_bits1(gb);

    if (s->targeted_system_display_actual_peak_luminance_flag) {
        int rows, cols;
        if (get_bits_left(gb) < 10)
            return AVERROR(EINVAL);
        rows = get_bits(gb, 5);
        cols = get_bits(gb, 5);
        if (((rows < 2) || (rows > 25)) || ((cols < 2) || (cols > 25))) {
            return AVERROR_INVALIDDATA;
        }
        s->num_rows_targeted_system_display_actual_peak_luminance = rows;
        s->num_cols_targeted_system_display_actual_peak_luminance = cols;

        if (get_bits_left(gb) < (rows * cols * 4))
            return AVERROR(EINVAL);

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                s->targeted_system_display_actual_peak_luminance[i][j] = (AVRational) { get_bits(gb, 4), peak_luminance_den };
            }
        }
    }
    for (int w = 0; w < s->num_windows; w++) {
        AVHDRPlusColorTransformParams* params = &s->params[w];
        if (get_bits_left(gb) < (3 * 17 + 17 + 4))
            return AVERROR(EINVAL);

        for (int i = 0; i < 3; i++) {
            params->maxscl[i] = (AVRational) { get_bits(gb, 17), rgb_den };
        }
        params->average_maxrgb = (AVRational) { get_bits(gb, 17), rgb_den };
        params->num_distribution_maxrgb_percentiles = get_bits(gb, 4);

        if (get_bits_left(gb) < (params->num_distribution_maxrgb_percentiles * 24))
            return AVERROR(EINVAL);

        for (int i = 0; i < params->num_distribution_maxrgb_percentiles; i++) {
            params->distribution_maxrgb[i].percentage = get_bits(gb, 7);
            params->distribution_maxrgb[i].percentile = (AVRational) { get_bits(gb, 17), rgb_den };
        }

        if (get_bits_left(gb) < 10)
            return AVERROR(EINVAL);

        params->fraction_bright_pixels = (AVRational) { get_bits(gb, 10), fraction_pixel_den };
    }
    if (get_bits_left(gb) < 1)
        return AVERROR(EINVAL);
    s->mastering_display_actual_peak_luminance_flag = get_bits1(gb);
    if (s->mastering_display_actual_peak_luminance_flag) {
        int rows, cols;
        if (get_bits_left(gb) < 10)
            return AVERROR(EINVAL);
        rows = get_bits(gb, 5);
        cols = get_bits(gb, 5);
        if (((rows < 2) || (rows > 25)) || ((cols < 2) || (cols > 25))) {
            return AVERROR_INVALIDDATA;
        }
        s->num_rows_mastering_display_actual_peak_luminance = rows;
        s->num_cols_mastering_display_actual_peak_luminance = cols;

        if (get_bits_left(gb) < (rows * cols * 4))
            return AVERROR(EINVAL);

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                s->mastering_display_actual_peak_luminance[i][j] = (AVRational) { get_bits(gb, 4), peak_luminance_den };
            }
        }
    }

    for (int w = 0; w < s->num_windows; w++) {
        AVHDRPlusColorTransformParams* params = &s->params[w];
        if (get_bits_left(gb) < 1)
            return AVERROR(EINVAL);

        params->tone_mapping_flag = get_bits1(gb);
        if (params->tone_mapping_flag) {
            if (get_bits_left(gb) < 28)
                return AVERROR(EINVAL);

            params->knee_point_x = (AVRational) { get_bits(gb, 12), knee_point_den };
            params->knee_point_y = (AVRational) { get_bits(gb, 12), knee_point_den };
            params->num_bezier_curve_anchors = get_bits(gb, 4);

            if (get_bits_left(gb) < (params->num_bezier_curve_anchors * 10))
                return AVERROR(EINVAL);

            for (int i = 0; i < params->num_bezier_curve_anchors; i++) {
                params->bezier_curve_anchors[i] = (AVRational) { get_bits(gb, 10), bezier_anchor_den };
            }
        }

        if (get_bits_left(gb) < 1)
            return AVERROR(EINVAL);
        params->color_saturation_mapping_flag = get_bits1(gb);
        if (params->color_saturation_mapping_flag) {
            if (get_bits_left(gb) < 6)
                return AVERROR(EINVAL);
            params->color_saturation_weight = (AVRational) { get_bits(gb, 6), saturation_weight_den };
        }
    }

    return 0;
}

extern const int ff_parse_full_itu_t_t35_to_dynamic_hdr10_plus(AVDynamicHDRPlus* s, const uint8_t* data,
					     int size)
{
    uint8_t country_code;
    uint16_t provider_code;
    uint16_t provider_oriented_code;
    uint8_t application_identifier;
    GetBitContext gbc, *gb = &gbc;
    int ret, offset;

    if (!s)
        return AVERROR(ENOMEM);

    if (size < 7)
        return AVERROR_INVALIDDATA;

    ret = init_get_bits8(gb, data, size);
    if (ret < 0)
        return ret;

    country_code = get_bits(gb, 8);
    provider_code = get_bits(gb, 16);

    if (country_code != usa_country_code ||
        provider_code != smpte_provider_code)
        return AVERROR_INVALIDDATA;

    // A/341 Amendment – 2094-40
    provider_oriented_code = get_bits(gb, 16);
    application_identifier = get_bits(gb, 8);
    if (provider_oriented_code != smpte2094_40_provider_oriented_code ||
        application_identifier != smpte2094_40_application_identifier)
        return AVERROR_INVALIDDATA;

    offset = get_bits_count(gb) / 8;

    return ff_parse_itu_t_t35_to_dynamic_hdr10_plus(s, gb->buffer + offset, size - offset);
}

int ff_itu_t_t35_buffer_size(const AVDynamicHDRPlus* s)
{
    int bit_count = 0;
    int w, size;

    if (!s)
        return 0;

    // 7 bytes for country code, provider code, and user identifier.
    bit_count += 56;

    if (s->num_windows < 1 || s->num_windows > 3)
        return 0;
    // Count bits for window params.
    bit_count += 2 + ((19 * 8 + 1) * (s->num_windows - 1));

    bit_count += 28;
    if (s->targeted_system_display_actual_peak_luminance_flag) {
        int rows, cols;
        rows = s->num_rows_targeted_system_display_actual_peak_luminance;
        cols = s->num_cols_targeted_system_display_actual_peak_luminance;
        if (((rows < 2) || (rows > 25)) || ((cols < 2) || (cols > 25)))
            return 0;

        bit_count += (10 + rows * cols * 4);
    }
    for (w = 0; w < s->num_windows; w++) {
        bit_count += (3 * 17 + 17 + 4 + 10) + (s->params[w].num_distribution_maxrgb_percentiles * 24);
    }
    bit_count++;

    if (s->mastering_display_actual_peak_luminance_flag) {
        int rows, cols;
        rows = s->num_rows_mastering_display_actual_peak_luminance;
        cols = s->num_cols_mastering_display_actual_peak_luminance;
        if (((rows < 2) || (rows > 25)) || ((cols < 2) || (cols > 25)))
            return 0;

        bit_count += (10 + rows * cols * 4);
    }

    for (w = 0; w < s->num_windows; w++) {
        bit_count++;
        if (s->params[w].tone_mapping_flag)
            bit_count += (28 + s->params[w].num_bezier_curve_anchors * 10);

        bit_count++;
        if (s->params[w].color_saturation_mapping_flag)
            bit_count += 6;
    }
    size = bit_count / 8;
    if (bit_count % 8 != 0)
        size++;
    return size;
}

extern const int ff_write_dynamic_hdr10_plus_to_full_itu_t_t35(const AVDynamicHDRPlus* s, uint8_t** data, size_t* size)
{
    int w, i, j;
    PutBitContext pbc, *pb = &pbc;

    if (!s || !size)
        return AVERROR(EINVAL);

    *size = ff_itu_t_t35_buffer_size(s);
    if (*size <= 0)
        return AVERROR(EINVAL);
    *data = av_mallocz(*size);
    init_put_bits(pb, *data, *size);
    if (put_bits_left(pb) < *size) {
        av_freep(data);
        return AVERROR(EINVAL);
    }
    put_bits(pb, 8, usa_country_code);

    put_bits(pb, 16, smpte_provider_code);
    put_bits(pb, 16, smpte2094_40_provider_oriented_code);
    put_bits(pb, 8, smpte2094_40_application_identifier);
    put_bits(pb, 8, s->application_version);

    put_bits(pb, 2, s->num_windows);

    for (w = 1; w < s->num_windows; w++) {
        put_bits(pb, 16, s->params[w].window_upper_left_corner_x.num / s->params[w].window_upper_left_corner_x.den);
        put_bits(pb, 16, s->params[w].window_upper_left_corner_y.num / s->params[w].window_upper_left_corner_y.den);
        put_bits(pb, 16, s->params[w].window_lower_right_corner_x.num / s->params[w].window_lower_right_corner_x.den);
        put_bits(pb, 16, s->params[w].window_lower_right_corner_y.num / s->params[w].window_lower_right_corner_y.den);
        put_bits(pb, 16, s->params[w].center_of_ellipse_x);
        put_bits(pb, 16, s->params[w].center_of_ellipse_y);
        put_bits(pb, 8, s->params[w].rotation_angle);
        put_bits(pb, 16, s->params[w].semimajor_axis_internal_ellipse);
        put_bits(pb, 16, s->params[w].semimajor_axis_external_ellipse);
        put_bits(pb, 16, s->params[w].semiminor_axis_external_ellipse);
        put_bits(pb, 1, s->params[w].overlap_process_option);
    }
    put_bits(pb, 27,
             s->targeted_system_display_maximum_luminance.num * luminance_den / s->targeted_system_display_maximum_luminance.den);
    put_bits(pb, 1, s->targeted_system_display_actual_peak_luminance_flag);
    if (s->targeted_system_display_actual_peak_luminance_flag) {
        int rows, cols;
        rows = s->num_rows_targeted_system_display_actual_peak_luminance;
        cols = s->num_cols_targeted_system_display_actual_peak_luminance;
        put_bits(pb, 5, rows);
        put_bits(pb, 5, cols);
        for (i = 0; i < rows; i++) {
            for (j = 0; j < cols; j++) {
                put_bits(
                    pb, 4,
                    s->targeted_system_display_actual_peak_luminance[i][j].num * peak_luminance_den / s->targeted_system_display_actual_peak_luminance[i][j].den);
            }
        }
    }
    for (w = 0; w < s->num_windows; w++) {
        for (i = 0; i < 3; i++) {
            put_bits(pb, 17,
                     s->params[w].maxscl[i].num * rgb_den / s->params[w].maxscl[i].den);
        }
        put_bits(pb, 17,
                 s->params[w].average_maxrgb.num * rgb_den / s->params[w].average_maxrgb.den);
        put_bits(pb, 4, s->params[w].num_distribution_maxrgb_percentiles);

        for (i = 0; i < s->params[w].num_distribution_maxrgb_percentiles; i++) {
            put_bits(pb, 7, s->params[w].distribution_maxrgb[i].percentage);
            put_bits(pb, 17,
                     s->params[w].distribution_maxrgb[i].percentile.num * rgb_den / s->params[w].distribution_maxrgb[i].percentile.den);
        }
        put_bits(pb, 10,
                 s->params[w].fraction_bright_pixels.num * fraction_pixel_den / s->params[w].fraction_bright_pixels.den);
    }
    put_bits(pb, 1, s->mastering_display_actual_peak_luminance_flag);
    if (s->mastering_display_actual_peak_luminance_flag) {
        int rows, cols;
        rows = s->num_rows_mastering_display_actual_peak_luminance;
        cols = s->num_cols_mastering_display_actual_peak_luminance;
        put_bits(pb, 5, rows);
        put_bits(pb, 5, cols);
        for (i = 0; i < rows; i++) {
            for (j = 0; j < cols; j++) {
                put_bits(
                    pb, 4,
                    s->mastering_display_actual_peak_luminance[i][j].num * peak_luminance_den / s->mastering_display_actual_peak_luminance[i][j].den);
            }
        }
    }

    for (w = 0; w < s->num_windows; w++) {
        put_bits(pb, 1, s->params[w].tone_mapping_flag);
        if (s->params[w].tone_mapping_flag) {
            put_bits(pb, 12,
                     s->params[w].knee_point_x.num * knee_point_den / s->params[w].knee_point_x.den);
            put_bits(pb, 12,
                     s->params[w].knee_point_y.num * knee_point_den / s->params[w].knee_point_y.den);
            put_bits(pb, 4, s->params[w].num_bezier_curve_anchors);
            for (i = 0; i < s->params[w].num_bezier_curve_anchors; i++) {
                put_bits(pb, 10,
                         s->params[w].bezier_curve_anchors[i].num * bezier_anchor_den / s->params[w].bezier_curve_anchors[i].den);
            }
        }
        put_bits(pb, 1, s->params[w].color_saturation_mapping_flag);
        if (s->params[w].color_saturation_mapping_flag)
            put_bits(pb, 6,
                     s->params[w].color_saturation_weight.num * saturation_weight_den / s->params[w].color_saturation_weight.den);
    }
    flush_put_bits(pb);
    return 0;
}
