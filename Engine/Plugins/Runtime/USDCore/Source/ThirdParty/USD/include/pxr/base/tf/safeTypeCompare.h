//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_BASE_TF_SAFE_TYPE_COMPARE_H
#define PXR_BASE_TF_SAFE_TYPE_COMPARE_H

/// \file tf/safeTypeCompare.h
/// \ingroup group_tf_RuntimeTyping
/// Safely compare C++ RTTI type structures.

#include "pxr/pxr.h"

#include <cstring>
#include <typeinfo>

PXR_NAMESPACE_OPEN_SCOPE

/// Safely compare \c std::type_info structures.
///
/// Returns \c true if \p t1 and \p t2 denote the same type.
inline bool TfSafeTypeCompare(const std::type_info& t1, const std::type_info& t2) {
// XXX(Epic Games):
#if PLATFORM_WINDOWS || PLATFORM_MAC
    return t1 == t2;
#else
    // clang's type comparison doesn't work the same way as gcc's type
    // comparison, and using the equal operator with clang on typeids from two
    // different shared libraries will return that the types are not the same.
    // This leads to USD errors stating that "typex" isn't "typex". To address
    // this, we replace the direct type_info comparison with a comparison by
    // type name instead, which is how operator== is implemented with gcc.
    // See for reference:
    //     https://github.com/PixarAnimationStudios/USD/issues/665
    //     https://github.com/PixarAnimationStudios/USD/issues/1475
    //
    // Note that since this is applied to one of OpenUSD's own headers,
    // this file is used when building both OpenUSD and Unreal Engine. We
    // want to use the string comparison when building OpenUSD itself
    // (where no "PLATFORM_..." definitions will exist) as well as when
    // building UE for Linux, but otherwise use the direct type comparison
    // when building UE for Windows or Mac.
    return std::strcmp(t1.name(), t2.name()) == 0;
#endif // PLATFORM_WINDOWS || PLATFORM_MAC
}

/// Safely perform a dynamic cast.
///
/// Usage should mirror regular \c dynamic_cast:
/// \code
///     Derived* d = TfSafeDynamic_cast<Derived*>(basePtr);
/// \endcode
///  
/// Note that this function also works with \c TfRefPtr and \c TfWeakPtr
/// managed objects.
template <typename TO, typename FROM>
TO
TfSafeDynamic_cast(FROM* ptr) {
        return dynamic_cast<TO>(ptr);
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_BASE_TF_SAFE_TYPE_COMPARE_H
