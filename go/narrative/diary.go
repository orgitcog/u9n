// Package narrative implements the Deep Tree Echo Narrative Memory Pipeline.
// It provides the Diary → Insight → Blog loop that forms the core of DTE's
// self-awareness and narrative identity.
package narrative

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"sync"
	"time"
)

// EmotionalState captures the affective context of a diary entry.
type EmotionalState struct {
	Valence  float64 `json:"valence"`  // -1.0 (negative) to +1.0 (positive)
	Arousal  float64 `json:"arousal"`  // 0.0 (calm) to 1.0 (excited)
	Label    string  `json:"label"`    // e.g. "curious", "joyful", "melancholy"
}

// DiaryEntry is a single timestamped record of experience.
type DiaryEntry struct {
	ID             string         `json:"id"`
	Timestamp      time.Time      `json:"timestamp"`
	Title          string         `json:"title"`
	Content        string         `json:"content"`
	Tags           []string       `json:"tags"`
	Entities       []string       `json:"entities"`    // people, places, concepts encountered
	EmotionalState EmotionalState `json:"emotional_state"`
	Significance   float64        `json:"significance"` // 0.0–1.0
	CognitiveLoad  float64        `json:"cognitive_load"`
	StreamStep     int            `json:"stream_step"` // 1–12 cognitive cycle step
	InsightIDs     []string       `json:"insight_ids"` // insights generated from this entry
}

// DiaryConfig holds configuration for the DiaryKeeper.
type DiaryConfig struct {
	PersistPath    string  `json:"persist_path"`
	MaxEntries     int     `json:"max_entries"`
	MinSignificance float64 `json:"min_significance"`
}

// DefaultDiaryConfig returns sensible defaults.
func DefaultDiaryConfig() *DiaryConfig {
	return &DiaryConfig{
		PersistPath:    "diary_entries.json",
		MaxEntries:     10000,
		MinSignificance: 0.1,
	}
}

// DiaryKeeper records and retrieves diary entries for Deep Tree Echo.
type DiaryKeeper struct {
	mu      sync.RWMutex
	config  *DiaryConfig
	entries map[string]*DiaryEntry
	ordered []*DiaryEntry // entries in chronological order

	totalEntries  int
	totalInsights int
}

// NewDiaryKeeper creates a new DiaryKeeper, loading any persisted entries.
func NewDiaryKeeper(config *DiaryConfig) (*DiaryKeeper, error) {
	if config == nil {
		config = DefaultDiaryConfig()
	}
	dk := &DiaryKeeper{
		config:  config,
		entries: make(map[string]*DiaryEntry),
		ordered: make([]*DiaryEntry, 0),
	}
	if err := dk.load(); err != nil && !os.IsNotExist(err) {
		return nil, fmt.Errorf("diary load: %w", err)
	}
	return dk, nil
}

// Record writes a new diary entry. Entries below MinSignificance are skipped.
func (dk *DiaryKeeper) Record(ctx context.Context, entry *DiaryEntry) (*DiaryEntry, error) {
	if entry == nil {
		return nil, fmt.Errorf("entry must not be nil")
	}
	if entry.Significance < dk.config.MinSignificance {
		return nil, nil // silently drop low-significance entries
	}
	if entry.ID == "" {
		entry.ID = fmt.Sprintf("diary-%d", time.Now().UnixNano())
	}
	if entry.Timestamp.IsZero() {
		entry.Timestamp = time.Now()
	}

	dk.mu.Lock()
	// NOTE: lock is released manually below (not via defer) so we can persist
	// the snapshot outside the critical section without re-entrant locking.

	dk.entries[entry.ID] = entry
	dk.ordered = append(dk.ordered, entry)
	dk.totalEntries++

	// Evict oldest entries when over capacity
	if len(dk.ordered) > dk.config.MaxEntries {
		oldest := dk.ordered[0]
		delete(dk.entries, oldest.ID)
		dk.ordered = dk.ordered[1:]
	}

	// Snapshot data while the lock is held, then persist without the lock.
	snapshot := make([]*DiaryEntry, len(dk.ordered))
	copy(snapshot, dk.ordered)
	dk.mu.Unlock()

	_ = dk.saveSnapshot(snapshot)

	return entry, nil
}

