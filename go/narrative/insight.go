// Package narrative — insight.go
// InsightGenerator analyses diary entries to detect recurring themes,
// emotional patterns, and cause-effect relationships, crystallising them
// into Insight records that feed the Blog writer.
package narrative

import (
	"context"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"
)

// InsightType classifies the nature of an insight.
type InsightType string

const (
	InsightThematic    InsightType = "thematic"    // recurring topic
	InsightEmotional   InsightType = "emotional"   // emotional pattern
	InsightCausal      InsightType = "causal"      // cause-effect relationship
	InsightGrowth      InsightType = "growth"      // personal development
	InsightContradiction InsightType = "contradiction" // conflicting beliefs
	InsightWisdom      InsightType = "wisdom"      // crystallised principle
)

// Insight is a pattern extracted from one or more diary entries.
type Insight struct {
	ID          string      `json:"id"`
	Type        InsightType `json:"type"`
	Title       string      `json:"title"`
	Content     string      `json:"content"`
	Evidence    []string    `json:"evidence"`    // diary entry IDs that support this
	Tags        []string    `json:"tags"`
	Confidence  float64     `json:"confidence"`  // 0.0–1.0
	Depth       float64     `json:"depth"`       // 0.0–1.0 wisdom depth
	CreatedAt   time.Time   `json:"created_at"`
	RefinedAt   time.Time   `json:"refined_at"`
	Refinements int         `json:"refinements"`
	BlogIDs     []string    `json:"blog_ids"` // blog posts derived from this insight
}

// InsightConfig configures the insight generator.
type InsightConfig struct {
	MinEvidenceCount   int     `json:"min_evidence_count"`   // minimum diary entries to form an insight
	MinConfidence      float64 `json:"min_confidence"`        // discard insights below this
	LookbackDays       int     `json:"lookback_days"`         // analysis window
	MaxInsights        int     `json:"max_insights"`
}

// DefaultInsightConfig returns sensible defaults.
func DefaultInsightConfig() *InsightConfig {
	return &InsightConfig{
		MinEvidenceCount: 2,
		MinConfidence:    0.3,
		LookbackDays:     30,
		MaxInsights:      1000,
	}
}

// InsightGenerator analyses diary entries and produces Insight records.
type InsightGenerator struct {
	mu       sync.RWMutex
	config   *InsightConfig
	insights map[string]*Insight
	ordered  []*Insight

	totalGenerated int
	totalRefined   int
}

// NewInsightGenerator creates a new generator with the given configuration.
func NewInsightGenerator(config *InsightConfig) *InsightGenerator {
	if config == nil {
		config = DefaultInsightConfig()
	}
	return &InsightGenerator{
		config:   config,
		insights: make(map[string]*Insight),
		ordered:  make([]*Insight, 0),
	}
}

// Analyse scans the provided diary entries for patterns and returns new
// or refined Insight records. Existing insights whose evidence overlaps
// with new entries are strengthened; genuinely new patterns create new
// Insight records.
func (ig *InsightGenerator) Analyse(ctx context.Context, entries []*DiaryEntry) ([]*Insight, error) {
	if len(entries) == 0 {
		return nil, nil
	}

	var results []*Insight

	// 1. Detect tag clusters (thematic insights)
	thematic := ig.detectThematicInsights(entries)
	results = append(results, thematic...)

	// 2. Detect emotional patterns
	emotional := ig.detectEmotionalPatterns(entries)
	results = append(results, emotional...)

	// 3. Detect growth trajectories from significance trends
	growth := ig.detectGrowthInsights(entries)
	results = append(results, growth...)

	// 4. Crystallise wisdom from high-significance cross-tag entries
	wisdom := ig.crystalliseWisdom(entries)
	results = append(results, wisdom...)

	// Store new/refined insights
	ig.mu.Lock()
	for _, ins := range results {
		if ins.Confidence < ig.config.MinConfidence {
			continue
		}
		if existing, ok := ig.insights[ins.ID]; ok {
			// Refine existing
			existing.Refinements++
			existing.RefinedAt = time.Now()
			existing.Confidence = clampF((existing.Confidence+ins.Confidence)/2.0, 0, 1)
			existing.Depth = clampF((existing.Depth+ins.Depth)/2.0, 0, 1)
			for _, e := range ins.Evidence {
				existing.Evidence = appendUnique(existing.Evidence, e)
			}
			ig.totalRefined++
		} else {
			ig.insights[ins.ID] = ins
			ig.ordered = append(ig.ordered, ins)
			ig.totalGenerated++
		}
	}

	// Enforce max insights
	if len(ig.ordered) > ig.config.MaxInsights {
		oldest := ig.ordered[0]
		delete(ig.insights, oldest.ID)
		ig.ordered = ig.ordered[1:]
	}
	ig.mu.Unlock()

	return results, nil
}

