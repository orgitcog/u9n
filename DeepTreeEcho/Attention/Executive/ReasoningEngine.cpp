/*
// Vendored from cogpy/cog-zero standalone cog0 (Gap B/C: forward-chaining reasoning over AtomStore).
 * standalone/src/ReasoningEngine.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <sstream>

#include "Cog0Logger.h"
#include "ReasoningEngine.h"

namespace cog0 {

ReasoningEngine::ReasoningEngine(std::shared_ptr<AtomStore> store)
    : _store(std::move(store)) {}

void ReasoningEngine::addRule(InferenceRule::Ptr rule) {
    _rules.push_back(std::move(rule));
    // Keep sorted by descending priority
    std::stable_sort(_rules.begin(), _rules.end(),
        [](const InferenceRule::Ptr& a, const InferenceRule::Ptr& b) {
            return a->priority > b->priority;
        });
}

void ReasoningEngine::addRule(std::string name,
                               InferenceRule::Condition cond,
                               InferenceRule::Action act,
                               double priority) {
    addRule(std::make_shared<InferenceRule>(
        std::move(name), std::move(cond), std::move(act), priority));
}

void ReasoningEngine::removeRule(const std::string& name) {
    _rules.erase(
        std::remove_if(_rules.begin(), _rules.end(),
            [&name](const InferenceRule::Ptr& r) { return r->name == name; }),
        _rules.end());
}

std::vector<InferenceResult> ReasoningEngine::runCycle() {
    std::vector<InferenceResult> results;
    for (auto& rule : _rules) {
        InferenceResult res;
        res.ruleName = rule->name;
        if (rule->condition && rule->condition(*_store)) {
            res.fired = true;
            if (rule->action) rule->action(*_store);
            ++rule->fireCount;
            ++_totalFirings;
            res.notes = "fired (total=" + std::to_string(rule->fireCount) + ")";
            logger().debug("[ReasoningEngine] Rule fired: " + rule->name);
        }
        results.push_back(std::move(res));
    }
    return results;
}

size_t ReasoningEngine::runForwardChaining(size_t maxCycles) {
    size_t totalFired = 0;
    for (size_t cycle = 0; cycle < maxCycles || maxCycles == 0; ++cycle) {
        auto results = runCycle();
        size_t firedThisCycle = 0;
        for (const auto& r : results)
            if (r.fired) ++firedThisCycle;
        totalFired += firedThisCycle;
        if (firedThisCycle == 0) break; // fixed point
        if (maxCycles > 0 && cycle + 1 >= maxCycles) break;
    }
    return totalFired;
}

bool ReasoningEngine::queryExists(AtomType t, const std::string& name) const {
    return _store->getNode(t, name) != nullptr;
}

bool ReasoningEngine::queryInherits(const std::string& child,
                                    const std::string& parent) const {
    auto childAtom  = _store->getNode(AtomType::CONCEPT, child);
    auto parentAtom = _store->getNode(AtomType::CONCEPT, parent);
    if (!childAtom || !parentAtom) return false;
    auto link = _store->getLink(AtomType::INHERITANCE, {childAtom, parentAtom});
    if (!link) return false;
    return link->tv().strength >= 0.5;
}

bool ReasoningEngine::queryEval(const std::string& pred,
                                 const std::string& arg,
                                 double threshold) const {
    auto predAtom = _store->getNode(AtomType::PREDICATE, pred);
    auto argAtom  = _store->getNode(AtomType::CONCEPT, arg);
    if (!predAtom || !argAtom) return false;
    auto listLink = _store->getLink(AtomType::LIST, {argAtom});
    if (!listLink) return false;
    auto evalLink = _store->getLink(AtomType::EVALUATION, {predAtom, listLink});
    if (!evalLink) return false;
    return evalLink->tv().strength >= threshold;
}

std::string ReasoningEngine::statusReport() const {
    std::ostringstream oss;
    oss << "=== ReasoningEngine Status ===\n";
    oss << "Rules (" << _rules.size() << "):\n";
    for (const auto& r : _rules) {
        oss << "  [" << r->name << "] prio=" << r->priority
            << " fired=" << r->fireCount << "\n";
    }
    oss << "Total firings: " << _totalFirings << "\n";
    oss << "AtomStore size: " << _store->size() << "\n";
    return oss.str();
}

} // namespace cog0
