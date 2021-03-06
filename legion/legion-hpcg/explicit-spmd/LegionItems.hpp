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

#include "LegionStuff.hpp"
#include "Geometry.hpp"

#include <cassert>
#include <deque>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct LogicalItemBase {
public:
    // Field ID.
    Legion::FieldID fid = 0;
    // Logical region that represents item.
    Legion::LogicalRegion logicalRegion;
    // Launch domain.
    Legion::Domain launchDomain;
    // Index partition.
    Legion::IndexPartition indexPartition;
    // Logical partition.
    Legion::LogicalPartition logicalPartition;
    // Parent logical region (if set).
    LogicalRegion parentLogicalRegion;

protected:
    // Name we attach to item.
    std::string mName;
    //
    bool mHasParentLogicalRegion = false;

public:

    /**
     *
     */
    virtual void
    deallocate(
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) = 0;

    /**
     *
     */
    void
    intent(
        Legion::PrivilegeMode privMode,
        Legion::CoherenceProperty cohProp,
        int shard,
        Legion::TaskLauncher &launcher,
        LegionRuntime::HighLevel::Context ctx,
        LegionRuntime::HighLevel::HighLevelRuntime *lrt
    ) {
        auto lsr = lrt->get_logical_subregion_by_color(
            ctx, logicalPartition, shard
        );
        launcher.add_region_requirement(
            RegionRequirement(
                lsr,
                privMode,
                cohProp,
                logicalRegion
            )
        ).add_field(fid);
    }

    /**
     *
     */
    virtual void
    partition(
        size_t nParts,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) { /* Nothing to do. */ }

    /**
     *
     */
    void
    setParentLogicalRegion(
        const LogicalRegion &parent
    ) {
        parentLogicalRegion = parent;
        mHasParentLogicalRegion = true;
    }

    /**
     *
     */
    bool
    hasParentLogicalRegion(void) { return mHasParentLogicalRegion; }

    /**
     *
     */
    LogicalRegion
    getParentLogicalRegion(void) { return parentLogicalRegion; }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * Base class for logical structures that contain multiple logical structures.
 */
struct LogicalMultiBase {
    // Launch domain
    LegionRuntime::HighLevel::Domain launchDomain;

protected:

    std::deque<LogicalItemBase *> mLogicalItems;

    /**
     *
     */
    LogicalMultiBase(void) = default;

    /**
     *
     */
    virtual void
    mPopulateRegionList(void) = 0;

public:

    /**
     *
     */
    virtual void
    allocate(
        const std::string &name,
        const Geometry &geom,
        LegionRuntime::HighLevel::Context ctx,
        LegionRuntime::HighLevel::HighLevelRuntime *lrt
    ) = 0;

    /**
     *
     */
    virtual void
    partition(
        int64_t nParts,
        LegionRuntime::HighLevel::Context ctx,
        LegionRuntime::HighLevel::HighLevelRuntime *lrt
    ) = 0;

    /**
     * Cleans up and returns all allocated resources.
     */
    virtual void
    deallocate(
        LegionRuntime::HighLevel::Context ctx,
        LegionRuntime::HighLevel::HighLevelRuntime *lrt
    ) {
        for (auto *i : mLogicalItems) {
            i->deallocate(ctx, lrt);
        }
    }

