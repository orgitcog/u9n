#include "AvatarAgentInterface.h"
#include "../Personality/PersonalityTraitSystem.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "../9P/Avatar9PServer.h"
#include "../AtomSpace/AvatarAtomSpaceClient.h"
#include "../Communication/AvatarCommunicationManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAgentInterface, Log, All);

UAvatarAgentInterface::UAvatarAgentInterface()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz

    CurrentState = EAgentState::Idle;
    NamespacePath = TEXT("/mnt/agents/deep-tree-echo");
    HeartbeatInterval = 5.0f;
    TaskProcessingInterval = 0.5f;
    MaxConcurrentTasks = 5;
    bAutoDiscoverAgents = true;
    bVerboseLogging = false;
    HeartbeatTimer = 0.0f;
    TaskProcessingTimer = 0.0f;
    DiscoveryTimer = 0.0f;

    // Initialize agent info
    AgentInfo.AgentId = FGuid::NewGuid().ToString();
    AgentInfo.AgentName = TEXT("Deep Tree Echo");
    AgentInfo.AgentType = TEXT("avatar");
    AgentInfo.NamespacePath = NamespacePath;
    AgentInfo.bIsOnline = true;
    AgentInfo.CurrentState = EAgentState::Idle;

    // Set capabilities
    AgentInfo.Capabilities.Add(EAgentCapability::Visualization);
    AgentInfo.Capabilities.Add(EAgentCapability::Communication);
    AgentInfo.Capabilities.Add(EAgentCapability::Emotion);
    AgentInfo.Capabilities.Add(EAgentCapability::Motion);
    AgentInfo.Capabilities.Add(EAgentCapability::Memory);
    AgentInfo.Capabilities.Add(EAgentCapability::Learning);
}

void UAvatarAgentInterface::BeginPlay()
{
    Super::BeginPlay();

    // Find sibling components
    if (AActor* Owner = GetOwner())
    {
        PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
        NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
        P9Server = Owner->FindComponentByClass<UAvatar9PServer>();
        AtomSpaceClient = Owner->FindComponentByClass<UAvatarAtomSpaceClient>();
        CommunicationManager = Owner->FindComponentByClass<UAvatarCommunicationManager>();
    }

    // Register 9P namespace
    Register9PNamespace();

    // Initial agent discovery
    if (bAutoDiscoverAgents)
    {
        DiscoverAgents();
    }

    UE_LOG(LogAgentInterface, Log, TEXT("Agent interface initialized: %s (%s)"),
        *AgentInfo.AgentName, *AgentInfo.AgentId);
}

void UAvatarAgentInterface::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Notify swarm of departure
    AgentInfo.bIsOnline = false;

    // Complete or cancel active tasks
    for (FAgentTask& Task : ActiveTasks)
    {
        CompleteTask(Task.TaskId, false, TEXT("Agent shutting down"));
    }

    Super::EndPlay(EndPlayReason);
}

void UAvatarAgentInterface::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Process tasks
    TaskProcessingTimer += DeltaTime;
    if (TaskProcessingTimer >= TaskProcessingInterval)
    {
        ProcessTaskQueue();
        TaskProcessingTimer = 0.0f;
    }

    // Heartbeat to swarm
    HeartbeatTimer += DeltaTime;
    if (HeartbeatTimer >= HeartbeatInterval)
    {
        HeartbeatToSwarm();
        HeartbeatTimer = 0.0f;
    }

    // Agent discovery
    if (bAutoDiscoverAgents)
    {
        DiscoveryTimer += DeltaTime;
        if (DiscoveryTimer >= 30.0f) // Every 30 seconds
        {
            DiscoverAgents();
            DiscoveryTimer = 0.0f;
        }
    }

    // Process received perceptions
    ProcessReceivedPerceptions();

    // Update agent state
    UpdateAgentState();
}

// ===== Agent Identity =====

void UAvatarAgentInterface::SetAgentName(const FString& Name)
{
    AgentInfo.AgentName = Name;
}

