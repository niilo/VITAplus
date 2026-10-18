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

#include <ngs/modules/atrac9.h>
#include <ngs/modules/envelope.h>
#include <ngs/scheduler.h>
#include <ngs/system.h>

#include <kernel/state.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <util/log.h>
#include <util/vector_utils.h>

namespace ngs {

bool VoiceScheduler::deque_voice(Ptr<Voice> voice) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);

    return vector_utils::erase_first(queue, voice);
}

void VoiceScheduler::deque_insert(const MemState &mem, Ptr<Voice> voice_handle) {
    Voice *const voice = voice_handle.get(mem);
    const std::lock_guard<std::recursive_mutex> guard(mutex);
    int32_t lowest_dest_pos = queue.size();

    // Check its dependencies position
    for (auto &patches : voice->patches) {
        for (const auto patch : patches) {
            if (!patch) {
                continue;
            }

            Voice *dest = patch.get(mem)->dest.get(mem);
            if (!dest) {
                continue;
            }
            const int32_t pos = get_position(Ptr<Voice>(dest, mem));

            if (pos == -1) {
                continue;
            }

            lowest_dest_pos = std::min<int32_t>(lowest_dest_pos, pos);
        }
    }

    queue.insert(queue.begin() + lowest_dest_pos, voice_handle);
}

bool VoiceScheduler::play(const MemState &mem, Voice *voice) {
    if (voice->state != VOICE_STATE_AVAILABLE) {
        static std::atomic<uint64_t> refused{ 0 };
        LOG_WARN("[NGSLIFE] VoicePlay REFUSED #{}: voice={} state={} paused={} keyed_off={} pending={}",
            refused.fetch_add(1, std::memory_order_relaxed) + 1, fmt::ptr(voice),
            static_cast<int>(voice->state), voice->is_paused ? 1 : 0, voice->is_keyed_off ? 1 : 0, voice->is_pending ? 1 : 0);
        return false;
    }

    // Transition
    voice->transition(mem, VOICE_STATE_ACTIVE);

    const auto &voice_modules = voice->rack.get(mem)->modules;
    for (size_t i = 0; i < voice_modules.size(); i++)
        if (voice_modules[i])
            voice_modules[i]->on_key_on(mem, voice->datas[i]);

    // Should Enqueue
    if (voice->is_paused) {
        static std::atomic<uint64_t> paused_keys{ 0 };
        const uint64_t n = paused_keys.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n <= 3 || (n % 4096) == 0)
            LOG_DEBUG("[NGSLIFE] voice={} keyed on while paused ({} so far) - runs at the next resume", fmt::ptr(voice), n);
    }
    if (!voice->is_paused)
        deque_insert(mem, Ptr<Voice>(voice, mem));

    return true;
}

bool VoiceScheduler::pause(const MemState &mem, Voice *voice) {
    if (!voice->is_paused) {
        voice->is_paused = true;

        // Remove from the list
        if (voice->state == VOICE_STATE_ACTIVE || voice->state == VOICE_STATE_FINALIZING)
            deque_voice(Ptr<Voice>(voice, mem));
        return true;
    }

    return false;
}

bool VoiceScheduler::resume(const MemState &mem, Voice *voice) {
    if (!voice->is_paused) {
        return false;
    }

    voice->is_paused = false;

    if (voice->state == VOICE_STATE_ACTIVE || voice->state == VOICE_STATE_FINALIZING)
        deque_insert(mem, Ptr<Voice>(voice, mem));

    return true;
}

bool VoiceScheduler::stop(const MemState &mem, Voice *voice) {
    if (voice->state != VOICE_STATE_ACTIVE && voice->state != VOICE_STATE_FINALIZING)
        return false;

    voice->transition(mem, VOICE_STATE_AVAILABLE);

    // is_paused is set by sceNgsVoicePause and gates the queue insert in play.
    // Leaving it set here meant a voice that was paused and then stopped stayed
    // flagged, so a later play inserted nothing and the voice never produced
    // sound again for the rest of the session.
    voice->is_paused = false;

    deque_voice(Ptr<Voice>(voice, mem));

    return true;
}

