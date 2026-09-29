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

#include <ngs/modules/reverb.h>

namespace ngs {

// The units are I3DL2: millibels for the levels, seconds for the times,
// percent for diffusion and density, and Hz for the reference. The values
// that Uncharted sends fit these ranges (for example reflections delay 0.3,
// reverb delay 0.1, reflections 1000, reverb 2000, and dry -10000). The early
// reflection patterns and the LF parameters are not known, and the LF
// parameters are not used.

std::unique_ptr<ModuleRuntimeState> ReverbModule::create_runtime_state() const {
    return std::make_unique<ReverbRuntimeState>();
}

void ReverbModule::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE)
        data.get_runtime_state<ReverbRuntimeState>()->reverb.reset();
}

bool ReverbModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    if (data.is_bypassed)
        return false;

    const SceNgsReverbParams *params = data.get_parameters<SceNgsReverbParams>(mem);
    if (!params || (params->desc.id != SCE_NGS_REVERB_PARAMS_STRUCT_ID && params->desc.id != SCE_NGS_REVERB_PARAMS_STRUCT_ID_V2))
        return false;

    Voice *voice = data.parent;
    float *signal = reinterpret_cast<float *>(voice->products[0].data);
    if (!signal || voice->rack->system->granularity <= 0)
        return false;

    dsp::ReverbSettings settings;
    settings.room_mb = params->fRoom;
    settings.room_hf_mb = params->fRoomHF;
    settings.decay_time_s = params->fDecayTime;
    settings.decay_hf_ratio = params->fDecayHFRatio;
    settings.reflections_mb = params->fReflections;
    settings.reflections_delay_s = params->fReflectionsDelay;
    settings.reverb_mb = params->fReverb;
    settings.reverb_delay_s = params->fReverbDelay;
    settings.diffusion_percent = params->fDiffusion;
    settings.density_percent = params->fDensity;
    settings.hf_reference_hz = params->fHFReference;
    settings.dry_mb = params->fDryMB;
    settings.early_pattern[0] = static_cast<uint32_t>(params->eEarlyReflectionPattern[0]);
    settings.early_pattern[1] = static_cast<uint32_t>(params->eEarlyReflectionPattern[1]);
    settings.early_scalar_percent = params->fEarlyReflectionScalar;

    data.get_runtime_state<ReverbRuntimeState>()->reverb.process(signal, voice->rack->system->granularity, voice->rack->system->sample_rate, settings);
    return false;
}
} // namespace ngs
