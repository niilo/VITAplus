// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program.  If not, see <https://www.gnu.org/licenses/>.

// sceNgsVoiceKeyOff used to stop the voice in the same call and to invoke the
// finish callback itself, so the release envelope never ran and a note ended
// with a hard cut. VoiceScheduler::stop left is_paused set, so a voice that was
// paused and then stopped never returned to the queue. The release itself
// started from release_height, which is 0 until a release has run once, so
// every first release multiplied its whole tail by zero.
//
// The tests drive a real system, rack and voice in guest memory and read the
// state machine through VoiceScheduler::update, the way the audio thread does.
// They need no device, no game and no listening. The sound itself cannot be
// checked here; the state that produces it can.
// See .scratch/emucorev-fixes/issues/08-ngs-key-off-invokes-finish-callback.md
// and .scratch/emucorev-fixes/issues/09-ngs-voice-stop-keeps-paused-flag.md.

#include <kernel/state.h>
#include <mem/functions.h>
#include <mem/state.h>
#include <ngs/modules/envelope.h>
#include <ngs/state.h>
#include <ngs/system.h>

#include <gtest/gtest.h>

#include <cstring>
#include <type_traits>

namespace {

constexpr int32_t sample_rate = 48000;
constexpr int32_t granularity = 256;
constexpr SceUID thread_id = 1;

// The NGS objects and the guest structures the tests write into share one
// allocation: the two memspaces at the front, the guest structures after them.
constexpr uint32_t arena_size = 4 * 1024 * 1024;
constexpr uint32_t system_memspace_size = 1024 * 1024;
constexpr uint32_t rack_memspace_offset = system_memspace_size;
constexpr uint32_t rack_memspace_limit = 2 * 1024 * 1024;
constexpr uint32_t guest_structs_offset = rack_memspace_offset + rack_memspace_limit;

// One emulator memory for the file. init() installs a process-wide access
// violation handler that closes over the MemState, so it has to outlive every
// test, exactly as in external_mapping_tests.cpp.
MemState &shared_state() {
    static MemState state;
    return state;
}

Address arena_base = 0;
size_t arena_cursor = 0;

// Writes a guest struct into the arena and returns its address.
template <typename T>
Ptr<T> put(const T &value) {
    static_assert(std::is_trivially_copyable_v<T>, "a guest struct is copied into guest memory");
    const Address address = arena_base + arena_cursor;
    arena_cursor += (sizeof(T) + 15) & ~size_t{ 15 };
    std::memcpy(Ptr<uint8_t>(address).get(shared_state()), &value, sizeof(T));
    return Ptr<T>(address);
}

class SchedulerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(init(shared_state(), true));
        arena_base = alloc(shared_state(), arena_size, "ngs scheduler test");
        ASSERT_NE(arena_base, 0);
    }

    MemState &mem() {
        return shared_state();
    }
};

// A system with one rack whose only module is the envelope. A source module
// would need a decoded stream, and the release path does not read one: the
// envelope ramps a gain and reports the voice finished when the ramp is over.
class EnvelopeVoiceTest : public SchedulerTest {
protected:
    void SetUp() override {
        arena_cursor = guest_structs_offset;

        ASSERT_TRUE(ngs::init(ngs_state_, mem()));

        SceNgsSystemInitParams system_params{};
        system_params.max_racks = 1;
        system_params.max_voices = 4;
        system_params.granularity = granularity;
        system_params.sample_rate = sample_rate;

        ASSERT_TRUE(ngs::init_system(ngs_state_, mem(), put(system_params).get(mem()),
            Ptr<void>(arena_base), system_memspace_size));
        system_ = Ptr<ngs::System>(arena_base).get(mem());
        ASSERT_NE(system_, nullptr);

        SceNgsRackDescription rack_description{};
        rack_description.definition = ngs::get_voice_definition(ngs_state_, mem(), ngs::BussType::BUSS_ENVELOPE);
        rack_description.voice_count = 1;
        rack_description.channels_per_voice = 2;
        rack_description.max_patches_per_input = 1;
        rack_description.patches_per_output = 1;
        const Ptr<SceNgsRackDescription> description = put(rack_description);

        const uint32_t needed = ngs::Rack::get_required_memspace_size(mem(), description.get(mem()));
        ASSERT_GT(needed, 0u);
        ASSERT_LE(needed, rack_memspace_limit);

        SceNgsBufferInfo rack_info{};
        rack_info.data = Ptr<void>(arena_base + rack_memspace_offset);
        rack_info.size = rack_memspace_limit;

        ASSERT_TRUE(ngs::init_rack(ngs_state_, mem(), Ptr<ngs::System>(system_, mem()), put(rack_info).get(mem()), description.get(mem())));

        // init_system sizes racks to max_racks null slots and init_rack
        // appends, so the rack is the last entry, not the first. The emulator
        // walks the vector and skips the null slots.
        rack_ = system_->racks.back().get(mem());
        ASSERT_NE(rack_, nullptr);
        // BUSS_ENVELOPE is an input mixer followed by the envelope.
        ASSERT_EQ(rack_->modules.size(), 2u);
        ASSERT_EQ(rack_->modules[1]->module_id(), SCE_NGS_ENVELOPE_MODULE_ID);

        voice_ = rack_->voices.front().get(mem());
        ASSERT_NE(voice_, nullptr);
        envelope_data_ = &voice_->datas[1];
    }

