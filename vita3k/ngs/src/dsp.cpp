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

static constexpr double pi = 3.14159265358979323846;

static float finite_clamp(const float value, const float fallback, const float min, const float max) {
    return std::isfinite(value) ? std::clamp(value, min, max) : fallback;
}

BiquadCoeffs make_biquad(const FilterType type, const float frequency, const float q, const float gain_db, const int32_t sample_rate) {
    if (sample_rate <= 0)
        return {};

    const double fs = sample_rate;
    const double f0 = finite_clamp(frequency, 1000.0f, 10.0f, static_cast<float>(fs * 0.49));
    const double Q = finite_clamp(q, 0.707f, 0.05f, 40.0f);
    const double gain = finite_clamp(gain_db, 0.0f, -90.0f, 24.0f);

    const double w0 = 2.0 * pi * f0 / fs;
    const double cos_w = std::cos(w0);
    const double sin_w = std::sin(w0);
    const double alpha = sin_w / (2.0 * Q);
    const double A = std::pow(10.0, gain / 40.0);
    const double sqrt_A = std::sqrt(A);

    double b0, b1, b2, a0, a1, a2;
    switch (type) {
    case FilterType::LowpassResonant:
    case FilterType::LowpassResonantNormalized:
        b0 = (1.0 - cos_w) / 2.0;
        b1 = 1.0 - cos_w;
        b2 = b0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cos_w;
        a2 = 1.0 - alpha;
        break;
    case FilterType::HighpassResonant:
        b0 = (1.0 + cos_w) / 2.0;
        b1 = -(1.0 + cos_w);
        b2 = b0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cos_w;
        a2 = 1.0 - alpha;
        break;
    case FilterType::BandpassPeak: // constant skirt gain, peak gain = Q
        b0 = sin_w / 2.0;
        b1 = 0.0;
        b2 = -sin_w / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cos_w;
        a2 = 1.0 - alpha;
        break;
    case FilterType::BandpassZero: // constant 0 dB peak gain
        b0 = alpha;
        b1 = 0.0;
        b2 = -alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cos_w;
        a2 = 1.0 - alpha;
        break;
    case FilterType::Notch:
        b0 = 1.0;
        b1 = -2.0 * cos_w;
        b2 = 1.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cos_w;
        a2 = 1.0 - alpha;
        break;
    case FilterType::Allpass:
        b0 = 1.0 - alpha;
        b1 = -2.0 * cos_w;
        b2 = 1.0 + alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cos_w;
        a2 = 1.0 - alpha;
        break;
    case FilterType::Peak:
        b0 = 1.0 + alpha * A;
        b1 = -2.0 * cos_w;
        b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A;
        a1 = -2.0 * cos_w;
        a2 = 1.0 - alpha / A;
        break;
    case FilterType::LowShelf:
        b0 = A * ((A + 1.0) - (A - 1.0) * cos_w + 2.0 * sqrt_A * alpha);
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w);
        b2 = A * ((A + 1.0) - (A - 1.0) * cos_w - 2.0 * sqrt_A * alpha);
        a0 = (A + 1.0) + (A - 1.0) * cos_w + 2.0 * sqrt_A * alpha;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_w);
        a2 = (A + 1.0) + (A - 1.0) * cos_w - 2.0 * sqrt_A * alpha;
        break;
    case FilterType::HighShelf:
        b0 = A * ((A + 1.0) + (A - 1.0) * cos_w + 2.0 * sqrt_A * alpha);
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w);
        b2 = A * ((A + 1.0) + (A - 1.0) * cos_w - 2.0 * sqrt_A * alpha);
        a0 = (A + 1.0) - (A - 1.0) * cos_w + 2.0 * sqrt_A * alpha;
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos_w);
        a2 = (A + 1.0) - (A - 1.0) * cos_w - 2.0 * sqrt_A * alpha;
        break;
    case FilterType::LowpassOnePole:
    case FilterType::HighpassOnePole: {
        // First order, from the bilinear transform.
        const double k = std::tan(w0 / 2.0);
        const double norm = 1.0 / (1.0 + k);
        if (type == FilterType::LowpassOnePole) {
            b0 = k * norm;
            b1 = k * norm;
        } else {
            b0 = norm;
            b1 = -norm;
        }
        b2 = 0.0;
        a0 = 1.0;
        a1 = (k - 1.0) * norm;
        a2 = 0.0;
        break;
    }
    default:
        return {};
    }

    // The normalized type keeps the resonance peak at 0 dB or lower. The peak
    // of the cookbook low-pass is about Q / sqrt(1 - 1 / (4 Q^2)) for Q > 0.707.
    double scale = 1.0;
    if (type == FilterType::LowpassResonantNormalized && Q > 0.7072)
        scale = std::sqrt(1.0 - 1.0 / (4.0 * Q * Q)) / Q;

    BiquadCoeffs coeffs;
    coeffs.b0 = static_cast<float>(scale * b0 / a0);
    coeffs.b1 = static_cast<float>(scale * b1 / a0);
    coeffs.b2 = static_cast<float>(scale * b2 / a0);
    coeffs.a1 = static_cast<float>(a1 / a0);
    coeffs.a2 = static_cast<float>(a2 / a0);
    return biquad_is_valid(coeffs) ? coeffs : BiquadCoeffs{};
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

