// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class IAsyncPackageLoader;

/**
 * Creates a new instance of the Transactionally Safe Async Package Loader.
 *
 * @param WrappedPackageLoader The underlying package loader to use when not in a transaction.
 *
 * @return The async package loader.
 */
IAsyncPackageLoader* MakeTransactionallySafeAsyncPackageLoader(IAsyncPackageLoader* WrappedPackageLoader = nullptr);
