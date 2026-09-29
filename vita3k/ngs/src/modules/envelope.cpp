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

#include <ngs/modules/envelope.h>

#include <algorithm>
#include <cmath>

namespace ngs {

// Not handled: the release after key-off. sceNgsVoiceKeyOff stops the voice at
// once in Vita3K, so uReleaseMsecs is not used.

static dsp::EnvelopeShape read_shape(const SceNgsEnvelopeParams &params) {
    dsp::EnvelopeShape shape;
    shape.count = std::min<uint32_t>(params.uNumPoints, SCE_NGS_ENVELOPE_MAX_POINTS);
    shape.loop_start = params.uLoopStart;
    shape.loop_end = params.nLoopEnd;

    for (uint32_t i = 0; i < shape.count; i++) {
        const SceNgsEnvelopePoint &point = params.envelopePoints[i];
        const float amplitude = point.fAmplitude;
        shape.points[i].ms_to_next = point.uMsecsToNextPoint;
        shape.points[i].amplitude = std::isfinite(amplitude) ? std::clamp(amplitude, 0.0f, 16.0f) : 1.0f;
        shape.points[i].curved = point.eCurveType == SCE_NGS_ENVELOPE_CURVED;
    }

    return shape;
}

std::unique_ptr<ModuleLogicalState> EnvelopeModule::create_logical_state() const {
    return std::make_unique<EnvelopeLogicalState>();
}

void EnvelopeModule::on_state_change(const MemState &mem, ModuleData &data, const VoiceState previous) {
    // A voice that starts to play starts its envelope from the first point.
    if (data.parent->state == VOICE_STATE_ACTIVE && previous == VOICE_STATE_AVAILABLE)
        data.get_logical_state<EnvelopeLogicalState>()->cursor = {};
}

bool EnvelopeModule::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) {
    if (data.is_bypassed)
        return false;

    const SceNgsEnvelopeParams *params = data.get_parameters<SceNgsEnvelopeParams>(mem);
    if (!params || params->desc.id != SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID)
        return false;

    EnvelopeLogicalState *logical = data.get_logical_state<EnvelopeLogicalState>();

    // New points start the envelope again. Games start a fade this way.
    const dsp::EnvelopeShape shape = read_shape(*params);
    if (!(shape == logical->shape)) {
        logical->shape = shape;
        logical->cursor = {};
    }

    if (shape.count == 0)
        return false;

    const System *system = data.parent->rack->system;
    if (system->granularity <= 0 || system->sample_rate <= 0)
        return false;

    const double grain_ms = static_cast<double>(system->granularity) * 1000.0 / system->sample_rate;
    const float start_gain = dsp::envelope_gain(shape, logical->cursor);
    dsp::advance_envelope(shape, logical->cursor, grain_ms);
    const float end_gain = dsp::envelope_gain(shape, logical->cursor);

    if (uint8_t *signal = data.parent->products[0].data)
        dsp::apply_gain_ramp(reinterpret_cast<float *>(signal), system->granularity, start_gain, end_gain);

    // Games read this state to know where a fade is.
    SceNgsEnvelopeStates *state = data.get_state<SceNgsEnvelopeStates>();
    state->fCurrentHeight = end_gain;
    state->fPosition = static_cast<float>(logical->cursor.total_ms);
    state->fReleaseScale = 1.0f;
    state->nCurrentPoint = static_cast<SceInt32>(logical->cursor.point);
    state->nReleasing = 0;

    return false;
}
} // namespace ngs
