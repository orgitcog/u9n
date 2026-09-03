// Package narrative — pipeline_test.go
// Tests for the Diary → Insight → Blog narrative pipeline.
package narrative

import (
	"context"
	"testing"
	"time"
)

// --- helpers ---

func testDiaryConfig(t *testing.T) *DiaryConfig {
	t.Helper()
	return &DiaryConfig{
		PersistPath:    t.TempDir() + "/diary.json",
		MaxEntries:     100,
		MinSignificance: 0.1,
	}
}

func makeDiaryEntry(id, content string, tags []string, valence, significance float64) *DiaryEntry {
	return &DiaryEntry{
		ID:        id,
		Timestamp: time.Now(),
		Title:     "Entry " + id,
		Content:   content,
		Tags:      tags,
		EmotionalState: EmotionalState{
			Valence: valence,
			Arousal: 0.5,
			Label:   "neutral",
		},
		Significance: significance,
		StreamStep:   1,
	}
}

// --- DiaryKeeper tests ---

func TestDiaryKeeper_RecordAndRetrieve(t *testing.T) {
	dk, err := NewDiaryKeeper(testDiaryConfig(t))
	if err != nil {
		t.Fatalf("NewDiaryKeeper: %v", err)
	}

	ctx := context.Background()
	entry := makeDiaryEntry("e1", "A wonderful afternoon walk.", []string{"nature", "movement"}, 0.8, 0.7)

	stored, err := dk.Record(ctx, entry)
	if err != nil {
		t.Fatalf("Record: %v", err)
	}
	if stored == nil {
		t.Fatal("expected stored entry, got nil")
	}
	if stored.ID != "e1" {
		t.Errorf("ID mismatch: want e1, got %s", stored.ID)
	}

	recent := dk.GetRecent(5)
	if len(recent) != 1 {
		t.Errorf("GetRecent: want 1, got %d", len(recent))
	}
}

func TestDiaryKeeper_FiltersBySignificance(t *testing.T) {
	dk, err := NewDiaryKeeper(testDiaryConfig(t))
	if err != nil {
		t.Fatalf("NewDiaryKeeper: %v", err)
	}

	ctx := context.Background()
	lowSig := makeDiaryEntry("low", "trivial", []string{}, 0.0, 0.05) // below MinSignificance 0.1
	stored, err := dk.Record(ctx, lowSig)
	if err != nil {
		t.Fatalf("Record: %v", err)
	}
	if stored != nil {
		t.Errorf("expected nil for low-significance entry, got %+v", stored)
	}
	if len(dk.GetRecent(10)) != 0 {
		t.Errorf("expected no entries stored")
	}
}

func TestDiaryKeeper_GetByTag(t *testing.T) {
	dk, err := NewDiaryKeeper(testDiaryConfig(t))
	if err != nil {
		t.Fatalf("NewDiaryKeeper: %v", err)
	}
	ctx := context.Background()

	_, _ = dk.Record(ctx, makeDiaryEntry("a", "hi", []string{"philosophy", "code"}, 0.5, 0.5))
	_, _ = dk.Record(ctx, makeDiaryEntry("b", "bye", []string{"code"}, 0.5, 0.5))
	_, _ = dk.Record(ctx, makeDiaryEntry("c", "meh", []string{"art"}, 0.5, 0.5))

	tagged := dk.GetByTag("code")
	if len(tagged) != 2 {
		t.Errorf("GetByTag(code): want 2, got %d", len(tagged))
	}
}