// Reverb

static float mb_to_linear(const float mb) {
    // -10000 mB is the I3DL2 value for silence.
    if (mb <= -10000.0f)
        return 0.0f;
    return std::pow(10.0f, mb / 2000.0f);
}

ReverbSettings clamp_reverb_settings(const ReverbSettings &in) {
    ReverbSettings out = in;
    out.room_mb = finite_clamp(in.room_mb, -10000.0f, -10000.0f, 0.0f);
    out.room_hf_mb = finite_clamp(in.room_hf_mb, 0.0f, -10000.0f, 0.0f);
    out.decay_time_s = finite_clamp(in.decay_time_s, 1.0f, 0.1f, 20.0f);
    out.decay_hf_ratio = finite_clamp(in.decay_hf_ratio, 0.5f, 0.1f, 2.0f);
    out.reflections_mb = finite_clamp(in.reflections_mb, -10000.0f, -10000.0f, 1000.0f);
    out.reflections_delay_s = finite_clamp(in.reflections_delay_s, 0.0f, 0.0f, 0.3f);
    out.reverb_mb = finite_clamp(in.reverb_mb, -10000.0f, -10000.0f, 2000.0f);
    out.reverb_delay_s = finite_clamp(in.reverb_delay_s, 0.0f, 0.0f, 0.1f);
    out.diffusion_percent = finite_clamp(in.diffusion_percent, 100.0f, 0.0f, 100.0f);
    out.density_percent = finite_clamp(in.density_percent, 100.0f, 0.0f, 100.0f);
    out.hf_reference_hz = finite_clamp(in.hf_reference_hz, 5000.0f, 20.0f, 20000.0f);
    out.dry_mb = finite_clamp(in.dry_mb, 0.0f, -10000.0f, 0.0f);
    out.early_scalar_percent = finite_clamp(in.early_scalar_percent, 100.0f, 10.0f, 100.0f);
    for (uint32_t &pattern : out.early_pattern)
        pattern = std::min<uint32_t>(pattern, 5);
    return out;
}

void Reverb::DelayLine::resize(const size_t size) {
    buffer.assign(size, 0.0f);
    position = 0;
}

void Reverb::DelayLine::clear() {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
}

float Reverb::DelayLine::read(const uint32_t delay) const {
    const size_t size = buffer.size();
    return buffer[(position + size - std::min<size_t>(delay, size - 1)) % size];
}

void Reverb::DelayLine::write(const float value) {
    position = static_cast<uint32_t>((position + 1) % buffer.size());
    buffer[position] = value;
}

