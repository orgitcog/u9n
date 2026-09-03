// Copyright Epic Games, Inc. All Rights Reserved.
//
// #include "PredictiveSolverInterface.h"
//
// UE_DISABLE_OPTIMIZATION
//
// namespace UE::MeshTrackerAPI
// {
//
// class PredictiveSolverAccess
// {
// public:
// 	static PredictiveSolverAccess& GetInstance()
// 	{
// 		static PredictiveSolverAccess Instance;
// 		return Instance;
// 	}
//
// 	void Add(TSharedPtr<IPredictiveSolverInterface> InMeshTrackerImplementation, const FString& InMeshTrackerName)
// 	{
// 		APIList.Add(InMeshTrackerName, InMeshTrackerImplementation);
// 	}
//
// 	void AddRaw(const int32 InNum)
// 	{
// 		Nums.Add(InNum);
// 	}
//
// 	TSharedPtr<IPredictiveSolverInterface> Get(const FString& InMeshTrackerName)
// 	{
// 		if (APIList.Contains(InMeshTrackerName))
// 		{
// 			TSharedPtr<IPredictiveSolverInterface> Result = APIList.FindChecked(InMeshTrackerName);
// 			ensureMsgf(Result.IsValid(), TEXT("Runtime %s is not valid"), *InMeshTrackerName);
//
// 			return Result;
// 		}
//
// 		return {};
// 	}
//
// private:
//
// 	TArray<int32> Nums;
//
// 	TMap<FString, TSharedPtr<IPredictiveSolverInterface>> APIList;
// 	
// };
//
// 	void AddAPI(TSharedPtr<IPredictiveSolverInterface> InMeshTrackerImplementation, const FString& InMeshTrackerName)
// 	{
// 		return PredictiveSolverAccess::GetInstance().Add(InMeshTrackerImplementation, InMeshTrackerName);
// 	}
//
// 	TSharedPtr<IPredictiveSolverInterface> GetAPI(const FString& InMeshTrackerName)
// 	{
// 		return PredictiveSolverAccess::GetInstance().Get(InMeshTrackerName);
// 	}
//
// 	void AddRaw(int32 Tracker)
// 	{
// 		PredictiveSolverAccess::GetInstance().AddRaw(Tracker);
// 	}
//
//
// } //namespace UE::MeshTrackerAPI
//
// UE_ENABLE_OPTIMIZATION
