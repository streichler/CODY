/**
 * Copyright (c) 2014      Los Alamos National Security, LLC
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

#ifndef LGNCG_COMP_SYMGS_H_INCLUDED
#define LGNCG_COMP_SYMGS_H_INCLUDED

#include "vector.h"
#include "sparsemat.h"
#include "utils.h"
#include "cg-task-args.h"
#include "tids.h"

#include "legion.h"

/**
 * implements one step of SYMmetric Gauss-Seidel.
 */

namespace {

static inline void
applyRBPatitioning(SparseMatrix &A,
                   LegionRuntime::HighLevel::Context &ctx,
                   LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
}

} // end namespace

namespace lgncg {

// steps
// color A and push partition based on coloring

/**
 * responsible for setting up the task launch of the symmetric gauss-seidel
 * where x is unknown.
 */
static inline void
symgs(const SparseMatrix &A,
     Vector &x,
     const Vector &r,
     LegionRuntime::HighLevel::Context &ctx,
     LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;

    // sanity - make sure that all launch domains are the same size
    assert(A.vals.lDom().get_volume() == x.lDom().get_volume() &&
           x.lDom().get_volume() == r.lDom().get_volume());
    // setup per-task args
    ArgumentMap argMap;
    CGTaskArgs targs;
    targs.sa = A;
    targs.va = x;
    targs.vb = r;
    for (int i = 0; i < A.vals.lDom().get_volume(); ++i) {
        targs.sa.vals.sgb = A.vals.sgb()[i];
        targs.sa.diag.sgb = A.diag.sgb()[i];
        targs.sa.mIdxs.sgb = A.mIdxs.sgb()[i];
        targs.sa.nzir.sgb = A.nzir.sgb()[i];
        // every task gets all of x
        //targs.va.sgb = x.sgb()[i];
        targs.vb.sgb = r.sgb()[i];
        argMap.set_point(DomainPoint::from_point<1>(Point<1>(i)),
                         TaskArgument(&targs, sizeof(targs)));
    }
    for (int sweepi = 0; sweepi < 2; ++sweepi) {
    int idx = 0;
    IndexLauncher il(LGNCG_SYMGS_TID, A.vals.lDom(),
                     TaskArgument(&sweepi, sizeof(sweepi)), argMap);
    // A's regions /////////////////////////////////////////////////////////////
    // vals
    il.add_region_requirement(
        RegionRequirement(A.vals.lp(), 0, READ_ONLY, EXCLUSIVE, A.vals.lr)
    );
    il.add_field(idx++, A.vals.fid);
    // diag
    il.add_region_requirement(
        RegionRequirement(A.diag.lp(), 0, READ_ONLY, EXCLUSIVE, A.diag.lr)
    );
    il.add_field(idx++, A.diag.fid);
    // mIdxs
    il.add_region_requirement(
        RegionRequirement(A.mIdxs.lp(), 0, READ_ONLY, EXCLUSIVE, A.mIdxs.lr)
    );
    il.add_field(idx++, A.mIdxs.fid);
    // nzir
    il.add_region_requirement(
        RegionRequirement(A.nzir.lp(), 0, READ_ONLY, EXCLUSIVE, A.nzir.lr)
    );
    il.add_field(idx++, A.nzir.fid);
    // x's regions /////////////////////////////////////////////////////////////
    il.add_region_requirement(
        /* notice we are using the entire region here */
        // FIXME coherence ???
        RegionRequirement(x.lr, 0, READ_WRITE, ATOMIC, x.lr)
    );
    il.add_field(idx++, x.fid);
    // r's regions /////////////////////////////////////////////////////////////
    il.add_region_requirement(
        RegionRequirement(r.lp(), 0, READ_ONLY, EXCLUSIVE, r.lr)
    );
    il.add_field(idx++, r.fid);
    // execute the thing...
    (void)lrt->execute_index_space(ctx, il);
    } // end for
}

/**
 * computes: symgs for Ax = r
 */
inline void
symgsTask(const LegionRuntime::HighLevel::Task *task,
          const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
          LegionRuntime::HighLevel::Context ctx,
          LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using LegionRuntime::Arrays::Rect;

    // A (x4), x, b
    assert(6 == rgns.size());
    size_t rid = 0;
    CGTaskArgs targs = *(CGTaskArgs *)task->local_args;
    int sweep = *(int *)task->args;
#if 0 // nice debug
    printf("%d: sub-grid bounds: (%d) to (%d)\n",
            getTaskID(task), rect.lo.x[0], rect.hi.x[0]);
#endif
    // name the regions
    // spare matrix regions
    const PhysicalRegion &avpr = rgns[rid++];
    const PhysicalRegion &adpr = rgns[rid++];
    const PhysicalRegion &aipr = rgns[rid++];
    const PhysicalRegion &azpr = rgns[rid++];
    // vector regions
    const PhysicalRegion &xpr  = rgns[rid++];
    const PhysicalRegion &rpr  = rgns[rid++];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>  GDRA;
    typedef RegionAccessor<AccessorType::Generic, int64_t> GLRA;
    typedef RegionAccessor<AccessorType::Generic, uint8_t> GSRA;
    // sparse matrix
    GDRA av = avpr.get_field_accessor(targs.sa.vals.fid).typeify<double>();
    GDRA ad = adpr.get_field_accessor(targs.sa.diag.fid).typeify<double>();
    GLRA ai = aipr.get_field_accessor(targs.sa.mIdxs.fid).typeify<int64_t>();
    GSRA az = azpr.get_field_accessor(targs.sa.nzir.fid).typeify<uint8_t>();
    // vectors
    GDRA x = xpr.get_field_accessor(targs.va.fid).typeify<double>();
    GDRA r = rpr.get_field_accessor(targs.vb.fid).typeify<double>();

    Rect<1> avsr; ByteOffset avOff[1];
    Rect<1> myGridBounds = targs.sa.vals.sgb;
    // calculate nRows and nCols for the local subgrid
    assert(0 == myGridBounds.volume() % targs.sa.nCols);
    int64_t lNRows = myGridBounds.volume() / targs.sa.nCols;
    int64_t lNCols = targs.sa.nCols;
    double *avp = av.raw_rect_ptr<1>(myGridBounds, avsr, avOff);
    bool offd = offsetsAreDense<1, double>(myGridBounds, avOff);
    assert(offd);
    // remember that vals and mIdxs should be the same size
    Rect<1> aisr; ByteOffset aiOff[1];
    int64_t *aip = ai.raw_rect_ptr<1>(myGridBounds, aisr, aiOff);
    offd = offsetsAreDense<1, int64_t>(myGridBounds, aiOff);
    assert(offd);
    // diag and nzir are smaller (by a stencil size factor).
    Rect<1> adsr; ByteOffset adOff[1];
    myGridBounds = targs.sa.diag.sgb;
    double *adp = ad.raw_rect_ptr<1>(myGridBounds, adsr, adOff);
    offd = offsetsAreDense<1, double>(myGridBounds, adOff);
    assert(offd);
    // remember nzir and diag are the same length
    myGridBounds = targs.sa.nzir.sgb;
    uint8_t *azp = az.raw_rect_ptr<1>(myGridBounds, adsr, adOff);
    offd = offsetsAreDense<1, uint8_t>(myGridBounds, adOff);
    assert(offd);
    // x
    Rect<1> xsr; ByteOffset xOff[1];
    // notice that we aren't using the subgridBounds here -- need all of x
    myGridBounds = targs.va.bounds;
    double *xp = x.raw_rect_ptr<1>(myGridBounds, xsr, xOff);
    offd = offsetsAreDense<1, double>(myGridBounds, xOff);
    assert(offd);
    // r
    Rect<1> rsr; ByteOffset rOff[1];
    myGridBounds = targs.vb.sgb;
    const double *const rp = r.raw_rect_ptr<1>(myGridBounds, rsr, rOff);
    offd = offsetsAreDense<1, double>(myGridBounds, rOff);
    assert(offd);
    // now, actually perform the computation
    // forward sweep
    if (1 == sweep) {
        for (int64_t i = 0; i < lNRows; ++i) {
            // get to base of next row of values
            const double *const cVals = (avp + (i * lNCols));
            // get to base of next row of "real" indices of values
            const int64_t *const cIndx = (aip + (i * lNCols));
            // capture how many non-zero values are in this particular row
            const int64_t cnnz = azp[i];
            // current diagonal value
            const double curDiag = adp[i];
            // RHS value
            double sum = rp[i];
            for (int64_t j = 0; j < cnnz; ++j) {
                int64_t curCol = cIndx[j];
                sum -= cVals[j] * xp[curCol];
            }
            sum += xp[i] * curDiag; // remove diagonal contribution from previous loop
            xp[i] = sum / curDiag;
        }
    }
    else {
        // back sweep
        for (int64_t i = lNRows - 1; i >= 0; --i) {
            // get to base of next row of values
            const double *const cVals = (avp + (i * lNCols));
            // get to base of next row of "real" indices of values
            const int64_t *const cIndx = (aip + (i * lNCols));
            // capture how many non-zero values are in this particular row
            const int64_t cnnz = azp[i];
            // current diagonal value
            const double curDiag = adp[i];
            // RHS value
            double sum = rp[i]; // RHS value
            for (int64_t j = 0; j < cnnz; ++j) {
                int64_t curCol = cIndx[j];
                sum -= cVals[j] * xp[curCol];
            }
            sum += xp[i] * curDiag; // remove diagonal contribution from previous loop
            xp[i] = sum / curDiag;
        }
    }
}

}

#endif