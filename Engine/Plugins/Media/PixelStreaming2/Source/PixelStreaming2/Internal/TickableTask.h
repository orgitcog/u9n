// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

namespace UE::PixelStreaming2
{
	template <typename T>
	using TUniqueTaskPtr = TUniquePtr<T, struct FPixelStreamingTaskDeleter>;

	/**
	 * Base class for a tickable task. Inheriting from this class ensures that your task can be ticked by the PixelStreaming thread
	 */
	class PIXELSTREAMING2_API FPixelStreamingTickableTask
	{
	public:
		/**
		 * Classes derived from FPixelStreamingTickableTask must construct themselves using this Create method.
		 * Using this Create method ensures the class is fully constructed at the time it is added to
		 * the PixelStreaming thread.
		 */
		template <typename T, typename... Args>
		static TUniqueTaskPtr<T> Create(Args&&...);

	public:
		virtual ~FPixelStreamingTickableTask() = default;

		virtual void Tick(float DeltaMs) { /* Purposeful no-op to avoid pure virtual call if task is ticked mid construction. */ };

		virtual const FString& GetName() const = 0;

	protected:
		FPixelStreamingTickableTask() = default;

	private:
		void Register();
		void Unregister();

		// Allow FPixelStreamingTaskDeleter to access the Unregiser method
		friend FPixelStreamingTaskDeleter;
	};

	/**
	 * TUniquePtr custom deleter that handle automatic unregistering of the task.
	 * This ensure that the task won't try to be ticked mid-deletion
	 */
	struct PIXELSTREAMING2_API FPixelStreamingTaskDeleter
	{
		template <typename T>
		void operator()(T* Ptr) const
		{
			static_assert(std::is_base_of_v<FPixelStreamingTickableTask, T>);
			Ptr->Unregister();
			delete Ptr;
		}
	};

	template <typename T, typename... Args>
	TUniqueTaskPtr<T> FPixelStreamingTickableTask::Create(Args&&... InArgs)
	{
		static_assert(std::is_base_of_v<FPixelStreamingTickableTask, T>);

		TUniqueTaskPtr<T> Task(new T(Forward<Args>(InArgs)...), FPixelStreamingTaskDeleter());
		Task->Register();

		return Task;
	}

} // namespace UE::PixelStreaming2