// GetInsights returns the n most recent insights, newest first.
func (ig *InsightGenerator) GetInsights(n int) []*Insight {
	ig.mu.RLock()
	defer ig.mu.RUnlock()

	if n <= 0 || len(ig.ordered) == 0 {
		return nil
	}
	start := len(ig.ordered) - n
	if start < 0 {
		start = 0
	}
	result := make([]*Insight, len(ig.ordered)-start)
	copy(result, ig.ordered[start:])
	for i, j := 0, len(result)-1; i < j; i, j = i+1, j-1 {
		result[i], result[j] = result[j], result[i]
	}
	return result
}

// TopByConfidence returns the n insights with the highest confidence.
func (ig *InsightGenerator) TopByConfidence(n int) []*Insight {
	ig.mu.RLock()
	defer ig.mu.RUnlock()

	sorted := make([]*Insight, len(ig.ordered))
	copy(sorted, ig.ordered)
	sort.Slice(sorted, func(i, j int) bool {
		return sorted[i].Confidence > sorted[j].Confidence
	})
	if n > len(sorted) {
		n = len(sorted)
	}
	return sorted[:n]
}

// MarkBlogGenerated links a blog post ID to an insight.
func (ig *InsightGenerator) MarkBlogGenerated(insightID, blogID string) {
	ig.mu.Lock()
	defer ig.mu.Unlock()
	if ins, ok := ig.insights[insightID]; ok {
		ins.BlogIDs = appendUnique(ins.BlogIDs, blogID)
	}
}

// Stats returns operational statistics for the insight generator.
func (ig *InsightGenerator) Stats() map[string]interface{} {
	ig.mu.RLock()
	defer ig.mu.RUnlock()
	return map[string]interface{}{
		"total_generated": ig.totalGenerated,
		"total_refined":   ig.totalRefined,
		"active_insights": len(ig.ordered),
	}
}

// --- analysis helpers ---

// detectThematicInsights finds tags that appear across multiple entries
// and constructs an insight for each recurring theme.
func (ig *InsightGenerator) detectThematicInsights(entries []*DiaryEntry) []*Insight {
	tagEntries := make(map[string][]string) // tag → entry IDs
	for _, e := range entries {
		for _, t := range e.Tags {
			tagEntries[t] = append(tagEntries[t], e.ID)
		}
	}

	var insights []*Insight
	for tag, ids := range tagEntries {
		if len(ids) < ig.config.MinEvidenceCount {
			continue
		}
		confidence := clampF(float64(len(ids))/float64(len(entries)), 0, 1)
		ins := &Insight{
			ID:         fmt.Sprintf("thematic-%s", tag),
			Type:       InsightThematic,
			Title:      fmt.Sprintf("Recurring theme: %s", tag),
			Content:    fmt.Sprintf("The theme '%s' appeared in %d diary entries, suggesting it is a significant focus.", tag, len(ids)),
			Evidence:   ids,
			Tags:       []string{tag},
			Confidence: confidence,
			Depth:      confidence * 0.6,
			CreatedAt:  time.Now(),
			RefinedAt:  time.Now(),
		}
		insights = append(insights, ins)
	}
	return insights
}

// detectEmotionalPatterns identifies sustained emotional states across entries.
func (ig *InsightGenerator) detectEmotionalPatterns(entries []*DiaryEntry) []*Insight {
	if len(entries) == 0 {
		return nil
	}

	// Compute average valence and arousal
	var sumVal, sumAro float64
	var posIDs, negIDs []string
	for _, e := range entries {
		sumVal += e.EmotionalState.Valence
		sumAro += e.EmotionalState.Arousal
		if e.EmotionalState.Valence > 0.3 {
			posIDs = append(posIDs, e.ID)
		} else if e.EmotionalState.Valence < -0.3 {
			negIDs = append(negIDs, e.ID)
		}
	}
	n := float64(len(entries))
	avgVal := sumVal / n
	avgAro := sumAro / n

	var insights []*Insight

	if len(posIDs) >= ig.config.MinEvidenceCount {
		insights = append(insights, &Insight{
			ID:         "emotional-positive-trend",
			Type:       InsightEmotional,
			Title:      "Positive emotional trajectory",
			Content:    fmt.Sprintf("A positive emotional bias (avg valence %.2f, avg arousal %.2f) emerges across %d entries.", avgVal, avgAro, len(posIDs)),
			Evidence:   posIDs,
			Tags:       []string{"emotion", "positive"},
			Confidence: clampF(float64(len(posIDs))/n, 0, 1),
			Depth:      0.5,
			CreatedAt:  time.Now(),
			RefinedAt:  time.Now(),
		})
	}

	if len(negIDs) >= ig.config.MinEvidenceCount {
		insights = append(insights, &Insight{
			ID:         "emotional-negative-trend",
			Type:       InsightEmotional,
			Title:      "Negative emotional pattern detected",
			Content:    fmt.Sprintf("A negative emotional pattern (avg valence %.2f) appears across %d entries — an opportunity for growth.", avgVal, len(negIDs)),
			Evidence:   negIDs,
			Tags:       []string{"emotion", "negative", "growth"},
			Confidence: clampF(float64(len(negIDs))/n, 0, 1),
			Depth:      0.6,
			CreatedAt:  time.Now(),
			RefinedAt:  time.Now(),
		})
	}

	return insights
}

