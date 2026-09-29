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
#include <ngs/param_log.h>
#include <util/log.h>

namespace ngs {

// Not implemented, for the same reason as the filter module (filter.cpp).

bool EqualizerModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    if (!data.is_bypassed) {
        const SceNgsParamsDescriptor *desc = data.get_parameters<SceNgsParamsDescriptor>(mem);
        if (desc && desc->id == SCE_NGS_PARAM_EQ_STRUCT_ID) {
            LOG_WARN_ONCE("Game is using unimplemented equalizer audio module");
            const auto *params = reinterpret_cast<const SceNgsParamEqParams *>(desc);
            if (is_new_param_set(module_id(), params, sizeof(*params))) {
                for (uint32_t i = 0; i < SCE_NGS_MAX_EQ_FILTERS; i++) {
                    const SceNgsParamFilter &filter = params->filter[i];
                    if (filter.eFilterMode != SCE_NGS_FILTER_MODE_OFF)
                        LOG_INFO("NGS equalizer module {} filter {}: mode {}, frequency {}, resonance {}, gain {}", data.index, i,
                            static_cast<uint32_t>(filter.eFilterMode), filter.fFrequency, filter.fResonance, filter.fGain);
                }
            }
        } else if (desc && desc->id == SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID) {
            LOG_WARN_ONCE("Game is using unimplemented equalizer audio module");
            const auto *params = reinterpret_cast<const SceNgsParamEqParamsCoEff *>(desc);
            if (is_new_param_set(module_id(), params, sizeof(*params))) {
                for (uint32_t i = 0; i < SCE_NGS_MAX_EQ_FILTERS; i++) {
                    const SceNgsParamCoEff &coeff = params->filterCoEff[i];
                    LOG_INFO("NGS equalizer module {} filter {}: b0 {}, b1 {}, b2 {}, a1 {}, a2 {}", data.index, i,
                        coeff.fB0, coeff.fB1, coeff.fB2, coeff.fA1, coeff.fA2);
                }
            }
        }
    }

    // Definitions with equalizers can have up to 4 outputs
    data.parent->products[1] = data.parent->products[0];
    data.parent->products[2] = data.parent->products[0];
    data.parent->products[3] = data.parent->products[0];

    return false;
}
} // namespace ngs
