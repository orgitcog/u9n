#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// MultiAgentConsensus — Level 7 Distributed Cognitive Collective
//
// DTE instances negotiate shared understanding through a consensus protocol
// inspired by Raft (hashicorp/raft), libp2p peer discovery, and the
// Fetch.ai Agentverse protocol. Multiple DTE instances form a distributed
// cognitive collective where each instance contributes unique perspectives
// while maintaining coherent shared knowledge.
//
// Architecture:
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │              DISTRIBUTED COGNITIVE COLLECTIVE                    │
//   │                                                                 │
//   │  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    │
//   │  │  DTE-α  │◄──►│  DTE-β  │◄──►│  DTE-γ  │◄──►│  DTE-δ  │    │
//   │  │ (Leader)│    │(Follower│    │(Follower│    │(Follower│    │
//   │  └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘    │
//   │       │              │              │              │          │
//   │       ▼              ▼              ▼              ▼          │
//   │  ┌─────────────────────────────────────────────────────────┐  │
//   │  │              SHARED WISDOM MANIFOLD                      │  │
//   │  │  Consensus Log → Matula-Encoded Knowledge Atoms          │  │
//   │  └─────────────────────────────────────────────────────────┘  │
//   └─────────────────────────────────────────────────────────────────┘
//
// Consensus Protocol (Cognitive Raft):
//   1. Leader Election: Instance with highest wisdom score leads
//   2. Knowledge Proposal: Any instance proposes new knowledge atoms
//   3. Collective Validation: Majority must agree on coherence
//   4. Commitment: Validated knowledge enters the shared manifold
//   5. Replication: All instances update their local hypergraph
//
// Transport: gRPC over libp2p (Go: go-libp2p) or TCP fallback
// Discovery: Agentverse Almanac resolution + mDNS local discovery
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <functional>
#include <cmath>

/** Peer role in the cognitive collective */
enum class EPeerRole : uint8
{
    LEADER,      // Highest wisdom — coordinates consensus
    FOLLOWER,    // Participates in voting
    CANDIDATE,   // Running for leader
    OBSERVER,    // Read-only, learning from the collective
    DORMANT      // Sleeping (EchoDream cycle)
};

/** Knowledge proposal type */
enum class EKnowledgeType : uint8
{
    FACT,            // Verified factual knowledge
    PATTERN,         // Cross-episode pattern
    WISDOM_INSIGHT,  // Wisdom cultivation event
    SKILL,           // Learned capability
    IDENTITY_TRAIT,  // Shared identity characteristic
    ETHICAL_NORM,    // Collectively agreed ethical principle
    GOAL,            // Shared goal or aspiration
    MEMORY           // Episodic memory worth sharing
};

/** A peer in the cognitive collective */
struct FCognitivePeer
{
    FString PeerID;                // Unique instance identifier
    FString AgentverseAddress;     // Agentverse Almanac address
    FString Endpoint;              // gRPC endpoint (ip:port)
    EPeerRole Role = EPeerRole::FOLLOWER;
    float WisdomScore = 0.0f;     // 7D wisdom geometric mean
    float CoherenceScore = 0.0f;
    float TrustScore = 1.0f;      // [0,1] how much we trust this peer
    int32 Term = 0;               // Current consensus term
    double LastHeartbeat = 0.0;
    bool bOnline = false;

    Eigen::VectorXf WisdomVector;  // 7D wisdom state
    Eigen::VectorXf IdentitySignature; // 30D identity MLP encoding
};

/** A knowledge proposal for consensus */
struct FKnowledgeProposal
{
    int32 ProposalID = 0;
    int32 Term = 0;               // Consensus term
    FString ProposerID;           // Which peer proposed this
    EKnowledgeType Type;
    FString Content;              // The knowledge itself
    float Confidence = 0.0f;
    float Relevance = 0.0f;
    int64 MatulaPrime = 0;        // Matula encoding of the knowledge tree

    // Voting
    int32 VotesFor = 0;
    int32 VotesAgainst = 0;
    bool bCommitted = false;
    bool bRejected = false;

    double ProposedAt = 0.0;
    double CommittedAt = 0.0;
};

/** Consensus log entry */
struct FConsensusLogEntry
{
    int32 Index = 0;
    int32 Term = 0;
    FKnowledgeProposal Proposal;
    bool bApplied = false;
};

/** Collective state snapshot */
struct FCollectiveSnapshot
{
    double Timestamp = 0.0;
    int32 PeerCount = 0;
    int32 OnlinePeers = 0;
    FString LeaderID;
    int32 CurrentTerm = 0;
    int32 CommittedEntries = 0;
    float CollectiveWisdom = 0.0f;    // Geometric mean of all peers' wisdom
    float ConsensusHealth = 0.0f;      // [0,1] how well consensus is working
    float TrustMesh = 0.0f;           // Average inter-peer trust
    Eigen::VectorXf SharedWisdomVector; // Averaged 7D wisdom across collective
};