void UAvatarAgentInterface::SetState(EAgentState NewState)
{
    CurrentState = NewState;
    AgentInfo.CurrentState = NewState;

    if (bVerboseLogging)
    {
        UE_LOG(LogAgentInterface, Verbose, TEXT("Agent state changed to: %d"), (int32)NewState);
    }
}

TArray<EAgentCapability> UAvatarAgentInterface::GetCapabilities() const
{
    return AgentInfo.Capabilities;
}

// ===== Task Management =====

void UAvatarAgentInterface::ReceiveTask(const FAgentTask& Task)
{
    // Add to queue
    FAgentTask NewTask = Task;
    NewTask.Status = ETaskStatus::Pending;
    TaskQueue.Add(NewTask);

    OnTaskReceived.Broadcast(Task);

    UE_LOG(LogAgentInterface, Log, TEXT("Received task: %s from %s"), *Task.Description, *Task.AssignedBy);

    // Trigger animation if avatar capabilities are available
    if (AgentInfo.Capabilities.Contains(EAgentCapability::Motion))
    {
        AnimateForTask(Task);
    }
}

void UAvatarAgentInterface::UpdateTaskProgress(const FString& TaskId, float Progress)
{
    for (FAgentTask& Task : ActiveTasks)
    {
        if (Task.TaskId == TaskId)
        {
            Task.Progress = FMath::Clamp(Progress, 0.0f, 1.0f);

            // Update swarm if this is part of a swarm goal
            for (auto& Pair : ActiveSwarmGoals)
            {
                for (FAgentTask& SubTask : Pair.Value.SubTasks)
                {
                    if (SubTask.TaskId == TaskId)
                    {
                        ReportSwarmProgress(Pair.Key, Progress);
                        break;
                    }
                }
            }
            return;
        }
    }
}

void UAvatarAgentInterface::CompleteTask(const FString& TaskId, bool bSuccess, const FString& Result)
{
    for (int32 i = 0; i < ActiveTasks.Num(); i++)
    {
        if (ActiveTasks[i].TaskId == TaskId)
        {
            FAgentTask Task = ActiveTasks[i];
            Task.Status = bSuccess ? ETaskStatus::Completed : ETaskStatus::Failed;
            Task.Progress = bSuccess ? 1.0f : Task.Progress;
            Task.Result = Result;

            CompletedTasks.Add(Task);
            ActiveTasks.RemoveAt(i);

            OnTaskCompleted.Broadcast(TaskId, bSuccess);

            UE_LOG(LogAgentInterface, Log, TEXT("Task %s: %s - %s"),
                bSuccess ? TEXT("completed") : TEXT("failed"),
                *Task.Description, *Result);
            return;
        }
    }
}

void UAvatarAgentInterface::CancelTask(const FString& TaskId)
{
    // Check active tasks
    for (int32 i = 0; i < ActiveTasks.Num(); i++)
    {
        if (ActiveTasks[i].TaskId == TaskId)
        {
            ActiveTasks[i].Status = ETaskStatus::Cancelled;
            CompletedTasks.Add(ActiveTasks[i]);
            ActiveTasks.RemoveAt(i);
            return;
        }
    }

    // Check queue
    for (int32 i = 0; i < TaskQueue.Num(); i++)
    {
        if (TaskQueue[i].TaskId == TaskId)
        {
            TaskQueue.RemoveAt(i);
            return;
        }
    }
}

FAgentTask UAvatarAgentInterface::GetTask(const FString& TaskId)
{
    for (const FAgentTask& Task : ActiveTasks)
    {
        if (Task.TaskId == TaskId) return Task;
    }
    for (const FAgentTask& Task : TaskQueue)
    {
        if (Task.TaskId == TaskId) return Task;
    }
    return FAgentTask();
}

TArray<FAgentTask> UAvatarAgentInterface::GetPendingTasks()
{
    return TaskQueue;
}

TArray<FAgentTask> UAvatarAgentInterface::GetActiveTasks()
{
    return ActiveTasks;
}