bool VoiceScheduler::off(const MemState &mem, Voice *voice) {
    if (voice->state != VOICE_STATE_ACTIVE)
        return false;

    voice->transition(mem, VOICE_STATE_FINALIZING);

    return true;
}

void VoiceScheduler::update(KernelState &kern, const MemState &mem, const SceUID thread_id) {
    std::unique_lock<std::recursive_mutex> scheduler_lock(mutex);
    is_updating = true;

    {
        constexpr bool NGS_CENSUS_VERBOSE = false;
        static uint32_t census_ticks = 0;
        if (NGS_CENSUS_VERBOSE && (++census_ticks % 512) == 0 && !queue.empty()) {
            System *system = queue.front().get(mem)->system(mem);
            if (system) {
                static std::unordered_map<const Voice *, uint32_t> nonavail_since;
                std::string report;
                uint32_t total = 0, busy = 0, paused_out_of_queue = 0, stuck = 0;
                for (const Ptr<Rack> &handle : system->racks) {
                    Rack *const rack = handle.get(mem);
                    if (!rack || rack->voices.size() <= 1)
                        continue;
                    for (const auto &vp : rack->voices) {
                        Voice *v = vp.get(mem);
                        if (!v)
                            continue;
                        total++;
                        if (v->state == VOICE_STATE_AVAILABLE && !v->is_paused) {
                            nonavail_since.erase(v);
                            continue;
                        }
                        busy++;
                        const bool in_queue = get_position(Ptr<Voice>(v, mem)) >= 0;
                        if (v->is_paused && !in_queue)
                            paused_out_of_queue++;
                        auto [it, fresh] = nonavail_since.try_emplace(v, census_ticks);
                        const uint32_t rounds = (census_ticks - it->second) / 512;
                        if (rounds >= 12) { // ~66s continuously not reclaimable
                            stuck++;
                            const uint32_t mod0 = (!rack->modules.empty() && rack->modules[0]) ? rack->modules[0]->module_id() : 0;
                            std::string stream;
                            if ((mod0 == 0x5CAA || mod0 == 0x5CE6) && !v->datas.empty()) {
                                const auto *st = reinterpret_cast<const SceNgsAT9States *>(v->datas[0].get_state<SceNgsAT9States>());
                                const auto *pp = v->datas[0].get_parameters<SceNgsAT9Params>(mem);
                                const int cb = st ? st->current_buffer : -99;
                                uint64_t in_head = 0;
                                int in_nonzero = -1;
                                if (pp && cb >= 0 && cb < 4 && pp->buffer_params[cb].buffer) {
                                    const uint8_t *ib = pp->buffer_params[cb].buffer.cast<uint8_t>().get(mem);
                                    const int32_t ilen = std::min<int32_t>(pp->buffer_params[cb].bytes_count, 1280);
                                    if (ib && ilen > 0) {
                                        for (int k = 0; k < 8 && k < ilen; k++)
                                            in_head = (in_head << 8) | ib[k];
                                        in_nonzero = 0;
                                        for (int k = 0; k < ilen; k++)
                                            if (ib[k])
                                                in_nonzero++;
                                    }
                                }
                                stream = fmt::format(" buf={} buf_bytes={} consumed={} samples_out={} in_head=0x{:016X} in_nonzero={}",
                                    cb, (pp && cb >= 0 && cb < 4) ? pp->buffer_params[cb].bytes_count : -1,
                                    st ? st->bytes_consumed_since_key_on : -1, st ? st->samples_generated_total : -1,
                                    in_head, in_nonzero);
                            }
                            float peak = 0.0f;
                            if (v->products[0].data) {
                                const float *pd = reinterpret_cast<const float *>(v->products[0].data);
                                for (int k = 0; k < system->granularity * 2; k++)
                                    peak = std::max(peak, std::abs(pd[k]));
                            }
                            report += fmt::format(" [voice={} rack={} mod0={:#x} state={} paused={} in_queue={} stuck~{}s peak={:.4f}{}]",
                                fmt::ptr(v), fmt::ptr(rack), mod0, static_cast<int>(v->state), v->is_paused ? 1 : 0,
                                in_queue ? 1 : 0, rounds * 55 / 10, peak, stream);
                            constexpr bool FORCE_RELEASE_SILENT_VOICES = false;
                            if (FORCE_RELEASE_SILENT_VOICES && peak <= 0.0001f && rounds >= 12) {
                                LOG_WARN("[NGSLIFE] FORCING voice={} back to AVAILABLE (silent {}s)", fmt::ptr(v), rounds * 55 / 10);
                                if (in_queue)
                                    deque_voice(Ptr<Voice>(v, mem));
                                v->state = VOICE_STATE_AVAILABLE;
                                v->is_paused = false;
                                v->is_pending = false;
                                v->is_keyed_off = false;
                                nonavail_since.erase(v);
                            }
                        }
                    }
                }
                if (stuck)
                    LOG_WARN("[NGSLIFE] STUCK VOICES ({} of {} busy, {} total): a voice that never finishes can NEVER be replayed:{}", stuck, busy, total, report);
                else
                    LOG_INFO("[NGSLIFE] voice census (rack-wide): total={} busy={} paused_out_of_queue={} stuck=0", total, busy, paused_out_of_queue);
            }
        }
    }

    // make a copy of the queue, this way we have no issue if it is modified in a callback
    std::vector<Ptr<Voice>> queue_copy = queue;

    // A voice can point at a released rack. sceNgsRackRelease destroys the
    // voices and then the rack, and the path with no callback calls
    // release_rack directly rather than waiting for the update to finish, so a
    // voice copied out of the queue here can name a destroyed rack. Reading
    // through such a voice faults at the offset of the member it reaches, which
    // is what PCSA00080 hit in sceNgsSystemUpdate. Drop those voices once, before
    // any loop below reads through a voice.
    queue_copy.erase(std::remove_if(queue_copy.begin(), queue_copy.end(),
                         [this, &mem](const Ptr<Voice> &handle) {
                             const Voice *const voice = handle.get(mem);
                             const Rack *const rack = voice ? voice->rack.get(mem) : nullptr;
                             if (rack && rack->vdef.get(mem))
                                 return false;
                             deque_voice(handle);
                             return true;
                         }),
        queue_copy.end());

    // Do a first routine to clear inputs from previous update session
    for (const Ptr<Voice> &handle : queue_copy) {
        handle.get(mem)->inputs.reset_inputs();
    }

    Ptr<Voice> implicit_master;
    if (use_implicit_master_routing) {
        Ptr<Voice> master;
        bool several_masters = false;
        for (const Ptr<Voice> &handle : queue_copy) {
            ngs::Voice *const voice = handle.get(mem);
            ngs::VoiceDefinition *const vdef = voice->rack.get(mem)->vdef.get(mem);
            if (vdef && vdef->type == BussType::BUSS_MASTER) {
                if (master)
                    several_masters = true;
                master = handle;
            }
        }

        if (master && !several_masters) {
            bool master_has_explicit_source = false;
            for (const Ptr<Voice> &handle : queue_copy) {
                ngs::Voice *const voice = handle.get(mem);
                if (handle == master)
                    continue;
                for (const auto &patches : voice->patches) {
                    for (const auto &patch_ptr : patches) {
                        if (!patch_ptr)
                            continue;
                        const Patch *patch = patch_ptr.get(mem);
                        if (patch && patch->is_active() && patch->dest.get(mem) == master.get(mem))
                            master_has_explicit_source = true;
                    }
                }
            }

            if (!master_has_explicit_source)
                implicit_master = master;
        }
    }

    // Games mute voices by disconnecting them so only fed dead-end submixes fall back to the master
    std::vector<Ptr<Voice>> implicit_sources;
    if (implicit_master) {
        std::unordered_map<Ptr<Voice>, uint32_t> incoming_count;
        std::unordered_set<Ptr<Voice>> has_outgoing;

        for (const Ptr<Voice> &handle : queue_copy) {
            ngs::Voice *const voice = handle.get(mem);
            for (const auto &patches : voice->patches) {
                for (const auto &patch_ptr : patches) {
                    if (!patch_ptr)
                        continue;
                    const Patch *patch = patch_ptr.get(mem);
                    if (!patch || !patch->is_active())
                        continue;
                    const Ptr<Voice> dest = patch->dest;
                    if (!dest)
                        continue;

                    // an all-zero volume matrix carries nothing, so it does not count as "routed"
                    const bool carries_audio = patch->volume_matrix[0][0] != 0.0f
                        || patch->volume_matrix[0][1] != 0.0f
                        || patch->volume_matrix[1][0] != 0.0f
                        || patch->volume_matrix[1][1] != 0.0f;
                    if (carries_audio)
                        has_outgoing.insert(handle);

                    incoming_count[dest]++;
                }
            }
        }

        for (const Ptr<Voice> &handle : queue_copy) {
            if (handle == implicit_master || has_outgoing.contains(handle))
                continue;
            if (incoming_count[handle] == 0)
                continue;
            implicit_sources.push_back(handle);
        }

        std::stable_sort(implicit_sources.begin(), implicit_sources.end(),
            [&incoming_count](const Ptr<Voice> &a, const Ptr<Voice> &b) { return incoming_count[a] > incoming_count[b]; });

        constexpr size_t sanity_cap = 8;
        if (implicit_sources.size() > sanity_cap)
            implicit_sources.resize(sanity_cap);
    }

    if (implicit_master) {
        // the master must run after the voices that implicitly feed it
        if (vector_utils::erase_first(queue_copy, implicit_master))
            queue_copy.push_back(implicit_master);
    }

    for (const Ptr<Voice> &handle : queue_copy) {
        ngs::Voice *const voice = handle.get(mem);
        // Modify the state, in peace....
        std::unique_lock<std::mutex> voice_lock(*voice->voice_mutex);

        // An earlier voice's guest callback may have paused or stopped this one
        // after queue_copy was taken. Read the current state rather than the
        // snapshot, or a stopped voice runs another tick and a key-off is undone.
        if (voice->is_paused || (voice->state != VOICE_STATE_ACTIVE && voice->state != VOICE_STATE_FINALIZING)) {
            continue;
        }

        memset(voice->products, 0, sizeof(voice->products));

        bool finished = false;
        uint32_t finished_module = 0;

        const auto &voice_modules = voice->rack.get(mem)->modules;

        if (voice->state == VOICE_STATE_FINALIZING) {
            // A looping source cannot reach an end after key-off, so with no
            // release envelope the voice would never finish. Complete it here,
            // before any more audio is decoded, and still let the callback run
            // from the deferred boundary below.
            bool has_release_envelope = false;
            for (size_t i = 0; i < voice_modules.size(); ++i) {
                const auto &module = voice_modules[i];
                if (!module || module->module_id() != SCE_NGS_ENVELOPE_MODULE_ID) {
                    continue;
                }
                ModuleData &data = voice->datas[i];
                if (data.is_bypassed) {
                    continue;
                }
                const auto *params = data.get_parameters<SceNgsEnvelopeParams>(mem);
                has_release_envelope |= params && params->desc.id == SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID && params->uNumPoints != 0;
            }

            if (!has_release_envelope) {
                finished = true;
                for (const auto &module : voice_modules) {
                    if (module) {
                        finished_module = module->module_id();
                        break;
                    }
                }
            }
        }

        for (size_t i = 0; !finished && i < voice_modules.size(); i++) {
            if (voice_modules[i]) {
                if (voice_modules[i]->process(kern, mem, thread_id, voice->datas[i], scheduler_lock, voice_lock)) {
                    finished = true;
                    finished_module = voice_modules[i]->module_id();
                }
            }
        }
        if (finished) {
            {
                static std::atomic<uint64_t> fin{ 0 };
                const uint64_t n = fin.fetch_add(1, std::memory_order_relaxed) + 1;
                if ((n % 128) == 0)
                    LOG_DEBUG("[NGSLIFE] voice finishes so far: {} (callback={})", n, voice->finished_callback ? "yes" : "NONE");
            }
            voice->is_keyed_off = true;
            voice->transition(mem, VOICE_STATE_FINALIZING);
            voice->is_keyed_off = false;
            stop(mem, voice);
            if (voice->finished_callback) {
                voice_lock.unlock();
                scheduler_lock.unlock();
                voice->invoke_callback(kern, mem, thread_id, voice->finished_callback, voice->finished_callback_user_data, finished_module);
                scheduler_lock.lock();
                voice_lock.lock();
            }
        }

        const bool can_route_to_master = implicit_master && std::ranges::contains(implicit_sources, handle);

        for (size_t i = 0; i < voice->rack.get(mem)->vdef.get(mem)->output_count; i++) {
            if (voice->products[i].data) {
                const bool delivered = deliver_data(mem, queue_copy, voice, static_cast<uint8_t>(i), voice->products[i]);

                if (!delivered && can_route_to_master && i == 0)
                    deliver_data_to_master(mem, implicit_master.get(mem), voice, voice->products[i]);
            }
        }

        voice->frame_count++;
    }

    while (!operations_pending.empty()) {
        OperationPending &op = operations_pending.front();

        switch (op.type) {
        case PendingType::ReleaseRack:
            release_rack(*op.release_data.state, mem, op.system.get(mem), op.release_data.rack.get(mem));
            // Run the callback, which is known to be defined. The thread may
            // have exited while the operation was queued, so check before
            // dereferencing.
            if (const ThreadStatePtr thread = kern.get_thread(thread_id)) {
                thread->run_callback(op.release_data.callback, { op.release_data.rack.cast<void>().address() });
            }
            break;
        }

        operations_pending.pop();
    }

    is_updating = false;
    condvar.notify_all();
}

