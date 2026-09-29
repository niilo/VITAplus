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

#pragma once

// Signal processing for the NGS effect modules. The code has no emulator
// dependencies, so the tests in vita3k/ngs/tests can call it directly.
// All buffers are interleaved stereo float.

#include <cstdint>

namespace ngs::dsp {

// y[0] = b0 x[0] + b1 x[-1] + b2 x[-2] - a1 y[-1] - a2 y[-2]
struct BiquadCoeffs {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

struct BiquadHistory {
    float x1[2] = {};
    float x2[2] = {};
    float y1[2] = {};
    float y2[2] = {};
};

// Return false when a coefficient is not finite or the filter is unstable.
bool biquad_is_valid(const BiquadCoeffs &coeffs);

void process_biquad(float *samples, uint32_t frames, const BiquadCoeffs &coeffs, BiquadHistory &history);

// Multiply the signal by a gain that moves in a straight line from `start` to
// `end` across the frames. This prevents clicks when the gain changes.
void apply_gain_ramp(float *samples, uint32_t frames, float start, float end);

static constexpr uint32_t ENVELOPE_MAX_POINTS = 4;

struct EnvelopePoint {
    uint32_t ms_to_next = 0;
    float amplitude = 1.0f;
    bool curved = false;
};

struct EnvelopeShape {
    EnvelopePoint points[ENVELOPE_MAX_POINTS];
    uint32_t count = 0;
    uint32_t loop_start = 0;
    int32_t loop_end = -1; ///< a negative value means no loop

    bool operator==(const EnvelopeShape &other) const;
};

struct EnvelopeCursor {
    uint32_t point = 0;
    double position_ms = 0.0; ///< time since the current point
    double total_ms = 0.0; ///< time since the start
};

// The gain at the cursor. Past the last point, the gain stays at the amplitude
// of the last point.
float envelope_gain(const EnvelopeShape &shape, const EnvelopeCursor &cursor);

// Move the cursor forward by `ms`. If the shape loops, the segment from
// `loop_end` goes back to `loop_start`, with the time of `loop_end`.
void advance_envelope(const EnvelopeShape &shape, EnvelopeCursor &cursor, double ms);

struct CompressorSettings {
    float ratio = 1.0f;
    float threshold_db = 0.0f;
    float attack_s = 0.01f;
    float release_s = 0.1f;
    float makeup_db = 0.0f;
    float knee_db = 0.0f;
    bool rms = false;
    bool stereo_link = false;
};

struct CompressorState {
    float detector[2] = {};
};

struct CompressorLevels {
    float input_peak[2] = {};
    float output_peak[2] = {};
};

// `key` is the signal that the level detector reads. Use `samples` for a
// normal compressor and the second input for a side-chain compressor.
CompressorLevels process_compressor(float *samples, const float *key, uint32_t frames, int32_t sample_rate,
    const CompressorSettings &settings, CompressorState &state);

} // namespace ngs::dsp
