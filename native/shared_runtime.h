// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_capture.h"
#include "snapshot_slot.h"
#include "input_policy.h"
#include "native_weapon.h"
#include <atomic>
#include <mutex>
namespace vcop2 {
// The engine and host are built with the same C++ ABI and share this object.
// Scene ownership is immutable; the small cabinet/state messages use one lock.
struct SharedRuntime {
    static constexpr uint32_t Abi=0x56433205;
    uint32_t abi{Abi};
    SnapshotSlot<const NativeFrame> scene;
    std::mutex mutex;
    GameState state;
    std::array<std::shared_ptr<const NativeWeapon>,7> weapons;
    CabinetInput input;
    CabinetInput engine_input;
    CabinetLatch input_latch;
    OperatorLatch operator_latch;
    std::atomic<bool> host_ready{},pause_requested{},quit_requested{},service_requested{},service_active{};
    std::atomic<uint16_t> operator_pending{},operator_latched{};
    std::atomic<float> camera_fov{110.f};
    void release_inputs() {
        std::lock_guard lock(mutex);input={};engine_input={};input_latch={};operator_latch={};
        operator_pending=0;operator_latched=0;
    }
    void reset() {
        scene.store({});weapons={};
        {std::lock_guard lock(mutex);state={};input={};engine_input={};input_latch={};operator_latch={};}
        host_ready=false;pause_requested=false;quit_requested=false;service_requested=false;service_active=false;
        operator_pending=0;operator_latched=0;camera_fov=110.f;
    }
};
inline SharedRuntime& shared_runtime(){static SharedRuntime state;return state;}
}
