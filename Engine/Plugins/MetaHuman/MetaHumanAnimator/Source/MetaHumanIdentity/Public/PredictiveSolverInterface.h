// Copyright Epic Games, Inc. All Rights Reserved.
//
// #pragma once
//
// #include "Containers/map.h"
// #include "Templates/SharedPointer.h"
//
//
// class METAHUMANIDENTITY_API IPredictiveSolverInterface
// {
// public:
// 	virtual ~IPredictiveSolverInterface() = default;
//
// 	using SolverProgressFunc = TFunction<void(float)>;
//
// 	virtual void TestFunc() = 0;
//
// 	virtual void TrainPredictiveSolver(std::atomic<bool>& bIsDone, std::atomic<float>& InProgress, SolverProgressFunc InOnProgress, std::atomic<bool>& bInIsCancelled,
// 		const struct FPredictiveSolversTaskConfig& InConfig, struct FPredictiveSolversResult& OutResult) = 0;
//
// };
//
// namespace UE::MeshTrackerAPI
// {
// 	METAHUMANIDENTITY_API void AddAPI(TSharedPtr<IPredictiveSolverInterface> InMeshTrackerImplementation, const FString& InMeshTrackerName);
//
// 	METAHUMANIDENTITY_API void AddRaw(int32 Tracker);
//
// 	METAHUMANIDENTITY_API TSharedPtr<IPredictiveSolverInterface> GetAPI(const FString& InMeshTrackerName);
// }