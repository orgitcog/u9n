/*
 * DeepTreeEcho/Executive/AgentMessageBus.h
 *
 * Multi-agent message bus for Deep Tree Echo (Gap D of the DTE
 * integration map): typed inter-agent channels with STI-weighted
 * delivery, adapted from cogpy/cogdiod's LimboChannel / CogMessage /
 * CogScheduler patterns (9P/DisTyx cognitive micro-kernel) into
 * header-only C++17.
 *
 * The C substrate (Dis VM isolates, pthread channel buffers, 9P
 * namespace) is replaced by std::deque channels guarded by a mutex,
 * while the cognitive message taxonomy (MSG_UPDATE_TV, MSG_ATTEND,
 * MSG_INFER, MSG_QUERY, MSG_SPAWN, MSG_DESTROY, MSG_SOURCE_CHANGED,
 * MSG_CUSTOM), TruthValue/AttentionValue payloads, and STI-weighted
 * scheduling priority are preserved. This is the standalone core that
 * an aphroditecho-style parallel inference orchestrator or a UE
 * subsystem can wrap for multi-agent scale.
 */

#ifndef DTE_EXECUTIVE_AGENT_MESSAGE_BUS_H
#define DTE_EXECUTIVE_AGENT_MESSAGE_BUS_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace dte
{

// -----------------------------------------------------------------
// Primitive cognitive types (from cogdiod.h, verbatim semantics)
// -----------------------------------------------------------------

/// SimpleTruthValue: strength in [0,1], confidence in [0,1].
struct TruthValue
{
    float strength = 0.0f;
    float confidence = 0.0f;
};

/// AttentionValue: STI drives scheduling priority, LTI drives persistence.
struct AttentionValue
{
    float sti = 0.0f;
    float lti = 0.0f;
};

/// Message taxonomy (values preserved from cogdiod MsgType).
enum class AgentMsgType : std::uint8_t {
    UpdateTV      = 0x01, // TruthValue changed
    Attend        = 0x02, // STI transfer
    Infer         = 0x03, // Trigger inference cycle
    Query         = 0x04, // Pattern query
    Spawn         = 0x05, // Request to spawn child agent
    Destroy       = 0x06, // Notify of impending teardown
    SourceChanged = 0x07, // Source agent state changed
    Custom        = 0xFF  // Agent-defined message
};

/// Inter-agent message (union payload from cogdiod CogMessage,
/// expressed as coexisting optional fields plus a raw payload).
struct AgentMessage
{
    AgentMsgType type = AgentMsgType::Custom;
    std::uint64_t sender = 0;
    TruthValue tv{};
    AttentionValue av{};
    std::array<std::uint8_t, 56> payload{};
    std::size_t payload_size = 0;

    void setPayload(const void* data, std::size_t n)
    {
        payload_size = std::min(n, payload.size());
        const auto* p = static_cast<const std::uint8_t*>(data);
        std::copy(p, p + payload_size, payload.begin());
    }
};

// -----------------------------------------------------------------
// AgentMessageBus
// -----------------------------------------------------------------

/**
 * Registry of agents and typed point-to-point channels. Delivery order
 * across agents is STI-weighted (cogdiod CogScheduler semantics): when
 * pumping, agents with higher STI drain their inboxes first.
 */
class AgentMessageBus
{
public:
    static constexpr std::size_t kChannelBufMax = 64; // CHANNEL_BUF_MAX

    using AgentId = std::uint64_t;
    using Handler = std::function<void(const AgentMessage&)>;

    /// Register an agent with an initial attention value and a message
    /// handler. Returns its id.
    AgentId registerAgent(const std::string& name,
                          AttentionValue av = {},
                          Handler handler = nullptr)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        AgentId id = ++_id_counter;
        _agents[id] = AgentRec{name, av, std::move(handler), {}};
        return id;
    }

    bool unregisterAgent(AgentId id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _agents.erase(id) > 0;
    }

    std::size_t agentCount() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _agents.size();
    }

    /// Set/transfer attention (MSG_ATTEND semantics).
    bool setAttention(AgentId id, AttentionValue av)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _agents.find(id);
        if (it == _agents.end()) return false;
        it->second.av = av;
        return true;
    }

    std::optional<AttentionValue> getAttention(AgentId id) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _agents.find(id);
        if (it == _agents.end()) return std::nullopt;
        return it->second.av;
    }

    /**
     * Send a message to an agent's inbox. Returns false if the target
     * does not exist or its inbox is full (bounded channel semantics,
     * CHANNEL_BUF_MAX from cogdiod).
     */
    bool send(AgentId target, AgentMessage msg)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _agents.find(target);
        if (it == _agents.end()) return false;
        if (it->second.inbox.size() >= kChannelBufMax) return false;
        it->second.inbox.push_back(std::move(msg));
        return true;
    }

    /// Broadcast to all agents except the sender.
    std::size_t broadcast(AgentId sender, const AgentMessage& msg)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::size_t delivered = 0;
        for (auto& [id, rec] : _agents) {
            if (id == sender) continue;
            if (rec.inbox.size() >= kChannelBufMax) continue;
            rec.inbox.push_back(msg);
            ++delivered;
        }
        return delivered;
    }

    std::size_t inboxSize(AgentId id) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _agents.find(id);
        return (it == _agents.end()) ? 0 : it->second.inbox.size();
    }

    /**
     * STI-weighted pump: drain up to max_per_agent messages per agent,
     * visiting agents in descending STI order (CogScheduler priority).
     * Messages are dispatched to each agent's registered handler.
     *
     * @return total messages delivered this pump
     */
    std::size_t pump(std::size_t max_per_agent = kChannelBufMax)
    {
        // Snapshot agent order by STI under lock, then dispatch without
        // holding the lock during handler execution.
        std::vector<AgentId> order;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            order.reserve(_agents.size());
            for (const auto& [id, rec] : _agents) order.push_back(id);
            std::sort(order.begin(), order.end(),
                      [this](AgentId a, AgentId b) {
                          return _agents.at(a).av.sti > _agents.at(b).av.sti;
                      });
        }

        std::size_t total = 0;
        for (AgentId id : order) {
            for (std::size_t n = 0; n < max_per_agent; ++n) {
                AgentMessage msg;
                Handler handler;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    auto it = _agents.find(id);
                    if (it == _agents.end() || it->second.inbox.empty())
                        break;
                    msg = std::move(it->second.inbox.front());
                    it->second.inbox.pop_front();
                    handler = it->second.handler;
                }
                if (handler) handler(msg);
                ++total;
            }
        }
        return total;
    }

private:
    struct AgentRec
    {
        std::string name;
        AttentionValue av{};
        Handler handler;
        std::deque<AgentMessage> inbox;
    };

    mutable std::mutex _mutex;
    std::map<AgentId, AgentRec> _agents;
    AgentId _id_counter = 0;
};

} // namespace dte

#endif // DTE_EXECUTIVE_AGENT_MESSAGE_BUS_H