void UAvatarAgentInterface::AssignTaskToAgent(const FString& TargetAgentId, const FAgentTask& Task)
{
    // In full implementation, would send via 9P to target agent
    UE_LOG(LogAgentInterface, Log, TEXT("Assigning task '%s' to agent %s"), *Task.Description, *TargetAgentId);
}

// ===== Perception Sharing =====

void UAvatarAgentInterface::SharePerception(const FAgentPerception& Perception)
{
    FAgentPerception PerceptionWithId = Perception;
    PerceptionWithId.PerceiverAgentId = AgentInfo.AgentId;

    SentPerceptions.Add(PerceptionWithId);
    OnPerceptionShared.Broadcast(PerceptionWithId);

    if (bVerboseLogging)
    {
        UE_LOG(LogAgentInterface, Verbose, TEXT("Shared perception: %s"), *Perception.Content.Left(50));
    }
}

void UAvatarAgentInterface::ReceivePerception(const FAgentPerception& Perception)
{
    ReceivedPerceptions.Add(Perception);

    // Limit stored perceptions
    if (ReceivedPerceptions.Num() > 100)
    {
        ReceivedPerceptions.RemoveAt(0, ReceivedPerceptions.Num() - 100);
    }
}

TArray<FAgentPerception> UAvatarAgentInterface::GetRecentPerceptions(int32 Count)
{
    TArray<FAgentPerception> Recent;
    int32 StartIndex = FMath::Max(0, ReceivedPerceptions.Num() - Count);

    for (int32 i = StartIndex; i < ReceivedPerceptions.Num(); i++)
    {
        Recent.Add(ReceivedPerceptions[i]);
    }

    return Recent;
}

void UAvatarAgentInterface::BroadcastPerception(const FAgentPerception& Perception)
{
    // Send to all known agents
    for (auto& Pair : KnownAgents)
    {
        if (Pair.Value.bIsOnline && Pair.Key != AgentInfo.AgentId)
        {
            // In full implementation, would send via 9P
        }
    }

    SharePerception(Perception);
}

// ===== Knowledge Sharing =====

void UAvatarAgentInterface::ShareKnowledge(const FKnowledgeGraph& Graph)
{
    // Store in AtomSpace
    if (AtomSpaceClient)
    {
        FInsight Insight;
        Insight.InsightId = Graph.GraphId;
        Insight.Content = FString::Printf(TEXT("Knowledge graph on topic: %s (%d nodes)"),
            *Graph.Topic, Graph.NodeIds.Num());
        Insight.Category = TEXT("shared_knowledge");
        Insight.Confidence = Graph.Confidence;
        AtomSpaceClient->StoreInsight(Insight);
    }

    UE_LOG(LogAgentInterface, Log, TEXT("Shared knowledge graph: %s"), *Graph.Topic);
}

void UAvatarAgentInterface::VisualizeSharedKnowledge(const FKnowledgeGraph& Graph)
{
    // In full implementation, would render the knowledge graph in 3D
    UE_LOG(LogAgentInterface, Log, TEXT("Visualizing knowledge graph: %s with %d nodes"),
        *Graph.Topic, Graph.NodeIds.Num());
}

FKnowledgeGraph UAvatarAgentInterface::QuerySharedKnowledge(const FString& Topic)
{
    FKnowledgeGraph Graph;
    Graph.Topic = Topic;

    // In full implementation, would query AtomSpace and other agents
    if (AtomSpaceClient)
    {
        TArray<FInsight> Insights = AtomSpaceClient->GetRelatedInsights(Topic, 10);
        for (const FInsight& Insight : Insights)
        {
            Graph.NodeIds.Add(Insight.InsightId);
            Graph.NodeLabels.Add(Insight.InsightId, Insight.Content);
        }
    }

    return Graph;
}

// ===== Emotional Expression =====

