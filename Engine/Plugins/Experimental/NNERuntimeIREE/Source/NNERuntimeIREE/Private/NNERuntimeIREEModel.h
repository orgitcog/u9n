// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_NNE_RUNTIME_IREE

#include "NNERuntimeCPU.h"
#include "NNERuntimeRDG.h"
#include "NNERuntimeIREEMetaData.h"

LLM_DECLARE_TAG(NNERuntimeIREE_Cpu);

namespace UE::NNERuntimeIREE
{
	namespace Private
	{
		class FModule;
	} // Private

	namespace CPU
	{
		namespace Private 
		{
			class FSession;
			class FDevice;
		} // Private

		class FModelInstance : public UE::NNE::IModelInstanceCPU
		{
		private:
			FModelInstance(TSharedRef<UE::NNERuntimeIREE::CPU::Private::FSession> InSession);

		public:
			static TSharedPtr<FModelInstance> Make(TSharedRef<UE::NNERuntimeIREE::CPU::Private::FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule);

		public:
			using ESetInputTensorShapesStatus = UE::NNE::IModelInstanceCPU::ESetInputTensorShapesStatus;
			using ERunSyncStatus = UE::NNE::IModelInstanceCPU::ERunSyncStatus;

			//~ Begin IModelInstanceCPU Interface
			virtual TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const override;
			virtual TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const override;
			virtual TConstArrayView<UE::NNE::FTensorShape> GetInputTensorShapes() const override;
			virtual TConstArrayView<UE::NNE::FTensorShape> GetOutputTensorShapes() const override;
			virtual ESetInputTensorShapesStatus SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes) override;
			virtual ERunSyncStatus RunSync(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings) override;
			//~ End IModelInstanceCPU Interface

		private:
			TSharedRef<UE::NNERuntimeIREE::CPU::Private::FSession> Session;
		};

		class FModel : public UE::NNE::IModelCPU
		{
		private:
			FModel(TSharedRef<UE::NNERuntimeIREE::CPU::Private::FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule);

		public:
			static TSharedPtr<FModel> Make(const FString& InDirPath, const FString& InSharedLibraryFileName, const FString& InVmfbFileName, const FString& InLibraryQueryFunctionName, const UNNERuntimeIREEModuleMetaData& InModuleMetaData);

		public:
			//~ Begin IModelCPU Interface
			virtual TSharedPtr<UE::NNE::IModelInstanceCPU> CreateModelInstanceCPU() override;
			//~ End IModelCPU Interface

		private:
			TSharedRef<UE::NNERuntimeIREE::CPU::Private::FDevice> Device;
			TSharedRef<UE::NNERuntimeIREE::Private::FModule> Module;
		};
	} // CPU

	namespace RDG
	{
		namespace Private 
		{
			class FSession;
			class FDevice;
		} // Private

		class FModelInstance : public UE::NNE::IModelInstanceRDG
		{
		private:
			FModelInstance(TSharedRef<UE::NNERuntimeIREE::RDG::Private::FSession> InSession);

		public:
			static TSharedPtr<FModelInstance> Make(TSharedRef<UE::NNERuntimeIREE::RDG::Private::FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule);

		public:
			using ESetInputTensorShapesStatus = UE::NNE::IModelInstanceRDG::ESetInputTensorShapesStatus;
			using EEnqueueRDGStatus = UE::NNE::IModelInstanceRDG::EEnqueueRDGStatus;

			//~ Begin IModelInstanceRDG Interface
			virtual TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const override;
			virtual TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const override;
			virtual TConstArrayView<UE::NNE::FTensorShape> GetInputTensorShapes() const override;
			virtual TConstArrayView<UE::NNE::FTensorShape> GetOutputTensorShapes() const override;
			virtual ESetInputTensorShapesStatus SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes) override;
			virtual EEnqueueRDGStatus EnqueueRDG(FRDGBuilder& RDGBuilder, TConstArrayView<UE::NNE::FTensorBindingRDG> InInputBindings, TConstArrayView<UE::NNE::FTensorBindingRDG> InOutputBindings) override;
			//~ End IModelInstanceRDG Interface

		private:
			TSharedRef<UE::NNERuntimeIREE::RDG::Private::FSession> Session;
		};

		class FModel : public UE::NNE::IModelRDG
		{
		private:
			FModel(TSharedRef<UE::NNERuntimeIREE::RDG::Private::FDevice> InDevice, TSharedRef<UE::NNERuntimeIREE::Private::FModule> InModule);

		public:
			static TSharedPtr<FModel> Make(const FString& InDirPath, const FString& InVmfbFileName, const UNNERuntimeIREEModuleMetaData& InModuleMetaData);

		public:
			//~ Begin IModelRDG Interface
			virtual TSharedPtr<UE::NNE::IModelInstanceRDG> CreateModelInstanceRDG() override;
			//~ End IModelRDG Interface

		private:
			TSharedRef<UE::NNERuntimeIREE::RDG::Private::FDevice> Device;
			TSharedRef<UE::NNERuntimeIREE::Private::FModule> Module;
		};
	} // namespace RDG

} // UE::NNERuntimeIREE

#endif // WITH_NNE_RUNTIME_IREE