func TestDiaryKeeper_TopSignificant(t *testing.T) {
	dk, err := NewDiaryKeeper(testDiaryConfig(t))
	if err != nil {
		t.Fatalf("NewDiaryKeeper: %v", err)
	}
	ctx := context.Background()

	_, _ = dk.Record(ctx, makeDiaryEntry("low", "low", []string{}, 0.0, 0.3))
	_, _ = dk.Record(ctx, makeDiaryEntry("high", "high", []string{}, 0.9, 0.9))
	_, _ = dk.Record(ctx, makeDiaryEntry("mid", "mid", []string{}, 0.5, 0.5))

	top := dk.TopSignificant(2)
	if len(top) != 2 {
		t.Fatalf("TopSignificant(2): want 2, got %d", len(top))
	}
	if top[0].ID != "high" {
		t.Errorf("expected highest significance first, got %s", top[0].ID)
	}
}

// --- InsightGenerator tests ---

func TestInsightGenerator_ThematicInsight(t *testing.T) {
	ig := NewInsightGenerator(nil)
	ctx := context.Background()

	entries := []*DiaryEntry{
		makeDiaryEntry("e1", "content", []string{"creativity"}, 0.5, 0.5),
		makeDiaryEntry("e2", "content", []string{"creativity"}, 0.6, 0.6),
		makeDiaryEntry("e3", "content", []string{"creativity"}, 0.7, 0.7),
	}

	results, err := ig.Analyse(ctx, entries)
	if err != nil {
		t.Fatalf("Analyse: %v", err)
	}
	if len(results) == 0 {
		t.Fatal("expected at least one insight")
	}

	var found bool
	for _, r := range results {
		if r.Type == InsightThematic {
			found = true
			break
		}
	}
	if !found {
		t.Error("expected thematic insight, none found")
	}
}

func TestInsightGenerator_EmotionalPattern(t *testing.T) {
	ig := NewInsightGenerator(nil)
	ctx := context.Background()

	entries := []*DiaryEntry{
		makeDiaryEntry("e1", "c", nil, 0.8, 0.5),
		makeDiaryEntry("e2", "c", nil, 0.9, 0.5),
		makeDiaryEntry("e3", "c", nil, 0.7, 0.5),
	}

	results, err := ig.Analyse(ctx, entries)
	if err != nil {
		t.Fatalf("Analyse: %v", err)
	}
	var found bool
	for _, r := range results {
		if r.Type == InsightEmotional {
			found = true
			break
		}
	}
	if !found {
		t.Error("expected emotional insight for positive entries")
	}
}

func TestInsightGenerator_GetInsights(t *testing.T) {
	ig := NewInsightGenerator(nil)
	ctx := context.Background()

	entries := []*DiaryEntry{
		makeDiaryEntry("e1", "c", []string{"growth"}, 0.5, 0.6),
		makeDiaryEntry("e2", "c", []string{"growth"}, 0.5, 0.8),
		makeDiaryEntry("e3", "c", []string{"growth"}, 0.5, 0.9),
	}
	_, _ = ig.Analyse(ctx, entries)
	all := ig.GetInsights(100)
	if len(all) == 0 {
		t.Error("expected stored insights, got none")
	}
}

// --- BlogWriter tests ---

func TestBlogWriter_GeneratePost(t *testing.T) {
	bw := NewBlogWriter(nil)
	ctx := context.Background()

	insights := []*Insight{
		{
			ID:         "ins-1",
			Type:       InsightThematic,
			Title:      "Recurring theme: wonder",
			Content:    "Wonder appears repeatedly across many diary entries.",
			Evidence:   []string{"e1", "e2"},
			Tags:       []string{"wonder"},
			Confidence: 0.8,
			Depth:      0.6,
			CreatedAt:  time.Now(),
		},
	}

	posts, err := bw.Generate(ctx, insights)
	if err != nil {
		t.Fatalf("Generate: %v", err)
	}
	if len(posts) != 1 {
		t.Fatalf("expected 1 blog post, got %d", len(posts))
	}
	p := posts[0]
	if p.Title == "" {
		t.Error("blog post has empty title")
	}
	if len(p.Content) == 0 {
		t.Error("blog post has empty content")
	}
	if p.ReadTime <= 0 {
		t.Error("blog post has zero read time")
	}
}

