// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

template<typename FuncType>
class TScopeGuard final {
    private:
        FuncType Func;
        bool bIsActive;

    public:
		TScopeGuard(FuncType InFunc) : Func(MoveTemp(InFunc)), bIsActive(true) {
        }

        ~TScopeGuard() {
            if (bIsActive) {
				Func();
            }
        }

		TScopeGuard() = delete;
		TScopeGuard(const TScopeGuard&) = delete;
		TScopeGuard(TScopeGuard&& other) : Func(MoveTemp(other.Func)), bIsActive(other.bIsActive) {
            other.Dismiss();
        }

		TScopeGuard& operator=(const TScopeGuard&) = delete;
		TScopeGuard& operator=(TScopeGuard&&) = delete;

        void Dismiss() noexcept {
			bIsActive = false;
        }
};


template<typename FuncType>
TScopeGuard<FuncType> MakeScopeGuard(FuncType InFunc) {
    return TScopeGuard<FuncType>(MoveTemp(InFunc));
}

namespace detail {
	enum class FScopeGuardOnExit {};
	template<typename Fun>
	TScopeGuard<Fun> operator+(FScopeGuardOnExit, Fun&& InFn) {
		return TScopeGuard<Fun>(Forward<Fun>(InFn));
	}
}


// Helper macro
#define SCOPE_EXIT \
    auto SCOPE_GUARD_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = detail::FScopeGuardOnExit() +[&]()

#define SCOPE_GUARD_CONCATENATE_IMPL(s1, s2) s1 ## s2
#define SCOPE_GUARD_CONCATENATE(s1, s2) SCOPE_GUARD_CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define SCOPE_GUARD_ANONYMOUS_VARIABLE(str) SCOPE_GUARD_CONCATENATE(str, __COUNTER__)
#else
#define SCOPE_GUARD_ANONYMOUS_VARIABLE(str) SCOPE_GUARD_CONCATENATE(str, __LINE__)
#endif