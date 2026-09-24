// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <arrayview/ArrayView.h>
#include <arrayview/StringView.h>
#include <pma/MemoryResource.h>
#include <pma/ScopedPtr.h>
#include <status/Status.h>
#include <status/StatusCode.h>
#include <trio/Stream.h>
#include <trio/streams/FileStream.h>

namespace raf {

using trio::BoundedIOStream;
using trio::FileStream;
using sc::Status;
using sc::StatusCode;

using namespace av;
using namespace pma;

}  // namespace raf
