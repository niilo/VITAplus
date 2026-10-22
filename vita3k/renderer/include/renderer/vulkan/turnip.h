// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <renderer/types.h>

namespace renderer::vulkan {

// Turnip can update LRZ incorrectly when a shader reads the colour attachment.
// An always-passing stencil equality test prevents that LRZ write.
inline bool fetch_draw_stops_lrz_write(bool is_mesa_turnip, bool direct_fragcolor,
    const GxmRecordState &record, const SceGxmProgram &fragment_program) {
    return is_mesa_turnip && direct_fragcolor && fragment_program.is_frag_color_used() && !fragment_program.has_no_effect()
        && record.front_side_fragment_program_mode != SCE_GXM_FRAGMENT_PROGRAM_DISABLED
        && record.front_depth_write_mode == SCE_GXM_DEPTH_WRITE_ENABLED
        && record.front_stencil_state_op.func == SCE_GXM_STENCIL_FUNC_ALWAYS
        && (record.two_sided != SCE_GXM_TWO_SIDED_ENABLED || record.back_stencil_state_op.func == SCE_GXM_STENCIL_FUNC_ALWAYS);
}

} // namespace renderer::vulkan
