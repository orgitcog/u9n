// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/PhysicsQueryHandler.h"

bool UPhysicsQueryHandler::Overlap(const EQueryInfo InfoType, const EThreadQueryContext ThreadContext, const UWorld* World, const FOverlapQueryData& OverlapData, const FCommonQueryData& CommonData, TArray<FOverlapResult>& OutOverlaps)
{
	return false;
}

bool UPhysicsQueryHandler::RaycastTest(const EThreadQueryContext ThreadContext, const UWorld* World, const FRayQueryData& RayData, const FCommonQueryData& CommonData, FHitResult& OutHits)
{
	return false;
}

bool UPhysicsQueryHandler::RaycastSingle(const EThreadQueryContext ThreadContext, const UWorld* World, const FRayQueryData& RayData, const FCommonQueryData& CommonData, FHitResult& OutHits)
{
	return false;
}

bool UPhysicsQueryHandler::RaycastMulti(const EThreadQueryContext ThreadContext, const UWorld* World, const FRayQueryData& RayData, const FCommonQueryData& CommonData, TArray<FHitResult>& OutHits)
{
	return false;
}

bool UPhysicsQueryHandler::SweepTest(const EThreadQueryContext ThreadContext, const UWorld* World, const FSweepQueryData& SweepData, const FCommonQueryData& CommonData, FHitResult& OutHits)
{
	return false;
}

bool UPhysicsQueryHandler::SweepSingle(const EThreadQueryContext ThreadContext, const UWorld* World, const FSweepQueryData& SweepData, const FCommonQueryData& CommonData, FHitResult& OutHits)
{
	return false;
}

bool UPhysicsQueryHandler::SweepMulti(const EThreadQueryContext ThreadContext, const UWorld* World, const FSweepQueryData& SweepData, const FCommonQueryData& CommonData, TArray<FHitResult>& OutHits)
{
	return false;
}

void UPhysicsQueryHandler::QueueAsyncOverlap(const FTraceHandle& TraceHandle, const EQueryInfo InfoType, const EThreadQueryContext ThreadContext, const UWorld* World, const FOverlapQueryData& OverlapData, const FCommonQueryData& CommonData)
{
}

void UPhysicsQueryHandler::QueueAsyncRaycast(const FTraceHandle& TraceHandle, const EPhysicsQueryKind QueryKind, const EThreadQueryContext ThreadContext, const UWorld* World, const FRayQueryData& RayData, const FCommonQueryData& CommonData)
{
}

void UPhysicsQueryHandler::QueueAsyncSweep(const FTraceHandle& TraceHandle, const EPhysicsQueryKind QueryKind, const EThreadQueryContext ThreadContext, const UWorld* World, const FSweepQueryData& SweepData, const FCommonQueryData& CommonData)
{
}

bool UPhysicsQueryHandler::AreAsyncRequestsAdded() 
{
	return false;
}

void UPhysicsQueryHandler::VerifyAsyncRequestsAreCompletedOrAbort()
{
}
