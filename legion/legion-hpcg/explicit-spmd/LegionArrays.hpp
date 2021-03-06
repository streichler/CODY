/*
 * Copyright (c) 2014-2017 Los Alamos National Security, LLC
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
 */

#pragma once

#include "LegionItems.hpp"

#include <deque>
#include <vector>
#include <climits>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TYPE>
struct LogicalArray : public LogicalItem<TYPE> {
public:

    /**
     *
     */
    LogicalArray(void) : LogicalItem<TYPE>() { }

    /**
     * Instantiate from a LogicalRegion.
     */
    LogicalArray(
        const LogicalRegion &lr,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) : LogicalItem<TYPE>(lr, ctx, lrt) { }

    /**
     *
     */
    virtual
    ~LogicalArray(void) = default;

    /**
     *
     */
    void
    allocate(
        const std::string &name,
        int64_t nElems,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        this->mAllocate(name, nElems, ctx, lrt);
    }

    /**
     *
     */
    void
    partition(
        size_t nParts,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        // Only allow even partitioning.
        assert(0 == this->mLength % nParts && "Uneven partitioning requested.");
        //
        int64_t inc = this->mLength / nParts; // the increment
        Rect<1> colorBounds(Point<1>(0), Point<1>(nParts - 1));
        Domain colorDomain = Domain::from_rect<1>(colorBounds);
        //          +
        //          |
        //          |
        //     (x1)-+-+
        //          | |
        //          | m / nSubregions
        //     (x0) + |
        size_t x0 = 0, x1 = inc - 1;
        DomainColoring disjointColoring;
        // a list of sub-grid bounds.
        // provides a task ID to sub-grid bounds mapping.
        std::vector< Rect<1> > subGridBounds;
        for (size_t color = 0; color < nParts; ++color) {
            Rect<1> subRect((Point<1>(x0)), (Point<1>(x1)));
            // cache the subgrid bounds
            subGridBounds.push_back(subRect);
#if 0 // Debug.
            printf("vec disjoint partition: (%d) to (%d)\n",
                    subRect.lo.x[0], subRect.hi.x[0]);
#endif
            disjointColoring[color] = Domain::from_rect<1>(subRect);
            x0 += inc;
            x1 += inc;
        }
        this->indexPartition = lrt->create_index_partition(
            ctx,
            this->mIndexSpace,
            colorDomain,
            disjointColoring,
            true,
            0 /* partition color */
        );
        // Logical partitions.
        this->logicalPartition = lrt->get_logical_partition(
            ctx,
            this->logicalRegion,
            this->indexPartition
        );
        // Launch domain -- one task per color.
        this->launchDomain = colorDomain;
        //
        this->mAttachNameAtPartition(ctx, lrt);
    }

    /**
     *
     */
    void
    partition(
        const std::vector<local_int_t> &partLens,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        const size_t nParts = partLens.size();
        Rect<1> colorBounds(Point<1>(0), Point<1>(nParts - 1));
        Domain colorDomain = Domain::from_rect<1>(colorBounds);
        //
        size_t x0 = 0, x1 = 0;
        DomainColoring disjointColoring;
        // Provides a task ID to sub-grid bounds mapping.
        std::vector< Rect<1> > subGridBounds;
        for (size_t color = 0; color < nParts; ++color) {
            x1 = x0 + partLens[color] - 1;
            Rect<1> subRect((Point<1>(x0)), (Point<1>(x1)));
            // Cache the subgrid bounds.
            subGridBounds.push_back(subRect);
#if 0 // Debug.
            printf("vec len=%ld\n", (long)partLens[color]);
            printf("vec disjoint partition: (%ld) to (%ld)\n",
                    (long)subRect.lo.x[0], (long)subRect.hi.x[0]);
#endif
            disjointColoring[color] = Domain::from_rect<1>(subRect);
            // Slide window.
            x0 += partLens[color];
        }
        this->indexPartition = lrt->create_index_partition(
            ctx,
            this->mIndexSpace,
            colorDomain,
            disjointColoring,
            true /* disjoint */,
            0 /* partition color */
        );
        // Logical partitions.
        this->logicalPartition = lrt->get_logical_partition(
            ctx,
            this->logicalRegion,
            this->indexPartition
        );
        // Launch domain -- one task per color.
        this->launchDomain = colorDomain;
        //
        this->mAttachNameAtPartition(ctx, lrt);
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TYPE>
struct Array : public Item<TYPE> {
    //
    std::vector<LogicalArray<TYPE> *> ghosts;

    /**
     *
     */
    Array(
        const PhysicalRegion &physicalRegion,
        Context ctx,
        HighLevelRuntime *runtime
    ) : Item<TYPE>(physicalRegion, ctx, runtime) { }

    /**
     *
     */
    ~Array(void) {
        for (auto *i : ghosts) {
            delete i;
        }
    }

    /**
     *
     */
    size_t
    length(void) const { return this->mLength; }

    /**
     *
     */
    bool
    hasGhosts(void) { return (ghosts.size() > 0); }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * Interprets 1D array as NxM 2D array.
 */
template<typename TYPE>
class Array2D {

protected:
    //
    local_int_t mNRows = 0;
    //
    local_int_t mNCols = 0;
    //
    TYPE *const mBasePtr = nullptr;

public:
    /**
     *
     */
    Array2D(void) = default;

    /**
     *
     */
    ~Array2D(void) {
        mNRows = 0;
        mNCols = 0;
        // Don't free memory here because we don't know what allocated that
        // memory. Assume that it'll get cleaned up another way.
    }

    /**
     *
     */
    Array2D(
        size_t nRows,
        size_t nCols,
        TYPE *basePtr
    ) : mNRows(nRows)
      , mNCols(nCols)
      , mBasePtr(basePtr) { }

    /**
     *
     */
    TYPE &
    operator()(local_int_t row, local_int_t col)
    {
        return mBasePtr[(row * mNCols) + col];
    }

    /**
     *
     */
    TYPE *
    operator()(local_int_t row)
    {
        return (mBasePtr + (row * mNCols));
    }
};