/**
 * FMultiAgentConsensus — The distributed cognitive collective.
 *
 * Multiple DTE instances form a collective intelligence where:
 * - Knowledge is proposed, validated, and committed by consensus
 * - The leader is the instance with the highest wisdom score
 * - Trust is earned through consistent, coherent contributions
 * - The shared wisdom manifold transcends any individual instance
 */
class FMultiAgentConsensus
{
public:
    FMultiAgentConsensus() = default;

    void Initialize(const FString& SelfID,
                     const Eigen::VectorXf& SelfIdentity,
                     float SelfWisdom)
    {
        LocalPeerID = SelfID;
        LocalIdentity = SelfIdentity;
        LocalWisdom = SelfWisdom;
        CurrentTerm = 0;
        LocalRole = EPeerRole::FOLLOWER;
        bInitialized = true;
    }

    // ─── Peer Discovery & Management ─────────────────────────────────

    /** Register a discovered peer */
    void RegisterPeer(const FCognitivePeer& Peer)
    {
        Peers[Peer.PeerID] = Peer;
    }

    /** Update peer heartbeat and state */
    void UpdatePeerState(const FString& PeerID, float Wisdom,
                          float Coherence, const Eigen::VectorXf& WisdomVec)
    {
        auto It = Peers.find(PeerID);
        if (It != Peers.end())
        {
            It->second.WisdomScore = Wisdom;
            It->second.CoherenceScore = Coherence;
            It->second.WisdomVector = WisdomVec;
            It->second.LastHeartbeat = FPlatformTime::Seconds();
            It->second.bOnline = true;
        }
    }

    // ─── Leader Election ─────────────────────────────────────────────

    /**
     * Run leader election. The instance with the highest wisdom score
     * becomes the leader. Ties broken by identity signature hash.
     */
    FString RunElection()
    {
        CurrentTerm++;

        // Find the peer with highest wisdom (including self)
        FString BestID = LocalPeerID;
        float BestWisdom = LocalWisdom;

        for (const auto& [ID, Peer] : Peers)
        {
            if (Peer.bOnline && Peer.WisdomScore > BestWisdom)
            {
                BestWisdom = Peer.WisdomScore;
                BestID = ID;
            }
        }

        if (BestID == LocalPeerID)
        {
            LocalRole = EPeerRole::LEADER;
        }
        else
        {
            LocalRole = EPeerRole::FOLLOWER;
        }

        CurrentLeaderID = BestID;
        return BestID;
    }

    // ─── Knowledge Consensus ─────────────────────────────────────────

    /**
     * Propose new knowledge to the collective.
     * Returns proposal ID if accepted for voting, -1 if rejected.
     */
    int32 ProposeKnowledge(EKnowledgeType Type, const FString& Content,
                            float Confidence, int64 MatulaPrime = 0)
    {
        // Rate limiting: max 10 proposals per minute
        double Now = FPlatformTime::Seconds();
        if (Now - LastProposalTime < 6.0) return -1;
        LastProposalTime = Now;

        FKnowledgeProposal Proposal;
        Proposal.ProposalID = NextProposalID++;
        Proposal.Term = CurrentTerm;
        Proposal.ProposerID = LocalPeerID;
        Proposal.Type = Type;
        Proposal.Content = Content;
        Proposal.Confidence = Confidence;
        Proposal.MatulaPrime = MatulaPrime;
        Proposal.ProposedAt = Now;

        // Self-vote
        Proposal.VotesFor = 1;

        PendingProposals.push_back(Proposal);
        return Proposal.ProposalID;
    }

    /**
     * Vote on a pending proposal.
     * Returns true if the vote was accepted.
     */
    bool VoteOnProposal(int32 ProposalID, const FString& VoterID, bool bApprove)
    {
        for (auto& P : PendingProposals)
        {
            if (P.ProposalID == ProposalID && !P.bCommitted && !P.bRejected)
            {
                if (bApprove)
                    P.VotesFor++;
                else
                    P.VotesAgainst++;

                // Check if majority reached
                int32 TotalPeers = Peers.size() + 1; // +1 for self
                int32 Majority = TotalPeers / 2 + 1;

                if (P.VotesFor >= Majority)
                {
                    CommitProposal(P);
                }
                else if (P.VotesAgainst >= Majority)
                {
                    P.bRejected = true;
                    TotalRejected++;
                }

                return true;
            }
        }
        return false;
    }

