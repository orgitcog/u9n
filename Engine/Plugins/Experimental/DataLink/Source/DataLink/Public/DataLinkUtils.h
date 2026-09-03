// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointerFwd.h"

class IDataLinkSinkProvider;
class UClass;
class UObject;
struct FDataLinkSink;
template <typename T> class TScriptInterface;

namespace UE::DataLink
{
	/**
	 * Replaces a given Object with a new object with the same name but different class
	 * @param InOutObject object to replace. Can come in as null
	 * @param InOuter outer of the object. Must be valid.
	 * @param InClass new class of the new object that will replace the older one
	 * @return true if the operation took place, false otherwise
	 */
	DATALINK_API bool ReplaceObject(UObject*& InOutObject, UObject* InOuter, UClass* InClass);

	/**
	 * Attempts to get the underlying Sink from the given Sink Provider
	 * @param InSinkProvider the sink provider interface
	 * @return the sink if found
	 */
	DATALINK_API TSharedPtr<FDataLinkSink> TryGetSink(TScriptInterface<IDataLinkSinkProvider> InSinkProvider);
}