    void TearDown() override {
        ngs::release_system(ngs_state_, mem(), system_);
    }

    // One envelope point held at full height, then a release of the given
    // length. The long first segment keeps the voice at 1.0 until the key-off,
    // so the height the release starts from is known.
    void write_params(const uint32_t release_msecs, const uint32_t point_count = 1) {
        SceNgsEnvelopeParams params{};
        params.desc.id = SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID;
        params.envelopePoints[0].uMsecsToNextPoint = 60000;
        params.envelopePoints[0].fAmplitude = 1.0f;
        params.envelopePoints[0].eCurveType = SCE_NGS_ENVELOPE_LINEAR;
        params.uReleaseMsecs = release_msecs;
        params.uNumPoints = point_count;
        params.uLoopStart = 0;
        params.nLoopEnd = -1;
        // Written straight into the module buffer, which is what
        // sceNgsVoiceSetParameterData does for a module the game drives itself.
        // sceNgsVoiceSetPreset is not usable here: it clears the finished
        // callback.
        std::memcpy(envelope_data_->info.data.get(mem()), &params, sizeof(params));
    }

    SceNgsEnvelopeStates *envelope_state() {
        return envelope_data_->get_state<SceNgsEnvelopeStates>();
    }

    bool queued() {
        for (const Ptr<ngs::Voice> &handle : system_->voice_scheduler.queue) {
            if (handle.get(mem()) == voice_)
                return true;
        }
        return false;
    }

    bool key_off() {
        voice_->is_keyed_off = true;
        const bool moved = system_->voice_scheduler.off(mem(), voice_);
        voice_->is_keyed_off = false;
        return moved;
    }

    void update() {
        system_->voice_scheduler.update(kernel_, mem(), thread_id);
    }

    ngs::State ngs_state_;
    ngs::System *system_ = nullptr;
    ngs::Rack *rack_ = nullptr;
    ngs::Voice *voice_ = nullptr;
    ngs::ModuleData *envelope_data_ = nullptr;
    KernelState kernel_;
};

} // namespace

TEST_F(EnvelopeVoiceTest, key_off_leaves_the_voice_finalizing_and_queued) {
    write_params(500);
    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    ASSERT_EQ(voice_->state, ngs::VOICE_STATE_ACTIVE);
    ASSERT_TRUE(queued());

    ASSERT_TRUE(key_off());

    // The old path called stop() here, which moved the voice to AVAILABLE and
    // took it out of the queue in the same call.
    EXPECT_EQ(voice_->state, ngs::VOICE_STATE_FINALIZING);
    EXPECT_TRUE(queued());
}

TEST_F(EnvelopeVoiceTest, key_off_on_a_stopped_voice_changes_nothing) {
    write_params(500);
    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    ASSERT_TRUE(system_->voice_scheduler.stop(mem(), voice_));
    ASSERT_EQ(voice_->state, ngs::VOICE_STATE_AVAILABLE);

    EXPECT_FALSE(key_off());

    EXPECT_EQ(voice_->state, ngs::VOICE_STATE_AVAILABLE);
    EXPECT_FALSE(queued());
}