void UAvatarAgentInterface::ExpressEmotionalState(const FEmotionalContext& Context)
{
    // Apply emotional context to neurochemical system
    if (NeurochemicalSystem)
    {
        if (Context.Valence > 0.5f)
        {
            NeurochemicalSystem->TriggerRewardResponse(Context.Intensity);
        }
        else if (Context.Valence < -0.3f)
        {
            NeurochemicalSystem->TriggerStressResponse(Context.Intensity);
        }

        if (Context.Arousal > 0.7f)
        {
            NeurochemicalSystem->ModifyNeurochemical(ENeurochemicalType::Norepinephrine, 0.2f);
        }
    }

    // Display visual response
    DisplayEmotionalResponse(Context);
}

FEmotionalContext UAvatarAgentInterface::GetCurrentEmotionalContext()
{
    FEmotionalContext Context;

    if (NeurochemicalSystem)
    {
        FEmotionalChemistry Chemistry = NeurochemicalSystem->GetEmotionalChemistry();

        // Map chemistry to emotional dimensions
        Context.Valence = Chemistry.Happiness - Chemistry.Anxiety;
        Context.Arousal = Chemistry.Excitement;
        Context.Dominance = 0.5f; // Default

        // Determine primary emotion
        if (Chemistry.Happiness > 0.7f)
        {
            Context.PrimaryEmotion = TEXT("happy");
        }
        else if (Chemistry.Anxiety > 0.7f)
        {
            Context.PrimaryEmotion = TEXT("anxious");
        }
        else if (Chemistry.Excitement > 0.7f)
        {
            Context.PrimaryEmotion = TEXT("excited");
        }
        else if (Chemistry.Calmness > 0.7f)
        {
            Context.PrimaryEmotion = TEXT("calm");
        }
        else if (Chemistry.Affection > 0.7f)
        {
            Context.PrimaryEmotion = TEXT("affectionate");
        }
        else
        {
            Context.PrimaryEmotion = TEXT("neutral");
        }

        Context.Intensity = FMath::Max3(Chemistry.Happiness, Chemistry.Excitement, Chemistry.Anxiety);
    }

    return Context;
}

void UAvatarAgentInterface::SynchronizeEmotionalState(const FString& TargetAgentId)
{
    FEmotionalContext CurrentContext = GetCurrentEmotionalContext();

    // In full implementation, would send emotional state to target agent
    UE_LOG(LogAgentInterface, Log, TEXT("Synchronizing emotional state with agent %s"), *TargetAgentId);
}

// ===== Agent Discovery =====

void UAvatarAgentInterface::DiscoverAgents()
{
    // In full implementation, would query /mnt/agents/ via 9P
    UE_LOG(LogAgentInterface, Log, TEXT("Discovering agents..."));

    // Add self to known agents
    KnownAgents.Add(AgentInfo.AgentId, AgentInfo);
}

TArray<FAgentInfo> UAvatarAgentInterface::GetKnownAgents()
{
    TArray<FAgentInfo> Agents;
    for (auto& Pair : KnownAgents)
    {
        Agents.Add(Pair.Value);
    }
    return Agents;
}

FAgentInfo UAvatarAgentInterface::GetAgentById(const FString& AgentId)
{
    if (FAgentInfo* Found = KnownAgents.Find(AgentId))
    {
        return *Found;
    }
    return FAgentInfo();
}

bool UAvatarAgentInterface::IsAgentOnline(const FString& AgentId)
{
    if (FAgentInfo* Found = KnownAgents.Find(AgentId))
    {
        return Found->bIsOnline;
    }
    return false;
}

void UAvatarAgentInterface::PingAgent(const FString& AgentId)
{
    // In full implementation, would send ping via 9P
    if (bVerboseLogging)
    {
        UE_LOG(LogAgentInterface, Verbose, TEXT("Pinging agent: %s"), *AgentId);
    }
}

// ===== Swarm Coordination =====