int32_t VoiceScheduler::get_position(Ptr<Voice> v) {
    // we assume the scheduler lock is being held when calling this function
    return vector_utils::find_index(queue, v);
}

bool VoiceScheduler::resort_to_respect_dependencies(const MemState &mem, Ptr<Voice> source_handle) {
    Voice *const source = source_handle.get(mem);
    // this function is called by patch, which already acquired the scheduler mutex

    // Get my position
    int32_t position = get_position(source_handle);

    if (position == -1) {
        return false;
    }

    // Check all dependencies, could be optimized- @sunho suggested dfs topological sort
    for (size_t i = 0; i < source->patches.size(); i++) {
        for (const auto &patch : source->patches[i]) {
            if (!patch || !patch.get(mem)->is_active()) {
                continue;
            }

            const Ptr<Voice> dest_handle = patch.get(mem)->dest;
            Voice *dest = dest_handle.get(mem);
            if (!dest) {
                continue;
            }
            const int32_t dest_pos = get_position(dest_handle);

            if (dest_pos == -1) {
                // Maybe not scheduled yet. Continue
                continue;
            }

            if (dest_pos < position) {
                // Switch to the end. Resort dependencies for this one that just got sorted too.
                std::rotate(queue.begin() + dest_pos, queue.begin() + dest_pos + 1, queue.end());
                resort_to_respect_dependencies(mem, dest_handle);
                position = get_position(source_handle);
            }
        }
    }

    return true;
}

Ptr<Patch> VoiceScheduler::patch(const MemState &mem, SceNgsPatchSetupInfo *info) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);
    // First, check if these two voices are scheduled yet
    const Ptr<Voice> source_handle = info->source;
    const Ptr<Voice> dest_handle = info->dest;
    Voice *source = source_handle.get(mem);
    Voice *dest = dest_handle.get(mem);

    Ptr<Patch> patch = source->patch(mem, info->source_output_index, info->source_output_subindex, info->dest_input_index, info->source, info->dest);

    if (!patch) {
        return patch;
    }

    const int32_t source_pos = get_position(source_handle);
    const int32_t dest_pos = get_position(dest_handle);

    if (source_pos == -1 || dest_pos == -1) {
        // Later
        return patch;
    }

    resort_to_respect_dependencies(mem, source_handle);
    return patch;
}
} // namespace ngs
