// Modules/ModuleManager.h - Stub for standalone compilation testing
// In production, the actual Unreal Engine header will be used.

#pragma once
#include "CoreMinimal.h"

// Module management stubs
class FModuleManager {
public:
    static FModuleManager& Get() {
        static FModuleManager Instance;
        return Instance;
    }
    
    template<typename T>
    static T& GetModuleChecked(const FName& ModuleName) {
        static T Module;
        return Module;
    }
    
    template<typename T>
    static T* GetModulePtr(const FName& ModuleName) {
        return nullptr;
    }
    
    static bool IsModuleLoaded(const FName& ModuleName) { return false; }
    
    template<typename T>
    static T& LoadModuleChecked(const FName& ModuleName) {
        static T Module;
        return Module;
    }
    
    static IModuleInterface* LoadModule(const FName& ModuleName) { return nullptr; }
    
    static void UnloadModule(const FName& ModuleName) {}
};

#define IMPLEMENT_PRIMARY_GAME_MODULE(ModuleClass, ModuleName, GameName) \
    IMPLEMENT_MODULE(ModuleClass, ModuleName)

#define IMPLEMENT_GAME_MODULE(ModuleClass, ModuleName) \
    IMPLEMENT_MODULE(ModuleClass, ModuleName)