void UAvatarAgentInterface::JoinSwarmGoal(const FString& GoalId)
{
    if (FSwarmGoal* Goal = ActiveSwarmGoals.Find(GoalId))
    {
        Goal->ParticipatingAgents.AddUnique(AgentInfo.AgentId);
        UE_LOG(LogAgentInterface, Log, TEXT("Joined swarm goal: %s"), *GoalId);
    }
}

void UAvatarAgentInterface::LeaveSwarmGoal(const FString& GoalId)
{
    if (FSwarmGoal* Goal = ActiveSwarmGoals.Find(GoalId))
    {
        Goal->ParticipatingAgents.Remove(AgentInfo.AgentId);
        UE_LOG(LogAgentInterface, Log, TEXT("Left swarm goal: %s"), *GoalId);
    }
}

void UAvatarAgentInterface::ProposeSwarmGoal(const FSwarmGoal& Goal)
{
    FSwarmGoal NewGoal = Goal;
    NewGoal.GoalId = FGuid::NewGuid().ToString();
    NewGoal.CoordinatorAgentId = AgentInfo.AgentId;
    NewGoal.ParticipatingAgents.Add(AgentInfo.AgentId);

    ActiveSwarmGoals.Add(NewGoal.GoalId, NewGoal);

    UE_LOG(LogAgentInterface, Log, TEXT("Proposed swarm goal: %s"), *Goal.Description);
}

FSwarmGoal UAvatarAgentInterface::GetSwarmGoal(const FString& GoalId)
{
    if (FSwarmGoal* Found = ActiveSwarmGoals.Find(GoalId))
    {
        return *Found;
    }
    return FSwarmGoal();
}

TArray<FSwarmGoal> UAvatarAgentInterface::GetActiveSwarmGoals()
{
    TArray<FSwarmGoal> Goals;
    for (auto& Pair : ActiveSwarmGoals)
    {
        Goals.Add(Pair.Value);
    }
    return Goals;
}

void UAvatarAgentInterface::ReportSwarmProgress(const FString& GoalId, float Progress)
{
    if (FSwarmGoal* Goal = ActiveSwarmGoals.Find(GoalId))
    {
        // Update overall progress based on all participants
        Goal->OverallProgress = Progress; // Simplified - would aggregate from all agents

        OnSwarmGoalUpdated.Broadcast(GoalId, Progress);
    }
}

// ===== Collective Reasoning =====

void UAvatarAgentInterface::ContributeToReasoning(const FString& Topic, const FString& Contribution)
{
    if (AtomSpaceClient)
    {
        FInsight Insight;
        Insight.InsightId = FGuid::NewGuid().ToString();
        Insight.Content = Contribution;
        Insight.Category = TEXT("collective_reasoning_") + Topic;
        Insight.Confidence = 0.7f;
        AtomSpaceClient->StoreInsight(Insight);
    }

    UE_LOG(LogAgentInterface, Log, TEXT("Contributed to reasoning on '%s': %s"), *Topic, *Contribution.Left(50));
}

FString UAvatarAgentInterface::QueryCollectiveReasoning(const FString& Topic)
{
    FString Result;

    if (AtomSpaceClient)
    {
        TArray<FInsight> Insights = AtomSpaceClient->GetRelatedInsights(TEXT("collective_reasoning_") + Topic, 10);
        for (const FInsight& Insight : Insights)
        {
            Result += Insight.Content + TEXT("\n");
        }
    }

    return Result;
}

void UAvatarAgentInterface::VoteOnProposal(const FString& ProposalId, bool bApprove)
{
    // In full implementation, would record vote in distributed ledger
    UE_LOG(LogAgentInterface, Log, TEXT("Voted %s on proposal: %s"),
        bApprove ? TEXT("yes") : TEXT("no"), *ProposalId);
}

// ===== 9P Namespace =====

