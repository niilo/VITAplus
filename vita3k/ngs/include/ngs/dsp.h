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
#include <vector>

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

// The filter types of SceNgsParamFilterMode, in the same order.
enum class FilterType : uint32_t {
    Off,
    LowpassResonant,
    HighpassResonant,
    BandpassPeak,
    BandpassZero,
    Notch,
    Peak,
    HighShelf,
    LowShelf,
    LowpassOnePole,
    HighpassOnePole,
    Allpass,
    LowpassResonantNormalized,
};

// Coefficients from the Audio EQ Cookbook (Robert Bristow-Johnson). `q` is
// the filter Q. `gain_db` is used only by the peak and shelf types. Bad input
// values are clamped. Off and unknown types give a filter with no effect.
BiquadCoeffs make_biquad(FilterType type, float frequency, float q, float gain_db, int32_t sample_rate);

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

struct ReverbSettings {
    float room_mb = -10000.0f;
    float room_hf_mb = 0.0f;
    float decay_time_s = 1.0f;
    float decay_hf_ratio = 0.5f;
    float reflections_mb = -10000.0f;
    float reflections_delay_s = 0.02f;
    float reverb_mb = -10000.0f;
    float reverb_delay_s = 0.04f;
    float diffusion_percent = 100.0f;
    float density_percent = 100.0f;
    float hf_reference_hz = 5000.0f;
    float dry_mb = 0.0f;
    uint32_t early_pattern[2] = { 0, 1 }; ///< SceNgsReverbRoom for each output channel
    float early_scalar_percent = 100.0f;
};

// Clamp the settings to the I3DL2 ranges.
ReverbSettings clamp_reverb_settings(const ReverbSettings &settings);

// An I3DL2-style reverb: early reflections from a tapped delay line, and a
// late reverb from a feedback delay network with high-frequency damping.
class Reverb {
public:
    void reset();
    void process(float *samples, uint32_t frames, int32_t sample_rate, const ReverbSettings &settings);

private:
    static constexpr int LINES = 4;

    struct DelayLine {
        std::vector<float> buffer;
        uint32_t position = 0;

        void resize(size_t size);
        void clear();
        float read(uint32_t delay) const;
        void write(float value);
    };

    int32_t sample_rate = 0;
    DelayLine pre_delay;
    DelayLine diffusers[2];
    DelayLine lines[LINES];
    float damping_state[LINES] = {};
    float room_hf_state = 0.0f;

    void allocate(int32_t rate);
};

} // namespace ngs::dsp
