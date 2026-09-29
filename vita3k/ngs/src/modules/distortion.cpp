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

#include <ngs/modules/distortion.h>
#include <ngs/param_log.h>
#include <util/log.h>

namespace ngs {

// Not implemented: no public source describes the distortion curve. A wrong
// curve sounds broken, so the sound passes through with no change, and the
// parameters are logged.

bool DistortionModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    if (!data.is_bypassed) {
        const SceNgsParamsDescriptor *desc = data.get_parameters<SceNgsParamsDescriptor>(mem);
        if (desc && desc->id == SCE_NGS_DISTORTION_PARAMS_STRUCT_ID) {
            LOG_WARN_ONCE("Game is using unimplemented distortion audio module");
            const auto *params = reinterpret_cast<const SceNgsDistortionParams *>(desc);
            if (is_new_param_set(module_id(), params, sizeof(*params)))
                LOG_INFO("NGS distortion module {}: A {}, B {}, clip {}, gate {}, wet gain {}, dry gain {}", data.index,
                    params->fA, params->fB, params->fClip, params->fGate, params->fWetGain, params->fDryGain);
        }
    }

    return false;
}
} // namespace ngs
