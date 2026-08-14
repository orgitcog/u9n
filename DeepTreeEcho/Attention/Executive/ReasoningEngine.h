/*
// Vendored from cogpy/cog-zero standalone cog0 (Gap B/C: forward-chaining reasoning over AtomStore).
 * standalone/include/cog0/ReasoningEngine.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Lightweight forward-chaining reasoning engine for the standalone cog0 agent.
 * Supports simple if/then rules over the AtomStore knowledge graph.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "AtomStore.h"

namespace cog0 {

// -----------------------------------------------------------------------
// InferenceRule — a named rule with a condition predicate and action

struct InferenceRule {
    using Ptr       = std::shared_ptr<InferenceRule>;
    using Condition = std::function<bool(const AtomStore&)>;
    using Action    = std::function<void(AtomStore&)>;

    std::string name;
    Condition   condition;
    Action      action;
    double      priority  = 1.0;
    size_t      fireCount = 0;

    InferenceRule(std::string n, Condition cond, Action act, double prio = 1.0)
        : name(std::move(n))
        , condition(std::move(cond))
        , action(std::move(act))
        , priority(prio) {}
};

// -----------------------------------------------------------------------
// InferenceResult — result of a single reasoning step

struct InferenceResult {
    std::string ruleName;
    bool        fired = false;
    std::string notes;
};

// -----------------------------------------------------------------------
// ReasoningEngine — manages rules and runs forward-chaining inference

class ReasoningEngine {
public:
    explicit ReasoningEngine(std::shared_ptr<AtomStore> store);

    // Rule registration
    void addRule(InferenceRule::Ptr rule);
    void addRule(std::string name,
                 InferenceRule::Condition cond,
                 InferenceRule::Action    act,
                 double priority = 1.0);
    void removeRule(const std::string& name);
    [[nodiscard]] const std::vector<InferenceRule::Ptr>& rules() const { return _rules; }

    // Execute one inference cycle — fire all applicable rules once.
    // Returns list of results (one per rule that fired).
    std::vector<InferenceResult> runCycle();

    // Run up to maxCycles forward-chaining passes until no new rules fire
    // or the limit is reached. Returns total number of firings.
    size_t runForwardChaining(size_t maxCycles = 10);

    // Query: does the store contain an atom of this type and name?
    [[nodiscard]] bool queryExists(AtomType t, const std::string& name) const;

    // Query: is the inheritance A -> B present?
    [[nodiscard]] bool queryInherits(const std::string& child, const std::string& parent) const;

    // Query: is the evaluation (pred, arg) present with strength >= threshold?
    [[nodiscard]] bool queryEval(const std::string& pred, const std::string& arg,
                   double threshold = 0.5) const;

    [[nodiscard]] std::string statusReport() const;

private:
    std::shared_ptr<AtomStore> _store;
    std::vector<InferenceRule::Ptr> _rules;
    size_t _totalFirings = 0;
};

} // namespace cog0
