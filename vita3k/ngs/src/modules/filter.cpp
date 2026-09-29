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
#include <ngs/param_log.h>
#include <util/log.h>

namespace ngs {

// Not implemented: no public source gives the units of fResonance and fGain,
// so a filter could make the sound wrong. The sound passes through with no
// change, and the parameters are logged.

static void log_filter(const char *module, const uint32_t index, const SceNgsParamFilter &filter) {
    LOG_INFO("NGS {} filter {}: mode {}, frequency {}, resonance {}, gain {}", module, index,
        static_cast<uint32_t>(filter.eFilterMode), filter.fFrequency, filter.fResonance, filter.fGain);
}

static void log_coeffs(const char *module, const uint32_t index, const SceNgsParamCoEff &coeff) {
    LOG_INFO("NGS {} filter {}: b0 {}, b1 {}, b2 {}, a1 {}, a2 {}", module, index, coeff.fB0, coeff.fB1, coeff.fB2, coeff.fA1, coeff.fA2);
}

bool FilterModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    if (!data.is_bypassed) {
        const SceNgsParamsDescriptor *desc = data.get_parameters<SceNgsParamsDescriptor>(mem);
        if (desc && desc->id == SCE_NGS_FILTER_PARAMS_STRUCT_ID) {
            LOG_WARN_ONCE("Game is using unimplemented filter audio module");
            const auto *params = reinterpret_cast<const SceNgsFilterParams *>(desc);
            if (params->params.eFilterMode != SCE_NGS_FILTER_MODE_OFF && is_new_param_set(module_id(), params, sizeof(*params)))
                log_filter("filter module", data.index, params->params);
        } else if (desc && desc->id == SCE_NGS_FILTER_PARAMS_COEFF_STRUCT_ID) {
            LOG_WARN_ONCE("Game is using unimplemented filter audio module");
            const auto *params = reinterpret_cast<const SceNgsFilterParamsCoEff *>(desc);
            if (is_new_param_set(module_id(), params, sizeof(*params)))
                log_coeffs("filter module", data.index, params->params);
        }
    }

    // Definitions with filters have 2 outputs
    data.parent->products[1] = data.parent->products[0];

    return false;
}
} // namespace ngs