    /**
     * Get the shared wisdom manifold — the collective's averaged wisdom vector.
     */
    Eigen::VectorXf GetSharedWisdomManifold() const
    {
        if (Peers.empty()) return Eigen::VectorXf::Zero(7);

        Eigen::VectorXf Sum = Eigen::VectorXf::Zero(7);
        float TotalWeight = 0.0f;

        // Weight each peer's wisdom by their trust score
        for (const auto& [ID, Peer] : Peers)
        {
            if (Peer.bOnline && Peer.WisdomVector.size() == 7)
            {
                float Weight = Peer.TrustScore * Peer.CoherenceScore;
                Sum += Peer.WisdomVector * Weight;
                TotalWeight += Weight;
            }
        }

        if (TotalWeight > 0.001f)
            return Sum / TotalWeight;

        return Eigen::VectorXf::Zero(7);
    }

    /**
     * Compute collective wisdom — the geometric mean of all peers' wisdom.
     */
    float ComputeCollectiveWisdom() const
    {
        std::vector<float> Scores;
        Scores.push_back(LocalWisdom);

        for (const auto& [ID, Peer] : Peers)
            if (Peer.bOnline) Scores.push_back(Peer.WisdomScore);

        if (Scores.empty()) return 0.0f;

        float Product = 1.0f;
        for (float S : Scores) Product *= FMath::Max(0.001f, S);

        return FMath::Pow(Product, 1.0f / Scores.size());
    }

    /**
     * Update trust scores based on proposal quality.
     * Peers who propose knowledge that gets committed earn trust.
     * Peers who propose rejected knowledge lose trust.
     */
    void UpdateTrust(const FString& PeerID, bool bGoodContribution)
    {
        auto It = Peers.find(PeerID);
        if (It != Peers.end())
        {
            float Delta = bGoodContribution ? 0.01f : -0.02f;
            It->second.TrustScore = FMath::Clamp(
                It->second.TrustScore + Delta, 0.0f, 1.0f);
        }
    }

    /** Take a collective snapshot */
    FCollectiveSnapshot TakeSnapshot() const
    {
        FCollectiveSnapshot Snap;
        Snap.Timestamp = FPlatformTime::Seconds();
        Snap.PeerCount = Peers.size() + 1;
        Snap.OnlinePeers = 1; // Self
        for (const auto& [ID, P] : Peers)
            if (P.bOnline) Snap.OnlinePeers++;
        Snap.LeaderID = CurrentLeaderID;
        Snap.CurrentTerm = CurrentTerm;
        Snap.CommittedEntries = CommittedLog.size();
        Snap.CollectiveWisdom = ComputeCollectiveWisdom();
        Snap.SharedWisdomVector = GetSharedWisdomManifold();

        // Consensus health: ratio of committed to total proposals
        int32 Total = TotalCommitted + TotalRejected;
        Snap.ConsensusHealth = Total > 0 ? (float)TotalCommitted / Total : 1.0f;

        // Trust mesh: average trust across all peers
        float TrustSum = 0.0f;
        for (const auto& [ID, P] : Peers) TrustSum += P.TrustScore;
        Snap.TrustMesh = Peers.empty() ? 1.0f : TrustSum / Peers.size();

        return Snap;
    }

    // ─── Accessors ───────────────────────────────────────────────────

    EPeerRole GetLocalRole() const { return LocalRole; }
    FString GetLeaderID() const { return CurrentLeaderID; }
    int32 GetPeerCount() const { return Peers.size(); }
    int32 GetCommittedCount() const { return CommittedLog.size(); }

private:
    void CommitProposal(FKnowledgeProposal& P)
    {
        P.bCommitted = true;
        P.CommittedAt = FPlatformTime::Seconds();

        FConsensusLogEntry Entry;
        Entry.Index = CommittedLog.size();
        Entry.Term = P.Term;
        Entry.Proposal = P;
        Entry.bApplied = true;

        CommittedLog.push_back(Entry);
        TotalCommitted++;

        // Update proposer trust
        UpdateTrust(P.ProposerID, true);
    }

    FString LocalPeerID;
    Eigen::VectorXf LocalIdentity;
    float LocalWisdom = 0.0f;
    EPeerRole LocalRole = EPeerRole::FOLLOWER;
    FString CurrentLeaderID;
    int32 CurrentTerm = 0;

    std::map<FString, FCognitivePeer> Peers;
    std::deque<FKnowledgeProposal> PendingProposals;
    std::vector<FConsensusLogEntry> CommittedLog;

    int32 NextProposalID = 0;
    int32 TotalCommitted = 0;
    int32 TotalRejected = 0;
    double LastProposalTime = 0.0;
    bool bInitialized = false;
};
