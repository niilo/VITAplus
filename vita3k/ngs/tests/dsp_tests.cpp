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

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

constexpr int sample_rate = 48000;
constexpr double pi = 3.14159265358979323846;

std::vector<float> stereo_sine(const float frequency, const uint32_t frames, const float amplitude = 0.5f) {
    std::vector<float> samples(frames * 2);
    for (uint32_t frame = 0; frame < frames; frame++) {
        const float value = amplitude * static_cast<float>(std::sin(2.0 * pi * frequency * frame / sample_rate));
        samples[frame * 2] = value;
        samples[frame * 2 + 1] = value;
    }
    return samples;
}

// Peak of the left channel after the first `skip` frames.
float peak(const std::vector<float> &samples, const uint32_t skip) {
    float result = 0.0f;
    for (size_t i = skip * 2; i < samples.size(); i += 2)
        result = std::max(result, std::fabs(samples[i]));
    return result;
}

} // namespace

TEST(ngs_dsp, identity_biquad_does_not_change_the_signal) {
    std::vector<float> samples = stereo_sine(1000.0f, 256);
    const std::vector<float> original = samples;
    ngs::dsp::BiquadHistory history;
    ngs::dsp::process_biquad(samples.data(), 256, ngs::dsp::BiquadCoeffs{}, history);
    EXPECT_EQ(samples, original);
}

TEST(ngs_dsp, biquad_history_continues_across_calls) {
    const ngs::dsp::BiquadCoeffs coeffs{ 0.2f, 0.3f, 0.1f, -0.4f, 0.1f };
    std::vector<float> whole = stereo_sine(440.0f, 512);
    std::vector<float> split = whole;

    ngs::dsp::BiquadHistory history_whole;
    ngs::dsp::process_biquad(whole.data(), 512, coeffs, history_whole);

    ngs::dsp::BiquadHistory history_split;
    ngs::dsp::process_biquad(split.data(), 256, coeffs, history_split);
    ngs::dsp::process_biquad(split.data() + 512, 256, coeffs, history_split);

    for (size_t i = 0; i < whole.size(); i++)
        ASSERT_FLOAT_EQ(whole[i], split[i]) << "sample " << i;
}

TEST(ngs_dsp, unstable_biquad_is_rejected) {
    EXPECT_TRUE(ngs::dsp::biquad_is_valid(ngs::dsp::BiquadCoeffs{}));
    EXPECT_FALSE(ngs::dsp::biquad_is_valid(ngs::dsp::BiquadCoeffs{ 1.0f, 0.0f, 0.0f, 0.0f, 1.5f }));
    EXPECT_FALSE(ngs::dsp::biquad_is_valid(ngs::dsp::BiquadCoeffs{ 1.0f, 0.0f, 0.0f, 2.5f, 0.5f }));
    EXPECT_FALSE(ngs::dsp::biquad_is_valid(ngs::dsp::BiquadCoeffs{ NAN, 0.0f, 0.0f, 0.0f, 0.0f }));
}