    /**
     *
     */
    virtual void
    intent(
        Legion::PrivilegeMode privMode,
        Legion::CoherenceProperty cohProp,
        int shard,
        Legion::TaskLauncher &launcher,
        Context ctx,
        HighLevelRuntime *lrt
    ) {
        for (auto &a : mLogicalItems) {
            a->intent(privMode, cohProp, shard, launcher, ctx, lrt);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TYPE>
struct LogicalItem : public LogicalItemBase {

    /**
     *
     */
    LogicalItem(void) : LogicalItemBase() { }

    /**
     * Instantiate a LogicalItem from a LogicalRegion.
     */
    LogicalItem(
        const LogicalRegion &lr,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) : LogicalItemBase()
    {
        //fid
        logicalRegion = lr;
        //launchDomain
        //logicalPartition
        mBounds       = lrt->get_index_space_domain(
                            ctx, lr.get_index_space()
                        ).get_rect<1>();
        //
        mLength       = mBounds.volume();
        mIndexSpace   = lr.get_index_space();
        mFS           = lr.get_field_space();
        mIndexSpaceID = mIndexSpace.get_id();
        mFieldSpaceID = mFS.get_id();
        mRTreeID      = lr.get_tree_id();
    }

private:
    // The vector rectangle bounds.
    LegionRuntime::Arrays::Rect<1> mBounds;

protected:
    // Number of elements stored in the item (the entire extent).
    int64_t mLength = 0;
    // Index space.
    Legion::IndexSpace mIndexSpace;
    // Field space.
    Legion::FieldSpace mFS;
    // The following are used for vector equality tests. That is, equality in
    // the "are these vectors the same from legion's perspective."
    Legion::IndexSpaceID mIndexSpaceID;
    //
    Legion::FieldSpaceID mFieldSpaceID;
    //
    Legion::RegionTreeID mRTreeID;

    /**
     *
     */
    virtual void
    mAttachNameAtAllocate(
        LegionRuntime::HighLevel::Context ctx,
        LegionRuntime::HighLevel::HighLevelRuntime *lrt
    ) {
        using namespace std;
        //
        const char *isName = mName.c_str();
        const char *fsName = mName.c_str();
        const char *lrName = mName.c_str();
        //
        lrt->attach_name(mIndexSpace,   isName);
        lrt->attach_name(mFS,           fsName);
        lrt->attach_name(logicalRegion, lrName);
    }

    /**
     *
     */
    virtual void
    mAttachNameAtPartition(
        LegionRuntime::HighLevel::Context ctx,
        LegionRuntime::HighLevel::HighLevelRuntime *lrt
    ) {
        using namespace std;
        //
        const char *lpName = mName.c_str();
        const char *ipName = mName.c_str();
        //
        lrt->attach_name(logicalPartition, lpName);
        lrt->attach_name(indexPartition, ipName);
    }

    /**
     *
     */
    void
    mAllocate(
        const std::string &name,
        int64_t len,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        mLength = len;
        // Calculate the size of the logicalRegion vec (inclusive).
        const size_t n = mLength - 1;
        // Item rect.
        mBounds = Rect<1>(Point<1>::ZEROES(), Point<1>(n));
        // Item domain.
        Domain dom(Domain::from_rect<1>(mBounds));
        // Item index space.
        mIndexSpace = lrt->create_index_space(ctx, dom);
        // Item field space.
        mFS = lrt->create_field_space(ctx);
        // Item field allocator.
        FieldAllocator fa = lrt->create_field_allocator(ctx, mFS);
        // All elements are going to be of size T.
        fa.allocate_field(sizeof(TYPE), fid);
        // Create the logical region.
        logicalRegion = lrt->create_logical_region(ctx, mIndexSpace, mFS);
        // Stash some info for equality checks.
        mIndexSpaceID = logicalRegion.get_index_space().get_id();
        mFieldSpaceID = logicalRegion.get_field_space().get_id();
        mRTreeID      = logicalRegion.get_tree_id();
        //
        mName = name;
        mAttachNameAtAllocate(ctx, lrt);
    }

public:
    //
    PhysicalRegion physicalRegion;

    /**
     *
     */
    void
    allocate(
        const std::string &name,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        mAllocate(name, 1, ctx, lrt);
    }

    /**
     * Cleans up and returns all allocated resources.
     */
    void
    deallocate(
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        lrt->destroy_index_space(ctx, mIndexSpace);
        lrt->destroy_field_space(ctx, mFS);
        lrt->destroy_logical_region(ctx, logicalRegion);
    }

    /**
     * Returns whether or not two LogicalItems are the same (as far as the
     * Legion RT is concerned).
     */
    static bool
    same(
        const LogicalItem &a,
        const LogicalItem &b
    ) {
        return a.mIndexSpaceID == b.mIndexSpaceID &&
               a.mFieldSpaceID == b.mFieldSpaceID &&
               a.mRTreeID      == b.mRTreeID;
    }

    /**
     *
     */
    Legion::PhysicalRegion
    mapRegion(
        Legion::PrivilegeMode privMode,
        Legion::CoherenceProperty cohProp,
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        RegionRequirement req(
            logicalRegion, privMode, cohProp, logicalRegion
        );
        req.add_field(fid);
        //
        InlineLauncher inl(req);
        physicalRegion = lrt->map_region(ctx, inl);
        physicalRegion.wait_until_valid();
        //
        return physicalRegion;
    }

    /**
     *
     */
    void
    unmapRegion(
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) {
        lrt->unmap_region(ctx, physicalRegion);
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TYPE>
class Item {
protected:
    //
    size_t mLength = 0;
    //
    TYPE *mData = nullptr;

public:
    //
    LogicalRegion logicalRegion;
    //
    PhysicalRegion physicalRegion;
    // Field ID (only one, so it doesn't change).
    const Legion::FieldID fid = 0;

    /**
     *
     */
    Item(
        const PhysicalRegion &physicalReg,
        Context ctx,
        HighLevelRuntime *runtime
    ) {
        // Cache logical and physical regions.
        physicalRegion = physicalReg;
        logicalRegion = physicalRegion.get_logical_region();
        //
        using GRA = RegionAccessor<AccessorType::Generic, TYPE>;
        GRA tAcc = physicalRegion.get_field_accessor(0).template typeify<TYPE>();
        //
        Domain tDom = runtime->get_index_space_domain(
            ctx, physicalRegion.get_logical_region().get_index_space()
        );
        Rect<1> subrect;
        ByteOffset inOffsets[1];
        auto subGridBounds = tDom.get_rect<1>();
        mLength = subGridBounds.volume();
        //
        mData = tAcc.template raw_rect_ptr<1>(
            subGridBounds, subrect, inOffsets
        );
        // Sanity.
        if (!mData || (subrect != subGridBounds) ||
            !offsetsAreDense<1, TYPE>(subGridBounds, inOffsets)) {
            // Signifies that something went south.
            mData = nullptr;
        }
        // It's all good...
    }

    /**
     *
     */
    TYPE *
    data(void) { return mData; }

    /**
     *
     */
    const TYPE *
    data(void) const { return mData; }

    /**
     *
     */
    FieldID
    getFieldID(void) { return 0; }

    /**
     *
     */
    int64_t
    getGlobalIdxZero(
        Context ctx,
        HighLevelRuntime *lrt
    ) {
        Rect<1> rect = lrt->get_index_space_domain(
            ctx,
            logicalRegion.get_index_space()
        ).template get_rect<1>();
        //
        return rect.lo.x[0];
    }

    /**
     *
     */
    void
    intent(
        Legion::PrivilegeMode privMode,
        Legion::CoherenceProperty cohProp,
        Legion::TaskLauncher &launcher,
        Context ctx,
        Runtime *lrt
    ) {
        launcher.add_region_requirement(
            RegionRequirement(
                logicalRegion,
                privMode,
                cohProp,
                logicalRegion
            )
        ).add_field(fid);
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct PhysicalMultiBase {
protected:
    // Number of region entries.
    size_t mNRegionEntries = 0;
    // Flags passed to unpack.
    ItemFlags mUnpackFlags = 0;

    /**
     * MUST MATCH PACK ORDER IN mPopulateRegionList!
     */
    virtual void
    mUnpack(
        const std::vector<PhysicalRegion> &regions,
        size_t baseRID,
        ItemFlags iFlags,
        Context ctx,
        HighLevelRuntime *rt
    ) = 0;

public:

    /**
     *
     */
    size_t
    nRegionEntries(void) { return mNRegionEntries; }

    /**
     *
     */
    virtual void
    unmapRegions(
        Legion::Context ctx,
        Legion::HighLevelRuntime *lrt
    ) { /* Nothing to do. */ }
};
