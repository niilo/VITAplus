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

#include <ngs/modules/filter.h>

#include <algorithm>
#include <cstring>

namespace ngs {

// The units come from the values that Uncharted sends (for example a
// low-pass at 875 Hz to 23499 Hz with resonance 0.5, and gain -89.9):
// frequency in Hz, resonance as the filter Q, and gain in dB.

void FilterStage::run(float *samples, const uint32_t frames, const dsp::BiquadCoeffs &coeffs) {
    if (!active) {
        history = {};
        active = true;
    }
    dsp::process_biquad(samples, frames, coeffs, history);
}

bool filter_is_off(const SceNgsParamFilter &filter) {
    return filter.eFilterMode == SCE_NGS_FILTER_MODE_OFF || filter.eFilterMode > SCE_NGS_FILTER_LOWPASS_RESONANT_NORMALIZED;
}

dsp::BiquadCoeffs filter_coeffs(const SceNgsParamFilter &filter, const int32_t sample_rate) {
    return dsp::make_biquad(static_cast<dsp::FilterType>(filter.eFilterMode), filter.fFrequency, filter.fResonance, filter.fGain, sample_rate);
}

dsp::BiquadCoeffs filter_coeffs(const SceNgsParamCoEff &coeff) {
    const dsp::BiquadCoeffs coeffs{ coeff.fB0, coeff.fB1, coeff.fB2, coeff.fA1, coeff.fA2 };
    return dsp::biquad_is_valid(coeffs) ? coeffs : dsp::BiquadCoeffs{};
}

float *own_product(ModuleData &data, const uint32_t index) {
    Voice *voice = data.parent;
    uint8_t *buffer = voice->products[index].data;
    if (!buffer)
        return nullptr;

    bool shared = false;
    for (uint32_t i = 0; i < MAX_VOICE_OUTPUT; i++)
        shared |= i != index && voice->products[i].data == buffer;

    if (shared) {
        const size_t size = static_cast<size_t>(voice->rack->system->granularity) * sizeof(float) * 2;
        data.ensure_scratch_size(size);
        std::memcpy(data.scratch_data.data(), buffer, size);
        voice->products[index].data = data.scratch_data.data();
    }

    return reinterpret_cast<float *>(voice->products[index].data);
}

std::unique_ptr<ModuleLogicalState> FilterModule::create_logical_state() const {
    return std::make_unique<FilterLogicalState>();
}

void FilterModule::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE)
        data.get_logical_state<FilterLogicalState>()->stage = {};
}

bool FilterModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    Voice *voice = data.parent;

    // Definitions with filters have 2 outputs, and each output has its own
    // filter: the first filter module acts on output 0, the second on output
    // 1 (vitaAL puts its distance filter on the first one and plays output 0).
    // Both outputs start from the same signal.
    uint32_t send, filters;
    module_position<FilterModule>(data, send, filters);
    if (send == 0)
        voice->products[1] = voice->products[0];

    FilterLogicalState *logical = data.get_logical_state<FilterLogicalState>();
    if (data.is_bypassed || send >= voice->rack->vdef->output_count) {
        logical->stage.active = false;
        return false;
    }

    const SceNgsParamsDescriptor *desc = data.get_parameters<SceNgsParamsDescriptor>(mem);
    dsp::BiquadCoeffs coeffs;
    if (desc && desc->id == SCE_NGS_FILTER_PARAMS_STRUCT_ID) {
        const auto *params = reinterpret_cast<const SceNgsFilterParams *>(desc);
        if (filter_is_off(params->params)) {
            logical->stage.active = false;
            return false;
        }
        coeffs = filter_coeffs(params->params, voice->rack->system->sample_rate);
    } else if (desc && desc->id == SCE_NGS_FILTER_PARAMS_COEFF_STRUCT_ID) {
        coeffs = filter_coeffs(reinterpret_cast<const SceNgsFilterParamsCoEff *>(desc)->params);
    } else {
        logical->stage.active = false;
        return false;
    }

    if (float *signal = own_product(data, send))
        logical->stage.run(signal, voice->rack->system->granularity, coeffs);

    return false;
}
} // namespace ngs
