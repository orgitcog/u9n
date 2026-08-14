#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Avatar9PServer.generated.h"

// Forward declarations
class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;
class UDiaryInsightBlogLoop;
class UAvatar3DComponent;

/**
 * 9P Message Types (Plan 9 protocol)
 */
UENUM(BlueprintType)
enum class E9PMessageType : uint8
{
    Tversion = 100,  // Version negotiation
    Rversion = 101,
    Tauth = 102,     // Authentication
    Rauth = 103,
    Tattach = 104,   // Attach to namespace
    Rattach = 105,
    Terror = 106,    // Error (client never sends)
    Rerror = 107,
    Tflush = 108,    // Abort message
    Rflush = 109,
    Twalk = 110,     // Walk to file
    Rwalk = 111,
    Topen = 112,     // Open file
    Ropen = 113,
    Tcreate = 114,   // Create file
    Rcreate = 115,
    Tread = 116,     // Read from file
    Rread = 117,
    Twrite = 118,    // Write to file
    Rwrite = 119,
    Tclunk = 120,    // Close fid
    Rclunk = 121,
    Tremove = 122,   // Remove file
    Rremove = 123,
    Tstat = 124,     // Get file stats
    Rstat = 125,
    Twstat = 126,    // Set file stats
    Rwstat = 127
};

/**
 * 9P Request structure
 */
USTRUCT(BlueprintType)
struct F9PRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    E9PMessageType MessageType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint32 Tag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint32 Fid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<uint8> Data;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint64 Offset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint32 Count;

    F9PRequest()
        : MessageType(E9PMessageType::Tversion)
        , Tag(0)
        , Fid(0)
        , Offset(0)
        , Count(0)
    {}
};

/**
 * 9P Response structure
 */
USTRUCT(BlueprintType)
struct F9PResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    E9PMessageType MessageType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint32 Tag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSuccess;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ErrorMessage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<uint8> Data;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StringData;

    F9PResponse()
        : MessageType(E9PMessageType::Rversion)
        , Tag(0)
        , bSuccess(true)
    {}
};

/**
 * File handle for 9P operations
 */
USTRUCT(BlueprintType)
struct F9PFileHandle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint32 Fid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsDirectory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsOpen;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 Mode; // 0=read, 1=write, 2=read/write

    F9PFileHandle()
        : Fid(0)
        , bIsDirectory(false)
        , bIsOpen(false)
        , Mode(0)
    {}
};

/**
 * Namespace entry for avatar filesystem
 */
USTRUCT(BlueprintType)
struct FAvatarNamespaceEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FullPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsDirectory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bReadable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bWritable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    FAvatarNamespaceEntry()
        : bIsDirectory(false)
        , bReadable(true)
        , bWritable(false)
    {}
};