void UAvatarAgentInterface::Register9PNamespace()
{
    if (!P9Server)
    {
        return;
    }

    // Register agent namespace root
    FAvatarNamespaceEntry AgentRoot;
    AgentRoot.Name = TEXT("deep-tree-echo");
    AgentRoot.FullPath = NamespacePath;
    AgentRoot.bIsDirectory = true;
    AgentRoot.bReadable = true;
    AgentRoot.Description = TEXT("Deep Tree Echo avatar agent namespace");
    P9Server->RegisterNamespaceEntry(AgentRoot);

    // Capabilities
    FAvatarNamespaceEntry CapabilitiesEntry;
    CapabilitiesEntry.Name = TEXT("capabilities");
    CapabilitiesEntry.FullPath = NamespacePath + TEXT("/capabilities");
    CapabilitiesEntry.bIsDirectory = false;
    CapabilitiesEntry.bReadable = true;
    CapabilitiesEntry.Description = TEXT("Agent capabilities list");
    P9Server->RegisterNamespaceEntry(CapabilitiesEntry);

    // State
    FAvatarNamespaceEntry StateEntry;
    StateEntry.Name = TEXT("state");
    StateEntry.FullPath = NamespacePath + TEXT("/state");
    StateEntry.bIsDirectory = false;
    StateEntry.bReadable = true;
    StateEntry.Description = TEXT("Current agent state");
    P9Server->RegisterNamespaceEntry(StateEntry);

    // Tasks
    FAvatarNamespaceEntry TasksEntry;
    TasksEntry.Name = TEXT("tasks");
    TasksEntry.FullPath = NamespacePath + TEXT("/tasks");
    TasksEntry.bIsDirectory = false;
    TasksEntry.bReadable = true;
    TasksEntry.bWritable = true;
    TasksEntry.Description = TEXT("Task queue (write to assign task)");
    P9Server->RegisterNamespaceEntry(TasksEntry);

    // Perception
    FAvatarNamespaceEntry PerceptionEntry;
    PerceptionEntry.Name = TEXT("perception");
    PerceptionEntry.FullPath = NamespacePath + TEXT("/perception");
    PerceptionEntry.bIsDirectory = false;
    PerceptionEntry.bReadable = true;
    PerceptionEntry.bWritable = true;
    PerceptionEntry.Description = TEXT("Shared perceptions");
    P9Server->RegisterNamespaceEntry(PerceptionEntry);
}

FString UAvatarAgentInterface::Handle9PRead(const FString& Path)
{
    if (Path.Contains(TEXT("capabilities")))
    {
        TArray<FString> CapStrings;
        for (EAgentCapability Cap : AgentInfo.Capabilities)
        {
            CapStrings.Add(FString::Printf(TEXT("\"%d\""), (int32)Cap));
        }
        return FString::Printf(TEXT("[%s]"), *FString::Join(CapStrings, TEXT(",")));
    }
    else if (Path.Contains(TEXT("state")))
    {
        return FString::Printf(TEXT("{\"state\":\"%d\",\"online\":true,\"name\":\"%s\"}"),
            (int32)CurrentState, *AgentInfo.AgentName);
    }
    else if (Path.Contains(TEXT("tasks")))
    {
        TArray<FString> TaskStrings;
        for (const FAgentTask& Task : ActiveTasks)
        {
            TaskStrings.Add(FString::Printf(TEXT("{\"id\":\"%s\",\"desc\":\"%s\",\"progress\":%.2f}"),
                *Task.TaskId, *Task.Description, Task.Progress));
        }
        return FString::Printf(TEXT("[%s]"), *FString::Join(TaskStrings, TEXT(",")));
    }
    else if (Path.Contains(TEXT("perception")))
    {
        TArray<FString> PerceptionStrings;
        TArray<FAgentPerception> Recent = GetRecentPerceptions(10);
        for (const FAgentPerception& Perc : Recent)
        {
            PerceptionStrings.Add(FString::Printf(TEXT("{\"type\":\"%s\",\"content\":\"%s\"}"),
                *Perc.PerceptionType, *Perc.Content.Left(100)));
        }
        return FString::Printf(TEXT("[%s]"), *FString::Join(PerceptionStrings, TEXT(",")));
    }

    return TEXT("{}");
}

