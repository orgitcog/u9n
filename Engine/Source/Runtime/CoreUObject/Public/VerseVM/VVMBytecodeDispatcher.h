// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/UnrealTemplate.h"
#include "VVMBytecode.h"
#include "VVMBytecodeOps.h"
#include "VVMBytecodesAndCaptures.h"
#include "VVMLog.h"
#include "VVMProcedure.h"

namespace Verse
{
template <typename HandlerType>
FOp* DispatchOp(FOp* Op, HandlerType&& Handler)
{
	switch (Op->Opcode)
	{
#define VISIT_OP(Name)                         \
	case EOpcode::Name:                        \
		Handler(*static_cast<FOp##Name*>(Op)); \
		return static_cast<FOp##Name*>(Op) + 1;

		VERSE_ENUM_OPS(VISIT_OP)

#undef VISIT_OP
	}
	V_DIE("Invalid opcode: %u", static_cast<FOpcodeInt>(Op->Opcode));
}

template <typename HandlerType>
void DispatchOps(FOp* OpsBegin, FOp* OpsEnd, HandlerType&& Handler)
{
	FOp* Op = OpsBegin;
	while (Op < OpsEnd)
	{
		Op = DispatchOp(Op, Forward<HandlerType>(Handler));
	}
}

// HandlerType can take in an FOp& or an auto& and descriminate on the actual
// subtype of FOp being passed in.
template <typename HandlerType>
void DispatchOps(VProcedure& Procedure, HandlerType&& Handler)
{
	DispatchOps(Procedure.GetOpsBegin(), Procedure.GetOpsEnd(), Forward<HandlerType>(Handler));
}

} // namespace Verse
#endif // WITH_VERSE_VM
