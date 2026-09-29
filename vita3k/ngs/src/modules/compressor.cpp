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

#include <ngs/modules/compressor.h>

#include <algorithm>
#include <cmath>

namespace ngs {

// No public source gives the units of the parameters. This code uses the
// same guesses as Vita3K-Plus, which were checked against games:
// - threshold, makeup gain and soft knee are in dB;
// - attack and release are in seconds, and a value above 10 is in ms;
// - a ratio between 0 and 1 is the inverse of the ratio.

static float finite_or(const float value, const float fallback) {
    return std::isfinite(value) ? value : fallback;
}

static float read_seconds(const float value, const float min, const float max) {
    float seconds = std::fabs(finite_or(value, min));
    if (seconds > 10.0f)
        seconds *= 0.001f;
    return std::clamp(seconds, min, max);
}

static dsp::CompressorSettings read_settings(const SceNgsCompressorParams &params) {
    float ratio = finite_or(params.fRatio, 1.0f);
    if (ratio > 0.0f && ratio < 1.0f)
        ratio = 1.0f / ratio;

    dsp::CompressorSettings settings;
    settings.ratio = std::clamp(ratio, 1.0f, 100.0f);
    settings.threshold_db = std::clamp(finite_or(params.fThreshold, 0.0f), -96.0f, 24.0f);
    settings.attack_s = read_seconds(params.fAttack, 0.0001f, 1.0f);
    settings.release_s = read_seconds(params.fRelease, 0.001f, 5.0f);
    settings.makeup_db = std::clamp(finite_or(params.fMakeupGain, 0.0f), -24.0f, 24.0f);
    settings.knee_db = std::clamp(std::fabs(finite_or(params.fSoftKnee, 0.0f)), 0.0f, 24.0f);
    settings.rms = params.nPeakMode == SCE_NGS_COMPRESSOR_RMS_MODE;
    settings.stereo_link = params.nStereoLink == SCE_NGS_COMPRESSOR_STEREO_LINK_ON;
    return settings;
}

std::unique_ptr<ModuleLogicalState> CompressorModule::create_logical_state() const {
    return std::make_unique<CompressorLogicalState>();
}

void CompressorModule::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE)
        data.get_logical_state<CompressorLogicalState>()->compressor = {};
}

bool CompressorModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    if (data.is_bypassed)
        return false;

    const SceNgsCompressorParams *params = data.get_parameters<SceNgsCompressorParams>(mem);
    if (!params || (params->desc.id != SCE_NGS_COMPRESSOR_PARAMS_STRUCT_ID && params->desc.id != SCE_NGS_COMPRESSOR_PARAMS_STRUCT_ID_V2))
        return false;

    Voice *voice = data.parent;
    const System *system = voice->rack->system;
    float *signal = reinterpret_cast<float *>(voice->products[0].data);
    if (!signal || system->granularity <= 0)
        return false;

    // A side-chain compressor bus has a second input that drives the level detector.
    const float *key = signal;
    if (voice->inputs.inputs.size() > 1 && !voice->inputs.inputs[1].empty())
        key = reinterpret_cast<const float *>(voice->inputs.inputs[1].data());

    CompressorLogicalState *logical = data.get_logical_state<CompressorLogicalState>();
    const dsp::CompressorLevels levels = dsp::process_compressor(signal, key, system->granularity, system->sample_rate,
        read_settings(*params), logical->compressor);

    SceNgsCompressorStates *state = data.get_state<SceNgsCompressorStates>();
    for (int channel = 0; channel < SCE_NGS_MAX_SYSTEM_CHANNELS; channel++) {
        state->fInputLevel[channel] = levels.input_peak[channel];
        state->fOutputLevel[channel] = levels.output_peak[channel];
    }

    return false;
}
} // namespace ngs