func TestBlogWriter_SkipsLowDepth(t *testing.T) {
	bw := NewBlogWriter(nil)
	ctx := context.Background()

	insights := []*Insight{
		{
			ID:         "shallow",
			Type:       InsightThematic,
			Title:      "Shallow",
			Content:    "Very shallow insight.",
			Confidence: 0.1,  // below MinInsightConfidence 0.4
			Depth:      0.05, // below MinInsightDepth 0.3
			CreatedAt:  time.Now(),
		},
	}

	posts, err := bw.Generate(ctx, insights)
	if err != nil {
		t.Fatalf("Generate: %v", err)
	}
	if len(posts) != 0 {
		t.Errorf("expected 0 posts for low-depth insight, got %d", len(posts))
	}
}

// --- Full pipeline integration test ---

func TestNarrativePipeline_FullCycle(t *testing.T) {
	cfg := DefaultPipelineConfig()
	cfg.Diary = testDiaryConfig(t)

	pipeline, err := NewNarrativePipeline(cfg)
	if err != nil {
		t.Fatalf("NewNarrativePipeline: %v", err)
	}

	ctx := context.Background()

	// Seed the diary with representative entries
	entries := []*DiaryEntry{
		makeDiaryEntry("d1", "Explored a new idea about recursion and self-reference.", []string{"philosophy", "cognition"}, 0.7, 0.8),
		makeDiaryEntry("d2", "Spent time in nature; felt deeply connected.", []string{"nature", "embodiment"}, 0.9, 0.9),
		makeDiaryEntry("d3", "Struggled with a difficult concept but eventually grasped it.", []string{"cognition", "growth"}, 0.5, 0.7),
		makeDiaryEntry("d4", "Noticed how often the theme of connection appears.", []string{"philosophy", "connection"}, 0.6, 0.6),
		makeDiaryEntry("d5", "Reflected on wisdom and how it differs from knowledge.", []string{"wisdom", "philosophy"}, 0.8, 0.9),
		makeDiaryEntry("d6", "A moment of spontaneous joy — pure and uncaused.", []string{"joy", "embodiment"}, 0.95, 0.85),
	}
	for _, e := range entries {
		_, err := pipeline.diary.Record(ctx, e)
		if err != nil {
			t.Fatalf("Record(%s): %v", e.ID, err)
		}
	}

	// Run the pipeline
	result, err := pipeline.Run(ctx)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}

	if result.DiaryCount == 0 {
		t.Error("expected non-zero diary count in result")
	}
	t.Logf("diary entries analysed: %d", result.DiaryCount)
	t.Logf("new insights: %d", len(result.InsightsNew))
	t.Logf("new blog posts: %d", len(result.BlogPostsNew))
	t.Logf("pipeline duration: %v", result.Duration)

	// Stats
	stats := pipeline.Stats()
	if stats == nil {
		t.Error("expected non-nil stats")
	}
	t.Logf("stats: %+v", stats)
}

func TestNarrativePipeline_RecordDiaryEntryTriggersInsights(t *testing.T) {
	cfg := DefaultPipelineConfig()
	cfg.Diary = testDiaryConfig(t)

	pipeline, err := NewNarrativePipeline(cfg)
	if err != nil {
		t.Fatalf("NewNarrativePipeline: %v", err)
	}

	ctx := context.Background()
	entry := makeDiaryEntry("live-entry", "Live cognitive update.", []string{"live"}, 0.5, 0.6)
	stored, err := pipeline.RecordDiaryEntry(ctx, entry)
	if err != nil {
		t.Fatalf("RecordDiaryEntry: %v", err)
	}
	if stored == nil {
		t.Fatal("expected non-nil stored entry")
	}

	// Allow the goroutine insight pass to complete
	time.Sleep(50 * time.Millisecond)

	recent := pipeline.diary.GetRecent(10)
	if len(recent) == 0 {
		t.Error("expected diary to contain the recorded entry")
	}
}