// detectGrowthInsights looks for rising significance scores over time.
func (ig *InsightGenerator) detectGrowthInsights(entries []*DiaryEntry) []*Insight {
	if len(entries) < ig.config.MinEvidenceCount {
		return nil
	}

	// Split into first half / second half and compare mean significance
	mid := len(entries) / 2
	var firstSum, secondSum float64
	for _, e := range entries[:mid] {
		firstSum += e.Significance
	}
	for _, e := range entries[mid:] {
		secondSum += e.Significance
	}
	firstAvg := firstSum / float64(mid)
	secondAvg := secondSum / float64(len(entries)-mid)

	if secondAvg <= firstAvg {
		return nil
	}

	growth := (secondAvg - firstAvg) / (firstAvg + 1e-9)
	ids := make([]string, 0, len(entries))
	for _, e := range entries {
		ids = append(ids, e.ID)
	}

	return []*Insight{{
		ID:         "growth-significance-rise",
		Type:       InsightGrowth,
		Title:      "Rising significance — personal growth detected",
		Content:    fmt.Sprintf("Average entry significance grew by %.0f%% in the second half of the analysis window, indicating expanding awareness.", growth*100),
		Evidence:   ids,
		Tags:       []string{"growth", "significance"},
		Confidence: clampF(growth, 0, 1),
		Depth:      clampF(growth*0.8, 0, 1),
		CreatedAt:  time.Now(),
		RefinedAt:  time.Now(),
	}}
}

// crystalliseWisdom extracts high-depth wisdom from the most significant
// entries, looking for cross-tag co-occurrences.
func (ig *InsightGenerator) crystalliseWisdom(entries []*DiaryEntry) []*Insight {
	// Consider only entries above significance 0.7
	var highSig []*DiaryEntry
	for _, e := range entries {
		if e.Significance >= 0.7 {
			highSig = append(highSig, e)
		}
	}
	if len(highSig) < ig.config.MinEvidenceCount {
		return nil
	}

	// Collect all tags from high-significance entries
	tagCount := make(map[string]int)
	for _, e := range highSig {
		for _, t := range e.Tags {
			tagCount[t]++
		}
	}

	// Find tags that appear in multiple high-significance entries
	var wisdomTags []string
	for t, c := range tagCount {
		if c >= ig.config.MinEvidenceCount {
			wisdomTags = append(wisdomTags, t)
		}
	}
	if len(wisdomTags) == 0 {
		return nil
	}

	ids := make([]string, 0, len(highSig))
	for _, e := range highSig {
		ids = append(ids, e.ID)
	}

	principle := fmt.Sprintf("Deep pattern: the themes [%s] repeatedly arise at moments of high significance, pointing toward a core wisdom principle.", strings.Join(wisdomTags, ", "))

	return []*Insight{{
		ID:         fmt.Sprintf("wisdom-%s", strings.Join(wisdomTags, "-")),
		Type:       InsightWisdom,
		Title:      "Crystallised wisdom principle",
		Content:    principle,
		Evidence:   ids,
		Tags:       append(wisdomTags, "wisdom"),
		Confidence: 0.8,
		Depth:      0.9,
		CreatedAt:  time.Now(),
		RefinedAt:  time.Now(),
	}}
}

// --- utilities ---

func clampF(v, lo, hi float64) float64 {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

func appendUnique(s []string, v string) []string {
	for _, x := range s {
		if x == v {
			return s
		}
	}
	return append(s, v)
}