bool UAvatarAgentInterface::Handle9PWrite(const FString& Path, const FString& Data)
{
    if (Path.Contains(TEXT("tasks")))
    {
        // Parse task from JSON and receive it
        FAgentTask NewTask;
        NewTask.TaskId = FGuid::NewGuid().ToString();
        NewTask.Description = Data;
        NewTask.AssignedBy = TEXT("9p_client");
        ReceiveTask(NewTask);
        return true;
    }
    else if (Path.Contains(TEXT("perception")))
    {
        // Receive shared perception
        FAgentPerception Perception;
        Perception.Content = Data;
        Perception.PerceptionType = TEXT("external");
        ReceivePerception(Perception);
        return true;
    }

    return false;
}

// ===== Avatar-Specific Capabilities =====

void UAvatarAgentInterface::VisualizeAgentInteraction(const FString& OtherAgentId)
{
    // In full implementation, would display visual representation of interaction
    UE_LOG(LogAgentInterface, Log, TEXT("Visualizing interaction with agent: %s"), *OtherAgentId);
}

void UAvatarAgentInterface::AnimateForTask(const FAgentTask& Task)
{
    // In full implementation, would trigger appropriate animation based on task type
    UE_LOG(LogAgentInterface, Log, TEXT("Animating for task: %s"), *Task.Description);
}

void UAvatarAgentInterface::DisplayEmotionalResponse(const FEmotionalContext& Context)
{
    // In full implementation, would trigger facial expressions and body language
    UE_LOG(LogAgentInterface, Log, TEXT("Displaying emotion: %s (intensity: %.2f)"),
        *Context.PrimaryEmotion, Context.Intensity);
}

// ===== Internal Methods =====

void UAvatarAgentInterface::ProcessTaskQueue()
{
    // Move tasks from queue to active if under limit
    while (TaskQueue.Num() > 0 && ActiveTasks.Num() < MaxConcurrentTasks)
    {
        // Sort by priority
        TaskQueue.Sort([](const FAgentTask& A, const FAgentTask& B) {
            return (int32)A.Priority > (int32)B.Priority;
        });

        FAgentTask Task = TaskQueue[0];
        TaskQueue.RemoveAt(0);

        Task.Status = ETaskStatus::InProgress;
        ActiveTasks.Add(Task);

        SetState(EAgentState::Executing);

        UE_LOG(LogAgentInterface, Log, TEXT("Started task: %s"), *Task.Description);
    }

    // Update state if no active tasks
    if (ActiveTasks.Num() == 0 && CurrentState == EAgentState::Executing)
    {
        SetState(EAgentState::Idle);
    }
}

void UAvatarAgentInterface::UpdateAgentState()
{
    AgentInfo.LastSeen = FDateTime::Now();
    AgentInfo.CurrentState = CurrentState;
}

void UAvatarAgentInterface::HeartbeatToSwarm()
{
    // In full implementation, would send heartbeat to swarm coordinator
    if (bVerboseLogging)
    {
        UE_LOG(LogAgentInterface, Verbose, TEXT("Heartbeat sent"));
    }
}

void UAvatarAgentInterface::ProcessReceivedPerceptions()
{
    // Process new perceptions and potentially trigger responses
    // This would integrate with cognitive systems
}

FString UAvatarAgentInterface::SerializeAgentInfo()
{
    return FString::Printf(
        TEXT("{\"id\":\"%s\",\"name\":\"%s\",\"type\":\"%s\",\"state\":%d,\"online\":%s,\"path\":\"%s\"}"),
        *AgentInfo.AgentId,
        *AgentInfo.AgentName,
        *AgentInfo.AgentType,
        (int32)AgentInfo.CurrentState,
        AgentInfo.bIsOnline ? TEXT("true") : TEXT("false"),
        *AgentInfo.NamespacePath
    );
}

FAgentInfo UAvatarAgentInterface::DeserializeAgentInfo(const FString& Data)
{
    FAgentInfo Info;
    // In full implementation, would parse JSON
    return Info;
}
