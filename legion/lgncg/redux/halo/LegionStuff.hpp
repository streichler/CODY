/**
 * Copyright (c) 2016      Los Alamos National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2016      Stanford University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * LA-CC 10-123
 */

#pragma once

#include "legion.h"

// For serialization.
#include "cereal/cereal.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/archives/binary.hpp"

#include <vector>
#include <map>

using namespace LegionRuntime::HighLevel;
using namespace LegionRuntime::Accessor;

#define RW_E READ_WRITE, EXCLUSIVE
#define RO_E READ_ONLY , EXCLUSIVE
#define WO_E WRITE_ONLY, EXCLUSIVE

#define RW_S READ_WRITE, SIMULTANEOUS
#define RO_S READ_ONLY , SIMULTANEOUS
#define WO_S WRITE_ONLY, SIMULTANEOUS

#define DEBUG_PHASE_BARRIER_SERIALIZATION 0

namespace cereal {
    /**
     *
     */
    template<class Archive>
    inline void
    save(Archive &ar, PhaseBarrier const &pb)
    {
        PhaseBarrier copy = pb;
        ar(cereal::binary_data(&copy, sizeof(PhaseBarrier)));
    }

    /**
     *
     */
    template<class Archive>
    inline void
    load(Archive &ar, PhaseBarrier &pb)
    {
        PhaseBarrier newpb;
        ar(cereal::binary_data(&newpb, sizeof(PhaseBarrier)));
        pb = newpb;
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct PhaseBarriers {
    //
    PhaseBarrier ready;
    //
    PhaseBarrier done;

    /**
     *
     */
    PhaseBarriers(void) = default;

    /**
     *
     */
    template <class Archive>
    void
    serialize(Archive &ar)
    {
        ar(ready, done);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Task IDs
////////////////////////////////////////////////////////////////////////////////
enum {
    MAIN_TID = 0,
    GEN_PROB_TID,
    START_SOLVE,
    TEST_TID
};

////////////////////////////////////////////////////////////////////////////////
// SPMD metadata.
////////////////////////////////////////////////////////////////////////////////
struct SPMDMeta {
    int rank;
    // Number of participants in SPMD computation.
    int nRanks;
};

////////////////////////////////////////////////////////////////////////////////
// SPMD context information.
////////////////////////////////////////////////////////////////////////////////
struct SPMDContext {
    // Unique identifier for SPMD work.
    int rank;
    // Number of participants in SPMD.
    int nRanks;
};

////////////////////////////////////////////////////////////////////////////////
// Task forward declarations.
////////////////////////////////////////////////////////////////////////////////
void
mainTask(
    const Task *task,
    const std::vector<PhysicalRegion> &regions,
    Context ctx, HighLevelRuntime *runtime
);

void
genProblemTask(
    const Task *task,
    const std::vector<PhysicalRegion> &regions,
    Context ctx, HighLevelRuntime *runtime
);

void
startSolveTask(
    const Task *task,
    const std::vector<PhysicalRegion> &regions,
    Context ctx, HighLevelRuntime *runtime
);

inline void
testTask(
    const Task *task,
    const std::vector<PhysicalRegion> &regions,
    Context ctx, HighLevelRuntime *runtime
) {
    ;
}

////////////////////////////////////////////////////////////////////////////////
// Task Registration
////////////////////////////////////////////////////////////////////////////////
// TODO: be explicit about leaf tasks in registration.
inline void
registerTasks(void) {
    {
    TaskVariantRegistrar tvr(MAIN_TID, "mainTask");
    tvr.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<mainTask>(tvr, "mainTask");
    Runtime::set_top_level_task_id(MAIN_TID);
    }
    //
    HighLevelRuntime::register_legion_task<genProblemTask>(
        GEN_PROB_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(false /* leaf task */),
        "genProblemTask"
    );
    HighLevelRuntime::register_legion_task<startSolveTask>(
        START_SOLVE /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(false /* leaf task */),
        "startSolveTask"
    );
    HighLevelRuntime::register_legion_task<testTask>(
        TEST_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(false /* leaf task */),
        "testTask"
    );
}

////////////////////////////////////////////////////////////////////////////////
inline void
updateMappers(
    Machine machine,
    HighLevelRuntime *runtime,
    const std::set<Processor> &local_procs
) {
#if 0 // SKG disable for now.
    for (const auto &p : local_procs) {
        runtime->replace_default_mapper(new CGMapper(machine, runtime, p), p);
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////
inline void
LegionInit(void)
{
    registerTasks();
    HighLevelRuntime::set_registration_callback(updateMappers);
}

/**
 * courtesy of some other legion code.
 */
template <unsigned DIM, typename T>
inline bool
offsetsAreDense(const Rect<DIM> &bounds,
                const LegionRuntime::Accessor::ByteOffset *offset)
{
    off_t exp_offset = sizeof(T);
    for (unsigned i = 0; i < DIM; i++) {
        bool found = false;
        for (unsigned j = 0; j < DIM; j++)
            if (offset[j].offset == exp_offset) {
                found = true;
                exp_offset *= (bounds.hi[j] - bounds.lo[j] + 1);
                break;
            }
        if (!found) return false;
    }
    return true;
}

/**
 * courtesy of some other legion code.
 */
inline bool
offsetMismatch(int i,
               const LegionRuntime::Accessor::ByteOffset *off1,
               const LegionRuntime::Accessor::ByteOffset *off2)
{
    while (i-- > 0) {
        if ((off1++)->offset != (off2++)->offset) return true;
    }
    return false;
}

/**
 * convenience routine to get a task's ID
 */
inline int
getTaskID(
    const LegionRuntime::HighLevel::Task *task
) {
    return task->index_point.point_data[0];
}

/**
 * TODO add proc type
 */
inline int
getNumProcs(
    void
) {
    size_t nProc = 0;
    std::set<Processor> allProcs;
    Realm::Machine::get_machine().get_all_processors(allProcs);
    for (auto &p : allProcs) {
        if (p.kind() == Processor::LOC_PROC) nProc++;
    }
    return nProc;
}
