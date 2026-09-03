// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/StringFwd.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Optional.h"
#include "VVMContext.h"
#include "VVMCppClassInfo.h"
#include "VVMHeap.h"

#include <atomic>

namespace Verse
{
enum class ECompares : uint8;
enum class EValueStringFormat;
struct FAbstractVisitor;
struct FMarkStackVisitor;
struct FStrongCellRegistry;
struct VEmergentType;
struct FOpResult;

/// An object managed by the Verse GC. Represents both Verse VM-internal data structures and user-facing values.
struct VCell
{
	// If set in GCData, means that this object has an object in the libpas verse_heap client_data for the verse_heap_page_header
	// corresponding to this object. This just means that during marking, we want to process that data structure.
	static constexpr uint8 GCDataIsWeakKeyBit = 1;

	DECLARE_BASE_VCPPCLASSINFO(COREUOBJECT_API);

	/// The header word of a VCell is the offset of an emergent type and 4 extra bytes
	// (one reserved for GC)
	uint32 EmergentTypeOffset;
	std::atomic<uint8> GCData{0};
	// The first two bits of this are used by FExternalMutexes in VCell subclasses.
	std::atomic<uint8> Mutex{0};
	// The first bit of Misc2 being set indicates this cell is deeply mutable.
	static constexpr uint8 DeeplyMutableTag = 1;
	// The second bit of Misc2 being set indicates this cell is a subobject of a CDO.
	// TODO(SOL-7928): Remove this bit. It is a hack for BPVM compatibility.
	static constexpr uint8 ArchetypeTag = 2;
	union
	{
		struct
		{
			uint8_t Misc2;
			uint8_t Misc3;
		};
		uint16 Misc2And3{0};
	};

	VCell(const VCell&) = delete;
	VCell& operator=(const VCell&) = delete;

	COREUOBJECT_API VCell(FAllocationContext Context, const VEmergentType* EmergentType);

	VEmergentType* GetEmergentType() const;
	const VCppClassInfo* GetCppClassInfo() const;

	// FIXME: In the future maybe these will take a FRunningContext or FAccessContext rather than
	// the MarkStack or nothing. That's because:
	// 1) Nothing wrong with saying that the GC threads allocate or run arbitrary code if the GC is
	//    concurrent by design. Like, maybe we'll want GC-time hash-consing.
	// 2) In a parallel GC, we'll probably want to just reuse the fact that each context has a
	//    MarkStack.
	COREUOBJECT_API void VisitReferences(FMarkStackVisitor& Visitor);
	COREUOBJECT_API void VisitReferences(FAbstractVisitor& Visitor);
	COREUOBJECT_API void ConductCensus();
	COREUOBJECT_API void RunDestructor();

	void AddRef(FAccessContext Context); // Temporarily prevent this cell from being GC'ed
	void ReleaseRef();

	COREUOBJECT_API ECompares Equal(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);
	COREUOBJECT_API VValue Melt(FAllocationContext Context);
	COREUOBJECT_API FOpResult Freeze(FAllocationContext Context);
	COREUOBJECT_API bool Subsumes(FAllocationContext Context, VValue);
	COREUOBJECT_API void VisitMembers(FAllocationContext Context, FDebuggerVisitor& Visitor);
	COREUOBJECT_API void AppendToString(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0);
	COREUOBJECT_API FUtf8String ToString(FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0);
	COREUOBJECT_API void Serialize(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);
	COREUOBJECT_API void Serialize(FAllocationContext Context, FArchive& Ar);

	bool IsDeeplyMutable() const { return Misc2 & DeeplyMutableTag; }
	bool SetIsDeeplyMutable() { return Misc2 |= DeeplyMutableTag; }

	COREUOBJECT_API static void InitializeGlobals(FAllocationContext Context);

private:
	// Use this if your cell subtype has any outgoing strong references.  It is used by both the
	// GC system and the abstract visitor to collect strong references.
	//
	// Note that this function will run concurrently to the mutator (this function is being called in
	// some collector thread while the VM's other threads are calling other methods on your object)
	// and in parallel (the collector will run multiple threads calling this function). However,
	// you're guaranteed that this function will only be called once per object per collection cycle;
	// i.e. the GC will never call this function simultaneously for the same object.
	//
	// It is defined implicitly by DECLARE_DERIVED_VCPPCLASSINFO, and so you must either implement it
	// or use the DEFINE_TRIVIAL_VISIT_REFERENCES macro to explicitly define a trivial implementation.
	//
	// Visit outgoing references for self. Don't call visit on your super class, or visit references
	// defined by your super class.
	template <typename TVisitor>
	void VisitReferencesImpl(TVisitor&);

	COREUOBJECT_API static TNeverDestroyed<TOptional<FStrongCellRegistry>> GlobalStrongCellRegistry;

protected:
	// Override this if your cell subtype has any outgoing weak references. Call ClearWeakDuringCensus
	// on those pointers in this function.
	//
	// Note that this function may run concurrently to the mutator, in parallel, or from within the
	// mutator's allocation slow paths. This means that this function cannot allocate, since it may be
	// called while internal allocator locks are held. It may also be called while mutator locks are
	// held, if those locks are held across allocations. So, this function cannot acquire locks,
	// unless those locks are never held while either handshaking or allocating.
	//
	// This function is called exactly once per collection cycle for any marked objects that belong to
	// the CensusSpace or the DestructorAndCensusSpace. This function is guaranteed to be called only
	// after all marking is finished, so you can use FHeap::IsMarked and that will tell you if the
	// object survived the collection or not. Because census runs before destruction and sweep, you're
	// also guaranteed that any pointed-to dead objects are still fully intact and usable.
	//
	// It's not meaningful to override this function unless the cell is allocated from the CensusSpace
	// or the DestructorAndCensusSpace.
	COREUOBJECT_API void ConductCensusImpl();

