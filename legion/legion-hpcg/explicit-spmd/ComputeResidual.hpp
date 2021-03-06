/**
 * Copyright (c)      2017 Los Alamos National Security, LLC
 *                         All rights reserved.
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

//@HEADER
// ***************************************************
//
// HPCG: High Performance Conjugate Gradient Benchmark
//
// Contact:
// Michael A. Heroux ( maherou@sandia.gov)
// Jack Dongarra     (dongarra@eecs.utk.edu)
// Piotr Luszczek    (luszczek@eecs.utk.edu)
//
// ***************************************************
//@HEADER

/*!
    @file ComputeResidual.cpp

    HPCG routine
 */

#pragma once

#include "LegionArrays.hpp"
#include "CollectiveOps.hpp"

#include "hpcg.hpp"

#include <cmath>

/**
 *
 */
struct ComputeResidualArgs {
    local_int_t n;
};

/*!
    Routine to compute the inf-norm difference between two vectors where:

    @param[in]  n        number of vector elements (local to this processor).

    @param[in]  v1, v2   input vectors.

    @param[out] residual pointer to scalar value; on exit, will contain result:
                inf-norm difference.

    @return Returns zero on success and a non-zero value otherwise.
*/
inline int
ComputeResidualKernel(
    const ComputeResidualArgs &args,
    Array<floatType> &v1,
    Array<floatType> &v2,
    floatType &residual
) {
    const floatType *const v1v = v1.data();
    const floatType *const v2v = v2.data();
    floatType local_residual = 0.0;

    for (local_int_t i = 0; i < args.n; i++) {
        floatType diff = std::fabs(v1v[i] - v2v[i]);
        if (diff > local_residual) local_residual = diff;
    }
    residual = local_residual;
    //
    return 0;
}

/**
 *
 */
inline int
ComputeResidual(
    local_int_t n,
    Array<floatType> &v1,
    Array<floatType> &v2,
    floatType &residual,
    Item< DynColl<floatType> > &dcReduceMax,
    Context ctx,
    Runtime *lrt
) {
    const ComputeResidualArgs args = {
        .n = n
    };
    //
    Future lrf;
#ifdef LGNCG_TASKING
    TaskLauncher tl(
        COMPUTE_RESIDUAL_TID,
        TaskArgument(&args, sizeof(args))
    );
    //
    v1.intent(RO_E, tl, ctx, lrt);
    v2.intent(RO_E, tl, ctx, lrt);
    //
    lrf = lrt->execute_task(ctx, tl);
#else
    floatType local_residual = 0.0;
    ComputeResidualKernel(args, v1, v2, local_residual);
    lrf = Future::from_value(lrt, local_residual);
#endif
    // Get max residual from all tasks.
    residual = allReduce(
        lrf,
        dcReduceMax,
        ctx,
        lrt
    ).get_result<floatType>(disableWarnings);
    //
    return 0;
}

/**
 *
 */
floatType
ComputeResidualTask(
    const Task *task,
    const std::vector<PhysicalRegion> &regions,
    Context ctx,
    Runtime *lrt
) {
    const auto *const args = (ComputeResidualArgs *)task->args;
    //
    Array<floatType> v1(regions[0], ctx, lrt);
    Array<floatType> v2(regions[1], ctx, lrt);
    //
    floatType local_residual = 0.0;
    ComputeResidualKernel(*args, v1, v2, local_residual);
    //
    return local_residual;
}

/**
 *
 */
inline void
registerComputeResidualTasks(void)
{
#ifdef LGNCG_TASKING
    HighLevelRuntime::register_legion_task<floatType, ComputeResidualTask>(
        COMPUTE_RESIDUAL_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        false /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "ComputeResidualTask"
    );
#endif
}