// Late reverb delay line lengths in ms, at 100 % density. The values have no
// common factor, so the echoes do not line up.
static constexpr float line_ms[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
static constexpr float diffuser_ms[2] = { 4.77f, 3.59f };

// Early reflection taps in ms for the left and right patterns of room 3 (the
// largest room). Rooms 1 and 2 use shorter times. The real NGS patterns are
// not known.
static constexpr int EARLY_TAPS = 6;
static constexpr float early_tap_ms[2][EARLY_TAPS] = {
    { 0.0f, 7.1f, 11.3f, 17.9f, 23.3f, 29.7f },
    { 0.0f, 5.3f, 13.1f, 19.7f, 25.1f, 31.3f },
};
static constexpr float early_tap_gain[EARLY_TAPS] = { 0.55f, 0.48f, 0.41f, 0.34f, 0.27f, 0.2f };
static constexpr float room_scale[3] = { 0.5f, 0.75f, 1.0f };

void Reverb::allocate(const int32_t rate) {
    sample_rate = rate;
    const auto samples = [rate](const float ms) { return static_cast<size_t>(ms * 0.001f * rate) + 2; };

    // Pre-delay: the reflections delay, the reverb delay and the longest early tap.
    pre_delay.resize(samples(300.0f + 100.0f + 40.0f));
    for (int i = 0; i < 2; i++)
        diffusers[i].resize(samples(diffuser_ms[i]));
    for (int i = 0; i < LINES; i++)
        lines[i].resize(samples(line_ms[i]));
    reset();
}

void Reverb::reset() {
    pre_delay.clear();
    for (DelayLine &diffuser : diffusers)
        diffuser.clear();
    for (DelayLine &line : lines)
        line.clear();
    std::fill(std::begin(damping_state), std::end(damping_state), 0.0f);
    room_hf_state = 0.0f;
}

// The pole of a one-pole low-pass y = (1 - a) x + a y[-1] that has gain `r`
// (0 < r < 1) at the angular frequency `w`. The gain at 0 Hz is 1.
static float one_pole_for_gain(const float r, const float w) {
    if (r >= 0.9999f)
        return 0.0f;
    const double r2 = static_cast<double>(r) * r;
    const double k = r2 - 1.0;
    const double b = 1.0 - r2 * std::cos(w);
    const double disc = std::max(b * b - k * k, 0.0);
    return static_cast<float>(std::clamp((b - std::sqrt(disc)) / -k, 0.0, 0.999));
}

void Reverb::process(float *samples, const uint32_t frames, const int32_t rate, const ReverbSettings &in) {
    if (rate <= 0 || frames == 0)
        return;
    if (rate != sample_rate)
        allocate(rate);

    const ReverbSettings settings = clamp_reverb_settings(in);
    const float fs = static_cast<float>(rate);

    const float room = mb_to_linear(settings.room_mb);
    const float dry = mb_to_linear(settings.dry_mb);
    const float wet = finite_clamp(in.wet_scale, 1.0f, 0.0f, 4.0f);
    const float reflections = wet * room * mb_to_linear(settings.reflections_mb);
    const float late = wet * room * mb_to_linear(settings.reverb_mb);
    const float w_hf = static_cast<float>(2.0 * pi * settings.hf_reference_hz / fs);

    // Room HF: a low-pass on the wet input with the given gain at the HF reference.
    const float room_hf_pole = one_pole_for_gain(mb_to_linear(settings.room_hf_mb), w_hf);

    // Early reflection taps for each output channel.
    uint32_t tap_delay[2][EARLY_TAPS];
    float tap_gain[2][EARLY_TAPS];
    const uint32_t reflections_delay = static_cast<uint32_t>(settings.reflections_delay_s * fs);
    for (int channel = 0; channel < 2; channel++) {
        const uint32_t pattern = settings.early_pattern[channel];
        const float scale = room_scale[pattern / 2] * settings.early_scalar_percent / 100.0f;
        for (int tap = 0; tap < EARLY_TAPS; tap++) {
            tap_delay[channel][tap] = reflections_delay + static_cast<uint32_t>(early_tap_ms[pattern % 2][tap] * scale * 0.001f * fs);
            tap_gain[channel][tap] = early_tap_gain[tap] * reflections;
        }
    }
    const uint32_t late_delay = reflections_delay + static_cast<uint32_t>(settings.reverb_delay_s * fs);

    // Density sets the line lengths (50 % to 100 % of the base lengths).
    // Decay time sets the feedback gain of each line (-60 dB after decay_time_s).
    const float length_scale = 0.5f + settings.density_percent / 200.0f;
    uint32_t line_delay[LINES];
    float line_gain[LINES];
    float line_pole[LINES];
    float line_norm[LINES];
    for (int i = 0; i < LINES; i++) {
        line_delay[i] = std::max<uint32_t>(1, static_cast<uint32_t>(line_ms[i] * length_scale * 0.001f * fs));
        const float seconds = static_cast<float>(line_delay[i]) / fs;
        line_gain[i] = std::pow(10.0f, -3.0f * seconds / settings.decay_time_s);
        const float hf_gain = std::pow(10.0f, -3.0f * seconds / (settings.decay_time_s * settings.decay_hf_ratio));
        line_pole[i] = one_pole_for_gain(std::min(hf_gain / line_gain[i], 1.0f), w_hf);
        // Scale the input so that the tail has about the energy of the input.
        line_norm[i] = std::sqrt(1.0f - line_gain[i] * line_gain[i]);
    }

    const float diffusion = 0.7f * settings.diffusion_percent / 100.0f;
    uint32_t diffuser_delay[2];
    for (int i = 0; i < 2; i++)
        diffuser_delay[i] = std::max<uint32_t>(1, static_cast<uint32_t>(diffuser_ms[i] * 0.001f * fs));

    for (uint32_t frame = 0; frame < frames; frame++) {
        float *sample = &samples[frame * 2];
        const float input_left = sample[0];
        const float input_right = sample[1];

        float mono = 0.5f * (input_left + input_right);
        room_hf_state = (1.0f - room_hf_pole) * mono + room_hf_pole * room_hf_state;
        pre_delay.write(room_hf_state);

        float early[2] = {};
        for (int channel = 0; channel < 2; channel++) {
            for (int tap = 0; tap < EARLY_TAPS; tap++)
                early[channel] += tap_gain[channel][tap] * pre_delay.read(tap_delay[channel][tap]);
        }

        // Two all-pass filters spread the input in time.
        float diffused = pre_delay.read(late_delay);
        for (int i = 0; i < 2; i++) {
            const float delayed = diffusers[i].read(diffuser_delay[i] - 1);
            const float value = diffused + diffusion * delayed;
            diffusers[i].write(value);
            diffused = delayed - diffusion * value;
        }

        // Feedback delay network: 4 lines mixed by a Hadamard matrix.
        float out[LINES];
        for (int i = 0; i < LINES; i++) {
            const float value = lines[i].read(line_delay[i] - 1);
            damping_state[i] = (1.0f - line_pole[i]) * value + line_pole[i] * damping_state[i];
            out[i] = damping_state[i] * line_gain[i];
        }
        const float mixed[LINES] = {
            0.5f * (out[0] + out[1] + out[2] + out[3]),
            0.5f * (out[0] - out[1] + out[2] - out[3]),
            0.5f * (out[0] + out[1] - out[2] - out[3]),
            0.5f * (out[0] - out[1] - out[2] + out[3]),
        };
        for (int i = 0; i < LINES; i++) {
            float value = mixed[i] + line_norm[i] * diffused;
            if (std::fabs(value) < 1.0e-20f || !std::isfinite(value))
                value = 0.0f;
            lines[i].write(value);
        }

        const float late_left = 0.5f * (out[0] + out[2]);
        const float late_right = 0.5f * (out[1] + out[3]);
        sample[0] = dry * input_left + early[0] + late * late_left;
        sample[1] = dry * input_right + early[1] + late * late_right;
    }
}

} // namespace ngs::dsp