TEST(ngs_dsp, gain_ramp_ends_at_the_end_gain) {
    std::vector<float> samples(64 * 2, 1.0f);
    ngs::dsp::apply_gain_ramp(samples.data(), 64, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(samples[63 * 2], 0.5f);
    EXPECT_FLOAT_EQ(samples[63 * 2 + 1], 0.5f);
    EXPECT_LT(samples[0], 0.01f);
}

namespace {

ngs::dsp::EnvelopeShape fade_in_then_hold() {
    ngs::dsp::EnvelopeShape shape;
    shape.count = 2;
    shape.points[0] = { 100, 0.0f, false };
    shape.points[1] = { 0, 1.0f, false };
    return shape;
}

} // namespace

TEST(ngs_dsp, envelope_moves_in_a_line_between_points) {
    const ngs::dsp::EnvelopeShape shape = fade_in_then_hold();
    ngs::dsp::EnvelopeCursor cursor;
    EXPECT_FLOAT_EQ(ngs::dsp::envelope_gain(shape, cursor), 0.0f);

    ngs::dsp::advance_envelope(shape, cursor, 25.0);
    EXPECT_FLOAT_EQ(ngs::dsp::envelope_gain(shape, cursor), 0.25f);

    ngs::dsp::advance_envelope(shape, cursor, 100.0);
    EXPECT_EQ(cursor.point, 1u);
    EXPECT_FLOAT_EQ(ngs::dsp::envelope_gain(shape, cursor), 1.0f);

    // The last point holds.
    ngs::dsp::advance_envelope(shape, cursor, 10000.0);
    EXPECT_FLOAT_EQ(ngs::dsp::envelope_gain(shape, cursor), 1.0f);
    EXPECT_DOUBLE_EQ(cursor.total_ms, 10125.0);
}

TEST(ngs_dsp, envelope_loop_goes_back_to_loop_start) {
    ngs::dsp::EnvelopeShape shape;
    shape.count = 3;
    shape.points[0] = { 10, 0.0f, false };
    shape.points[1] = { 20, 1.0f, false };
    shape.points[2] = { 20, 0.5f, false };
    shape.loop_start = 1;
    shape.loop_end = 2;

    ngs::dsp::EnvelopeCursor cursor;
    ngs::dsp::advance_envelope(shape, cursor, 10.0 + 20.0 + 10.0);
    // Half way from point 2 (0.5) back to point 1 (1.0).
    EXPECT_EQ(cursor.point, 2u);
    EXPECT_FLOAT_EQ(ngs::dsp::envelope_gain(shape, cursor), 0.75f);

    ngs::dsp::advance_envelope(shape, cursor, 10.0);
    EXPECT_EQ(cursor.point, 1u);
    EXPECT_FLOAT_EQ(ngs::dsp::envelope_gain(shape, cursor), 1.0f);
}

TEST(ngs_dsp, envelope_zero_length_loop_does_not_hang) {
    ngs::dsp::EnvelopeShape shape;
    shape.count = 2;
    shape.points[0] = { 0, 1.0f, false };
    shape.points[1] = { 0, 0.5f, false };
    shape.loop_start = 0;
    shape.loop_end = 1;

    ngs::dsp::EnvelopeCursor cursor;
    ngs::dsp::advance_envelope(shape, cursor, 5.0);
    const float gain = ngs::dsp::envelope_gain(shape, cursor);
    EXPECT_TRUE(gain == 1.0f || gain == 0.5f);
}

TEST(ngs_dsp, compressor_lowers_a_loud_signal) {
    ngs::dsp::CompressorSettings settings;
    settings.ratio = 4.0f;
    settings.threshold_db = -20.0f;
    settings.attack_s = 0.001f;
    settings.release_s = 0.1f;

    const uint32_t frames = sample_rate / 2;
    std::vector<float> samples = stereo_sine(1000.0f, frames, 1.0f);
    ngs::dsp::CompressorState state;
    const ngs::dsp::CompressorLevels levels = ngs::dsp::process_compressor(samples.data(), samples.data(), frames, sample_rate, settings, state);

    // 0 dB input, -20 dB threshold, 4:1 ratio: about -15 dB output (0.18).
    const float output = peak(samples, frames / 2);
    EXPECT_GT(output, 0.12f);
    EXPECT_LT(output, 0.3f);
    EXPECT_FLOAT_EQ(levels.input_peak[0], 1.0f);
    EXPECT_LE(levels.output_peak[0], levels.input_peak[0]);
}

TEST(ngs_dsp, compressor_does_not_change_a_quiet_signal) {
    ngs::dsp::CompressorSettings settings;
    settings.ratio = 4.0f;
    settings.threshold_db = -6.0f;

    std::vector<float> samples = stereo_sine(1000.0f, 4800, 0.1f);
    const std::vector<float> original = samples;
    ngs::dsp::CompressorState state;
    ngs::dsp::process_compressor(samples.data(), samples.data(), 4800, sample_rate, settings, state);

    for (size_t i = 0; i < samples.size(); i++)
        ASSERT_NEAR(samples[i], original[i], 1.0e-6f) << "sample " << i;
}