/**
 * Delegate for 9P state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOn9PStateChanged, const FString&, Path, const FString&, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn9PClientConnected, int32, ClientId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn9PClientDisconnected, int32, ClientId);

/**
 * Avatar 9P Server Component
 * Exposes avatar state via 9P filesystem protocol for AGI-OS integration
 *
 * Namespace Structure:
 * /mnt/avatar/
 * ├── personality/
 * │   ├── traits/
 * │   │   ├── confidence
 * │   │   ├── charm
 * │   │   ├── playfulness
 * │   │   └── ...
 * │   ├── state
 * │   └── ctl
 * ├── neurochemical/
 * │   ├── levels/
 * │   │   ├── dopamine
 * │   │   ├── serotonin
 * │   │   └── ...
 * │   ├── emotional_state
 * │   └── ctl
 * ├── narrative/
 * │   ├── diary/
 * │   ├── insights/
 * │   ├── blog/
 * │   └── ctl
 * ├── visual/
 * │   ├── appearance/
 * │   ├── animation/
 * │   ├── effects/
 * │   └── ctl
 * ├── cognitive/
 * │   ├── attention
 * │   ├── memory_load
 * │   ├── processing_state
 * │   └── ctl
 * ├── performance/
 * │   ├── metrics
 * │   ├── budgets
 * │   └── ctl
 * └── batch/
 *     ├── query
 *     └── results
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAvatar9PServer : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatar9PServer();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Server Lifecycle =====

    UFUNCTION(BlueprintCallable, Category = "9P|Server")
    void Initialize();

    UFUNCTION(BlueprintCallable, Category = "9P|Server")
    void StartServer(int32 Port = 564);

    UFUNCTION(BlueprintCallable, Category = "9P|Server")
    void StopServer();

    UFUNCTION(BlueprintCallable, Category = "9P|Server")
    bool IsServerRunning() const { return bServerRunning; }

    // ===== Namespace Management =====

    UFUNCTION(BlueprintCallable, Category = "9P|Namespace")
    void ExportNamespace();

    UFUNCTION(BlueprintCallable, Category = "9P|Namespace")
    TArray<FAvatarNamespaceEntry> GetNamespaceEntries() const { return NamespaceEntries; }

    UFUNCTION(BlueprintCallable, Category = "9P|Namespace")
    void RegisterNamespaceEntry(const FAvatarNamespaceEntry& Entry);

    // ===== Request Handling =====

    UFUNCTION(BlueprintCallable, Category = "9P|Protocol")
    F9PResponse HandleRequest(const F9PRequest& Request);

    UFUNCTION(BlueprintCallable, Category = "9P|Protocol")
    F9PResponse HandleRead(const FString& Path, uint64 Offset, uint32 Count);

    UFUNCTION(BlueprintCallable, Category = "9P|Protocol")
    F9PResponse HandleWrite(const FString& Path, const TArray<uint8>& Data);

    UFUNCTION(BlueprintCallable, Category = "9P|Protocol")
    F9PResponse HandleStat(const FString& Path);

    // ===== Personality Namespace Handlers =====

    UFUNCTION(BlueprintCallable, Category = "9P|Personality")
    FString ReadPersonalityTrait(const FString& TraitName);

    UFUNCTION(BlueprintCallable, Category = "9P|Personality")
    bool WritePersonalityTrait(const FString& TraitName, float Value);

    UFUNCTION(BlueprintCallable, Category = "9P|Personality")
    FString ReadPersonalityState();

    // ===== Neurochemical Namespace Handlers =====

    UFUNCTION(BlueprintCallable, Category = "9P|Neurochemical")
    FString ReadNeurochemicalLevel(const FString& ChemicalName);

    UFUNCTION(BlueprintCallable, Category = "9P|Neurochemical")
    bool WriteNeurochemicalLevel(const FString& ChemicalName, float Value);

    UFUNCTION(BlueprintCallable, Category = "9P|Neurochemical")
    FString ReadEmotionalState();

    // ===== Narrative Namespace Handlers =====

    UFUNCTION(BlueprintCallable, Category = "9P|Narrative")
    FString ReadDiaryEntry(const FString& EntryId);

    UFUNCTION(BlueprintCallable, Category = "9P|Narrative")
    TArray<FString> ListDiaryEntries();

    UFUNCTION(BlueprintCallable, Category = "9P|Narrative")
    FString ReadInsight(const FString& InsightId);

    // ===== Visual Namespace Handlers =====

    UFUNCTION(BlueprintCallable, Category = "9P|Visual")
    FString ReadAppearance(const FString& Parameter);

    UFUNCTION(BlueprintCallable, Category = "9P|Visual")
    bool WriteAppearance(const FString& Parameter, const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "9P|Visual")
    FString ReadAnimationState();

    // ===== Cognitive Namespace Handlers =====

    UFUNCTION(BlueprintCallable, Category = "9P|Cognitive")
    FString ReadCognitiveState(const FString& Parameter);

    UFUNCTION(BlueprintCallable, Category = "9P|Cognitive")
    float GetAttentionLevel() const;

    UFUNCTION(BlueprintCallable, Category = "9P|Cognitive")
    float GetMemoryLoad() const;

    // ===== Performance Namespace Handlers =====

    UFUNCTION(BlueprintCallable, Category = "9P|Performance")
    FString ReadPerformanceMetrics();

    UFUNCTION(BlueprintCallable, Category = "9P|Performance")
    FString ReadPerformanceBudgets();

    // ===== Batch Operations =====

    UFUNCTION(BlueprintCallable, Category = "9P|Batch")
    FString ProcessBatchQuery(const FString& QueryList);

    UFUNCTION(BlueprintCallable, Category = "9P|Batch")
    FString ProcessBatchWrite(const FString& WriteList);

    // ===== Message Coalescing =====

    UFUNCTION(BlueprintCallable, Category = "9P|Performance")
    void EnableMessageCoalescing(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "9P|Performance")
    void SetCoalescingInterval(float IntervalSeconds);

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "9P|Events")
    FOn9PStateChanged On9PStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "9P|Events")
    FOn9PClientConnected On9PClientConnected;

    UPROPERTY(BlueprintAssignable, Category = "9P|Events")
    FOn9PClientDisconnected On9PClientDisconnected;

protected:
    // ===== Internal Methods =====

    void BuildNamespace();
    void ProcessPendingMessages();
    FString ReadFromPath(const FString& Path);
    bool WriteToPath(const FString& Path, const FString& Value);
    FString SerializeToJson(const FString& Path);
    bool DeserializeFromJson(const FString& Path, const FString& JsonValue);

    // Path parsing helpers
    TArray<FString> ParsePath(const FString& Path);
    FString GetNamespaceRoot(const FString& Path);
    FString GetRelativePath(const FString& Path);

private:
    // ===== Component References =====

    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem;

    UPROPERTY()
    UDiaryInsightBlogLoop* NarrativeSystem;

    UPROPERTY()
    UAvatar3DComponent* AvatarComponent;

    // ===== Server State =====

    UPROPERTY()
    bool bServerRunning;

    UPROPERTY()
    int32 ServerPort;

    UPROPERTY()
    TArray<FAvatarNamespaceEntry> NamespaceEntries;

    UPROPERTY()
    TMap<uint32, F9PFileHandle> FileHandles;

    UPROPERTY()
    uint32 NextFid;

    // ===== Message Coalescing =====

    UPROPERTY()
    bool bEnableCoalescing;

    UPROPERTY()
    float CoalescingInterval;

    UPROPERTY()
    float CoalescingTimer;

    UPROPERTY()
    TArray<F9PRequest> PendingRequests;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "9P|Configuration")
    FString NamespaceRoot;

    UPROPERTY(EditAnywhere, Category = "9P|Configuration")
    int32 MaxMessageSize;

    UPROPERTY(EditAnywhere, Category = "9P|Configuration")
    int32 MaxPendingMessages;

    UPROPERTY(EditAnywhere, Category = "9P|Configuration")
    bool bVerboseLogging;
};
