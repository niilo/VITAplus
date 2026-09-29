// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <ngs/dsp.h>

#include <algorithm>
#include <cmath>

namespace ngs::dsp {

bool biquad_is_valid(const BiquadCoeffs &coeffs) {
    if (!std::isfinite(coeffs.b0) || !std::isfinite(coeffs.b1) || !std::isfinite(coeffs.b2)
        || !std::isfinite(coeffs.a1) || !std::isfinite(coeffs.a2))
        return false;

    // Both poles are inside the unit circle when |a2| < 1 and |a1| < 1 + a2.
    return std::fabs(coeffs.a2) < 1.0f && std::fabs(coeffs.a1) < 1.0f + coeffs.a2;
}

void process_biquad(float *samples, const uint32_t frames, const BiquadCoeffs &coeffs, BiquadHistory &history) {
    for (int channel = 0; channel < 2; channel++) {
        float x1 = history.x1[channel];
        float x2 = history.x2[channel];
        float y1 = history.y1[channel];
        float y2 = history.y2[channel];

        for (uint32_t frame = 0; frame < frames; frame++) {
            float &sample = samples[frame * 2 + channel];
            const float x0 = sample;
            float y0 = coeffs.b0 * x0 + coeffs.b1 * x1 + coeffs.b2 * x2 - coeffs.a1 * y1 - coeffs.a2 * y2;
            // Flush denormal values. They are very slow on some CPUs.
            if (std::fabs(y0) < 1.0e-20f)
                y0 = 0.0f;

            x2 = x1;
            x1 = x0;
            y2 = y1;
            y1 = y0;
            sample = y0;
        }

        history.x1[channel] = x1;
        history.x2[channel] = x2;
        history.y1[channel] = y1;
        history.y2[channel] = y2;
    }
}

void apply_gain_ramp(float *samples, const uint32_t frames, const float start, const float end) {
    if (frames == 0)
        return;

    if (start == end) {
        if (start == 1.0f)
            return;
        for (uint32_t i = 0; i < frames * 2; i++)
            samples[i] *= start;
        return;
    }

    const float step = (end - start) / static_cast<float>(frames);
    float gain = start;
    for (uint32_t frame = 0; frame < frames; frame++) {
        gain += step;
        samples[frame * 2] *= gain;
        samples[frame * 2 + 1] *= gain;
    }
}

bool EnvelopeShape::operator==(const EnvelopeShape &other) const {
    if (count != other.count || loop_start != other.loop_start || loop_end != other.loop_end)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (points[i].ms_to_next != other.points[i].ms_to_next || points[i].amplitude != other.points[i].amplitude
            || points[i].curved != other.points[i].curved)
            return false;
    }

    return true;
}

static bool envelope_loops(const EnvelopeShape &shape) {
    return shape.loop_end >= 0 && static_cast<uint32_t>(shape.loop_end) < shape.count
        && shape.loop_start <= static_cast<uint32_t>(shape.loop_end);
}

// The point that the segment from `point` goes to. The segment from
// `loop_end` goes back to `loop_start`. Return false when `point` is the end.
static bool next_envelope_point(const EnvelopeShape &shape, const uint32_t point, uint32_t &next) {
    if (envelope_loops(shape) && point == static_cast<uint32_t>(shape.loop_end)) {
        next = shape.loop_start;
        return true;
    }

    next = point + 1;
    return next < shape.count;
}

float envelope_gain(const EnvelopeShape &shape, const EnvelopeCursor &cursor) {
    if (shape.count == 0)
        return 1.0f;

    const uint32_t point = std::min(cursor.point, shape.count - 1);
    uint32_t next;
    if (!next_envelope_point(shape, point, next))
        return shape.points[point].amplitude;

    const EnvelopePoint &from = shape.points[point];
    const float to = shape.points[next].amplitude;
    if (from.ms_to_next == 0)
        return to;

    double t = std::clamp(cursor.position_ms / from.ms_to_next, 0.0, 1.0);
    if (from.curved)
        t = t * t * (3.0 - 2.0 * t);

    return static_cast<float>(from.amplitude + (to - from.amplitude) * t);
}

void advance_envelope(const EnvelopeShape &shape, EnvelopeCursor &cursor, const double ms) {
    cursor.total_ms += ms;
    if (shape.count == 0)
        return;

    cursor.position_ms += ms;

    // The step limit stops a loop where every segment has zero length.
    for (uint32_t steps = 0; steps < 64; steps++) {
        uint32_t next;
        if (!next_envelope_point(shape, cursor.point, next)) {
            cursor.position_ms = 0.0;
            return;
        }

        const uint32_t segment_ms = shape.points[cursor.point].ms_to_next;
        if (cursor.position_ms < segment_ms)
            return;

        cursor.position_ms -= segment_ms;
        cursor.point = next;
    }

    cursor.position_ms = 0.0;
}

static float linear_to_db(const float linear) {
    return 20.0f * std::log10(std::max(linear, 1.0e-7f));
}

static float db_to_linear(const float db) {
    return std::pow(10.0f, db * 0.05f);
}

static float time_coefficient(const float seconds, const int32_t sample_rate) {
    if (seconds <= 0.0f || sample_rate <= 0)
        return 0.0f;
    return std::exp(-1.0f / (seconds * static_cast<float>(sample_rate)));
}

CompressorLevels process_compressor(float *samples, const float *key, const uint32_t frames, const int32_t sample_rate,
    const CompressorSettings &settings, CompressorState &state) {
    CompressorLevels levels;

    const float attack = time_coefficient(settings.attack_s, sample_rate);
    const float release = time_coefficient(settings.release_s, sample_rate);
    const float slope = 1.0f / settings.ratio - 1.0f;
    const float makeup = db_to_linear(settings.makeup_db);
    const float knee = settings.knee_db;

    // Gain change in dB for a detector level. It is 0 or negative.
    const auto reduction_db = [&](const float level) {
        const float over = linear_to_db(level) - settings.threshold_db;
        if (knee > 0.0f && 2.0f * std::fabs(over) < knee) {
            const float t = over + knee * 0.5f;
            return slope * t * t / (2.0f * knee);
        }
        return over > 0.0f ? slope * over : 0.0f;
    };

    const int detectors = settings.stereo_link ? 1 : 2;
    for (uint32_t frame = 0; frame < frames; frame++) {
        float *sample = &samples[frame * 2];
        const float *key_sample = &key[frame * 2];

        float gain[2];
        for (int d = 0; d < detectors; d++) {
            float input = settings.stereo_link ? std::max(std::fabs(key_sample[0]), std::fabs(key_sample[1])) : std::fabs(key_sample[d]);
            if (settings.rms)
                input *= input;

            float &detector = state.detector[d];
            const float coeff = input > detector ? attack : release;
            detector = coeff * detector + (1.0f - coeff) * input;

            const float level = settings.rms ? std::sqrt(detector) : detector;
            // The gain never goes above 1, so the output is never louder than the input.
            gain[d] = std::min(db_to_linear(reduction_db(level)) * makeup, 1.0f);
        }
        if (settings.stereo_link)
            gain[1] = gain[0];

        for (int channel = 0; channel < 2; channel++) {
            levels.input_peak[channel] = std::max(levels.input_peak[channel], std::fabs(sample[channel]));
            sample[channel] *= gain[channel];
            levels.output_peak[channel] = std::max(levels.output_peak[channel], std::fabs(sample[channel]));
        }
    }

    // Keep the detector finite if the input had a NaN or an infinite value.
    for (float &detector : state.detector) {
        if (!std::isfinite(detector))
            detector = 0.0f;
    }

    return levels;
}

} // namespace ngs::dsp