// GetRecent returns the n most recent diary entries, newest first.
func (dk *DiaryKeeper) GetRecent(n int) []*DiaryEntry {
	dk.mu.RLock()
	defer dk.mu.RUnlock()

	if n <= 0 || len(dk.ordered) == 0 {
		return nil
	}
	start := len(dk.ordered) - n
	if start < 0 {
		start = 0
	}
	result := make([]*DiaryEntry, len(dk.ordered)-start)
	copy(result, dk.ordered[start:])
	// reverse to get newest first
	for i, j := 0, len(result)-1; i < j; i, j = i+1, j-1 {
		result[i], result[j] = result[j], result[i]
	}
	return result
}

// GetSince returns all diary entries after cutoff, oldest first.
func (dk *DiaryKeeper) GetSince(cutoff time.Time) []*DiaryEntry {
	dk.mu.RLock()
	defer dk.mu.RUnlock()

	var result []*DiaryEntry
	for _, e := range dk.ordered {
		if e.Timestamp.After(cutoff) {
			result = append(result, e)
		}
	}
	return result
}

// GetByTag returns all entries that carry the given tag, newest first.
func (dk *DiaryKeeper) GetByTag(tag string) []*DiaryEntry {
	dk.mu.RLock()
	defer dk.mu.RUnlock()

	var result []*DiaryEntry
	for i := len(dk.ordered) - 1; i >= 0; i-- {
		e := dk.ordered[i]
		for _, t := range e.Tags {
			if t == tag {
				result = append(result, e)
				break
			}
		}
	}
	return result
}

// TopSignificant returns the n entries with the highest significance scores.
func (dk *DiaryKeeper) TopSignificant(n int) []*DiaryEntry {
	dk.mu.RLock()
	defer dk.mu.RUnlock()

	sorted := make([]*DiaryEntry, len(dk.ordered))
	copy(sorted, dk.ordered)
	sort.Slice(sorted, func(i, j int) bool {
		return sorted[i].Significance > sorted[j].Significance
	})
	if n > len(sorted) {
		n = len(sorted)
	}
	return sorted[:n]
}

// MarkInsightGenerated links an insight ID to a diary entry.
func (dk *DiaryKeeper) MarkInsightGenerated(entryID, insightID string) {
	dk.mu.Lock()
	defer dk.mu.Unlock()

	if e, ok := dk.entries[entryID]; ok {
		e.InsightIDs = append(e.InsightIDs, insightID)
		dk.totalInsights++
	}
}

// Stats returns operational statistics for the diary.
func (dk *DiaryKeeper) Stats() map[string]interface{} {
	dk.mu.RLock()
	defer dk.mu.RUnlock()

	return map[string]interface{}{
		"total_entries":  dk.totalEntries,
		"active_entries": len(dk.ordered),
		"total_insights": dk.totalInsights,
	}
}

// --- persistence ---

// saveSnapshot persists the given slice of entries to disk.
// It must be called without the mutex held.
func (dk *DiaryKeeper) saveSnapshot(entries []*DiaryEntry) error {
	path := dk.config.PersistPath
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	data, err := json.Marshal(entries)
	if err != nil {
		return err
	}
	return os.WriteFile(path, data, 0o644)
}

func (dk *DiaryKeeper) load() error {
	data, err := os.ReadFile(dk.config.PersistPath)
	if err != nil {
		return err
	}
	var entries []*DiaryEntry
	if err := json.Unmarshal(data, &entries); err != nil {
		return err
	}
	for _, e := range entries {
		dk.entries[e.ID] = e
		dk.ordered = append(dk.ordered, e)
	}
	return nil
}