	// Override this if your cell requries deep comparison (simple comparisons should be inlined in VValue::Equal).
	COREUOBJECT_API ECompares EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	// Override this if your cell subtype requires a deep hash.
	COREUOBJECT_API uint32 GetTypeHashImpl();

	// Override this if your cell requires deep copying.
	//
	// Note: This also acts as "Clone"  for mutable types which is defined as 'Freeze->Melt'.
	// We can ignore the possibility that the skipped 'freeze' could error on placeholders
	// as the mutable data we are operating on requires all values be concrete on creation
	// or we would've suspended.
	COREUOBJECT_API VValue MeltImpl(FAllocationContext Context);

	// Override this if your cell is a mutable representation and requires deep copying.
	COREUOBJECT_API FOpResult FreezeImpl(FAllocationContext Context);

	// Override this if your cell represents a type that can be used in casts.
	COREUOBJECT_API bool SubsumesImpl(FAllocationContext, VValue);

	// Override this if your cell has members that should be visible in the debugger.
	COREUOBJECT_API void VisitMembersImpl(FAllocationContext, FDebuggerVisitor&);

	// Override this to customize how your cell is displayed, in the debugger and elsewhere.
	COREUOBJECT_API void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	// Override these if your cell is serializable.
	//
	// SerializeIdentity means that a cell's identity must be preserved through serialization,
	// particularly when imported by another package. When it is false, the cell may be duplicated
	// into multiple packages (and potentially deduplicated by SerializeLayout on load.)
	//
	// SerializeLayout should serialize just enough to allocate a deserialized cell - for example,
	// the length of a trailing array. SerializeImpl should then serialize the remaining fields.

	static constexpr bool SerializeIdentity = true;

	template <typename CellType>
	static void SerializeLayout(FAllocationContext, CellType*&, FStructuredArchiveVisitor&)
	{
		V_DIE("VCell subtype without `SerializeLayout` override called!");
	}

	COREUOBJECT_API void SerializeImpl(FAllocationContext, FStructuredArchiveVisitor&);

	// VCell() and SetEmergentType(..) are used during setup when creating some cyclic dependencies.
	VCell()
		: EmergentTypeOffset(0)
	{
		checkSlow(FHeap::OwnsAddress(this));
	}

public:
	void SetEmergentType(FAccessContext, VEmergentType* EmergentType);

	// Override this if your cell subtype has a destructor.
	//
	// Note that this function may run concurrently to the mutator, in parallel, or from within the
	// mutator's allocation slow paths. This means that this function cannot allocate, since it may be
	// called while internal allocator locks are held. It may also be called while mutator locks are
	// held, if those locks are held across allocations. So, this function cannot acquire locks,
	// unless those locks are never held while either handshaking or allocating.
	//
	// This function is called exactly once per collection cycle for any unmarked objects that belong
	// to the DestructorSpace or the DestructorAndCensusSpace. This function is guaranteed to be called
	// only after all marking and census are finished. Because census doesn't cover unmarked objects,
	// you will see nonnull weak references to dead objects. Because destruction runs before sweep,
	// you're also guaranteed that any pointed-to dead objects are still fully intact and usable.
	//
	// It's not meaningful to override this function unless the cell is allocated from the
	// DestructorSpace or the DestructorAndCensusSpace.
	//
	// Note: You must override this if your cell has external memory to report swept external
	// bytes during destruction.
	~VCell() = default;

	template <typename CastType>
	bool IsA() const;

	template <typename CastType>
	const CastType& StaticCast() const;

	template <typename CastType>
	CastType& StaticCast();

	template <typename CastType>
	CastType* DynamicCast();

	template <typename CastType>
	CastType* DynamicCast() const;

	COREUOBJECT_API FString DebugName() const;

	// Inform the GC that this cell is now a key in the following weak map and it keeps the given value alive.
	// For sound concurrent GC handling of weak maps, both the map and the key must know about the mapping.
	// Hence, every cell in the VVM has the secret ability to become a weak key. This costs just one bit per
	// object if unused. It costs horrors and nightmares if used (but at least the terror is O(1)ish).
	void AddWeakMapping(VCell* Map, VCell* Value);
	void RemoveWeakMapping(VCell* Map);

	// This function is a test-only function because it has a very limited kind of meaning. Requesting the size (or
	// checking the emptiness) of a weap map (including a "transposed" weak map, like the weak key map) gives a kind
	// of upper bound: it means that the map has at most this many entries. But we cannot tell you which of those
	// entries are real. When you query them, you are likely to find fewer entries.
	bool HasWeakMappings();
};

static_assert(sizeof(VCell) <= 8);

/// `VHeapValue` represents Verse-facing values, while `VCell` represents VM-internal structures.
// To be or not to be ...
// Keep it here for now.
struct VHeapValue : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);

	using VCell::VCell;
};

} // namespace Verse
#endif // WITH_VERSE_VM
