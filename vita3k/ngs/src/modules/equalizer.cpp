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

#include <ngs/modules/equalizer.h>

namespace ngs {

// The units are the same as for the filter module (filter.cpp).

std::unique_ptr<ModuleLogicalState> EqualizerModule::create_logical_state() const {
    return std::make_unique<EqualizerLogicalState>();
}

void EqualizerModule::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE) {
        for (FilterStage &stage : data.get_logical_state<EqualizerLogicalState>()->stages)
            stage = {};
    }
}

bool EqualizerModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    Voice *voice = data.parent;

    // The first equalizer of a voice comes before the split into outputs, and
    // all outputs get its result. Some definitions (ATRAC9, template 1) have
    // one more equalizer for each output after it. Equalizer n + 1 acts on
    // output n.
    uint32_t position, equalizers;
    module_position<EqualizerModule>(data, position, equalizers);
    const bool per_output = position > 0 && equalizers == voice->rack->vdef->output_count + 1;
    const uint32_t output = per_output ? position - 1 : 0;

    const auto split_outputs = [&]() {
        if (!per_output) {
            voice->products[1] = voice->products[0];
            voice->products[2] = voice->products[0];
            voice->products[3] = voice->products[0];
        }
    };

    EqualizerLogicalState *logical = data.get_logical_state<EqualizerLogicalState>();
    const auto stop_all = [&]() {
        for (FilterStage &stage : logical->stages)
            stage.active = false;
    };

    if (data.is_bypassed) {
        stop_all();
        split_outputs();
        return false;
    }

    const SceNgsParamsDescriptor *desc = data.get_parameters<SceNgsParamsDescriptor>(mem);
    dsp::BiquadCoeffs coeffs[SCE_NGS_MAX_EQ_FILTERS];
    bool enabled[SCE_NGS_MAX_EQ_FILTERS] = {};
    if (desc && desc->id == SCE_NGS_PARAM_EQ_STRUCT_ID) {
        const auto *params = reinterpret_cast<const SceNgsParamEqParams *>(desc);
        for (uint32_t i = 0; i < SCE_NGS_MAX_EQ_FILTERS; i++) {
            enabled[i] = !filter_is_off(params->filter[i]);
            if (enabled[i])
                coeffs[i] = filter_coeffs(params->filter[i], voice->rack->system->sample_rate);
        }
    } else if (desc && desc->id == SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID) {
        const auto *params = reinterpret_cast<const SceNgsParamEqParamsCoEff *>(desc);
        for (uint32_t i = 0; i < SCE_NGS_MAX_EQ_FILTERS; i++) {
            coeffs[i] = filter_coeffs(params->filterCoEff[i]);
            enabled[i] = true;
        }
    }

    bool any = false;
    for (uint32_t i = 0; i < SCE_NGS_MAX_EQ_FILTERS; i++) {
        any |= enabled[i];
        if (!enabled[i])
            logical->stages[i].active = false;
    }

    if (any) {
        float *signal = per_output ? own_product(data, output) : reinterpret_cast<float *>(voice->products[0].data);
        if (signal) {
            // The 4 filters run in series.
            for (uint32_t i = 0; i < SCE_NGS_MAX_EQ_FILTERS; i++) {
                if (enabled[i])
                    logical->stages[i].run(signal, voice->rack->system->granularity, coeffs[i]);
            }
        }
    }

    split_outputs();
    return false;
}
} // namespace ngs
