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
#include <ngs/param_log.h>
#include <util/log.h>

namespace ngs {

// Not implemented. The sound passes through with no change, and the
// parameters are logged. The units are probably I3DL2 (millibels, seconds and
// percent), but no NGS source confirms it.

bool ReverbModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    if (!data.is_bypassed) {
        const SceNgsParamsDescriptor *desc = data.get_parameters<SceNgsParamsDescriptor>(mem);
        if (desc && (desc->id == SCE_NGS_REVERB_PARAMS_STRUCT_ID || desc->id == SCE_NGS_REVERB_PARAMS_STRUCT_ID_V2)) {
            LOG_WARN_ONCE("Game is using unimplemented reverb audio module");
            const auto *params = reinterpret_cast<const SceNgsReverbParams *>(desc);
            if (is_new_param_set(module_id(), params, sizeof(*params)))
                LOG_INFO("NGS reverb module {}: id {:#x}, room {}, room HF {}, decay time {}, decay HF ratio {}, reflections {}, "
                         "reflections delay {}, reverb {}, reverb delay {}, diffusion {}, density {}, HF reference {}, "
                         "early reflection pattern {} {}, early reflection scalar {}, LF reference {}, room LF {}, dry {}",
                    data.index, params->desc.id, params->fRoom, params->fRoomHF, params->fDecayTime, params->fDecayHFRatio,
                    params->fReflections, params->fReflectionsDelay, params->fReverb, params->fReverbDelay, params->fDiffusion,
                    params->fDensity, params->fHFReference, static_cast<int32_t>(params->eEarlyReflectionPattern[0]),
                    static_cast<int32_t>(params->eEarlyReflectionPattern[1]), params->fEarlyReflectionScalar,
                    params->fLFReference, params->fRoomLF, params->fDryMB);
        }
    }

    return false;
}
} // namespace ngs