TEST_F(EnvelopeVoiceTest, the_first_release_starts_from_the_current_height) {
    write_params(500);
    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    update();
    ASSERT_FLOAT_EQ(envelope_state()->fCurrentHeight, 1.0f);

    ASSERT_TRUE(key_off());
    update();

    // One tick of a 500 ms release is 5.33 ms, so the height is still close to
    // one. Before the fix the first tick read release_height, which is 0 until
    // a release has run once, and the whole tail was multiplied by zero.
    EXPECT_FLOAT_EQ(envelope_state()->fReleaseScale, 1.0f);
    EXPECT_GT(envelope_state()->fCurrentHeight, 0.9f);
    EXPECT_EQ(envelope_state()->nReleasing, 1);
}

TEST_F(EnvelopeVoiceTest, the_voice_is_stopped_when_the_release_is_over) {
    write_params(100);
    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    ASSERT_TRUE(key_off());

    // 100 ms at a 256 frame granularity is 19 ticks. Until then the voice is
    // still releasing, and the updater has to keep it queued.
    for (int32_t tick = 0; tick < 18; tick++) {
        update();
        ASSERT_EQ(voice_->state, ngs::VOICE_STATE_FINALIZING) << "tick " << tick;
        ASSERT_TRUE(queued()) << "tick " << tick;
    }

    update();
    EXPECT_EQ(voice_->state, ngs::VOICE_STATE_AVAILABLE);
    EXPECT_FALSE(queued());
    EXPECT_FALSE(voice_->is_paused);
    EXPECT_FALSE(voice_->is_keyed_off);
}

TEST_F(EnvelopeVoiceTest, a_keyed_off_voice_with_no_release_envelope_is_completed) {
    // A source module loops, so a voice with no release envelope can never
    // reach an end on its own. A point count of zero makes the envelope a
    // pass-through, which is what no usable release envelope looks like.
    write_params(5000, 0);

    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    ASSERT_TRUE(key_off());

    update();
    EXPECT_EQ(voice_->state, ngs::VOICE_STATE_AVAILABLE);
    EXPECT_FALSE(queued());
}

TEST_F(EnvelopeVoiceTest, the_updater_does_not_run_a_stopped_voice) {
    // What this covers is the ordinary case: a voice stopped before the walk
    // started is out of the queue copy. The snapshot defect needs a guest
    // callback that stops a voice while the walk is running, and this harness
    // has no way to raise one, so the per-voice state check in update() has no
    // test here.
    write_params(500);
    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    ASSERT_TRUE(system_->voice_scheduler.stop(mem(), voice_));
    ASSERT_FALSE(queued());

    // The scheduler works from a copy of the queue taken before the walk, so a
    // guest callback that stops a voice during an update used to be ignored by
    // the walk and the voice ran another tick.
    update();
    EXPECT_EQ(voice_->frame_count, 0u);
    EXPECT_EQ(voice_->state, ngs::VOICE_STATE_AVAILABLE);
}

TEST_F(EnvelopeVoiceTest, stop_clears_is_paused_and_play_queues_the_voice_again) {
    write_params(500);
    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    ASSERT_TRUE(queued());

    ASSERT_TRUE(system_->voice_scheduler.pause(mem(), voice_));
    ASSERT_TRUE(voice_->is_paused);
    ASSERT_FALSE(queued());

    ASSERT_TRUE(system_->voice_scheduler.stop(mem(), voice_));
    // The old stop() left is_paused set and play() gates its queue insert on
    // it, so this voice never produced sound again for the rest of the session.
    EXPECT_FALSE(voice_->is_paused);

    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    EXPECT_EQ(voice_->state, ngs::VOICE_STATE_ACTIVE);
    EXPECT_TRUE(queued());
}

TEST_F(EnvelopeVoiceTest, a_paused_voice_is_not_processed_until_it_is_resumed) {
    write_params(500);
    ASSERT_TRUE(system_->voice_scheduler.play(mem(), voice_));
    update();
    const uint32_t frames_after_one_update = voice_->frame_count;
    ASSERT_GT(frames_after_one_update, 0u);

    ASSERT_TRUE(system_->voice_scheduler.pause(mem(), voice_));
    update();
    EXPECT_EQ(voice_->frame_count, frames_after_one_update);

    ASSERT_TRUE(system_->voice_scheduler.resume(mem(), voice_));
    update();
    EXPECT_GT(voice_->frame_count, frames_after_one_update);
}