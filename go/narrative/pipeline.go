// Package narrative — pipeline.go
// NarrativePipeline wires the three stages of the Diary → Insight → Blog loop
// into a single coherent system. It can be run on-demand or autonomously at a
// configurable cadence, and optionally persists generated posts to the
// vectormem HypergraphMemory for cross-system recall.
package narrative

import (
	"context"
	"fmt"
	"sync"
	"time"
)

// PipelineConfig controls the end-to-end narrative pipeline.
type PipelineConfig struct {
	Diary   *DiaryConfig
	Insight *InsightConfig
	Blog    *BlogConfig

	// How far back the pipeline looks when it runs.
	AnalysisWindowDays int

	// If > 0, the pipeline runs autonomously on this interval.
	AutoRunInterval time.Duration

	// Optional callback invoked after each successful run.
	OnRunComplete func(result *PipelineResult)
}

// DefaultPipelineConfig returns sensible defaults wiring the three stages.
func DefaultPipelineConfig() *PipelineConfig {
	return &PipelineConfig{
		Diary:              DefaultDiaryConfig(),
		Insight:            DefaultInsightConfig(),
		Blog:               DefaultBlogConfig(),
		AnalysisWindowDays: 30,
		AutoRunInterval:    0, // manual by default
	}
}

// PipelineResult summarises one execution of the Diary → Insight → Blog loop.
type PipelineResult struct {
	RunAt          time.Time
	DiaryCount     int
	InsightsNew    []*Insight
	BlogPostsNew   []*BlogPost
	Duration       time.Duration
	Errors         []error
}

// NarrativePipeline is the top-level orchestrator for Deep Tree Echo's
// self-narrative system.
type NarrativePipeline struct {
	config  *PipelineConfig
	diary   *DiaryKeeper
	insight *InsightGenerator
	blog    *BlogWriter

	mu      sync.Mutex
	running bool
	stopCh  chan struct{}
	wg      sync.WaitGroup

	lastResult *PipelineResult
}

// NewNarrativePipeline constructs and initialises the full pipeline.
func NewNarrativePipeline(config *PipelineConfig) (*NarrativePipeline, error) {
	if config == nil {
		config = DefaultPipelineConfig()
	}

	diary, err := NewDiaryKeeper(config.Diary)
	if err != nil {
		return nil, fmt.Errorf("diary keeper: %w", err)
	}

	insight := NewInsightGenerator(config.Insight)
	blog := NewBlogWriter(config.Blog)

	return &NarrativePipeline{
		config:  config,
		diary:   diary,
		insight: insight,
		blog:    blog,
		stopCh:  make(chan struct{}),
	}, nil
}

// --- public API ---

// Diary returns the underlying DiaryKeeper for direct use.
func (p *NarrativePipeline) Diary() *DiaryKeeper { return p.diary }

// InsightGenerator returns the underlying InsightGenerator.
func (p *NarrativePipeline) InsightGenerator() *InsightGenerator { return p.insight }

// BlogWriter returns the underlying BlogWriter.
func (p *NarrativePipeline) BlogWriter() *BlogWriter { return p.blog }

// RecordDiaryEntry is a convenience method that writes a diary entry and
// immediately offers it to the insight generator for on-the-fly analysis.
func (p *NarrativePipeline) RecordDiaryEntry(ctx context.Context, entry *DiaryEntry) (*DiaryEntry, error) {
	stored, err := p.diary.Record(ctx, entry)
	if err != nil || stored == nil {
		return stored, err
	}
	// Lightweight single-entry insight pass (non-blocking)
	go func() {
		_, _ = p.insight.Analyse(ctx, []*DiaryEntry{stored})
	}()
	return stored, nil
}

// Run executes one full Diary → Insight → Blog cycle over the configured
// analysis window. It is safe to call concurrently; overlapping runs are
// skipped.
func (p *NarrativePipeline) Run(ctx context.Context) (*PipelineResult, error) {
	p.mu.Lock()
	if p.running {
		p.mu.Unlock()
		return nil, fmt.Errorf("pipeline already running")
	}
	p.running = true
	p.mu.Unlock()

	defer func() {
		p.mu.Lock()
		p.running = false
		p.mu.Unlock()
	}()

	start := time.Now()
	result := &PipelineResult{RunAt: start}

	// Stage 1: Collect diary entries from the analysis window
	cutoff := start.AddDate(0, 0, -p.config.AnalysisWindowDays)
	entries := p.diary.GetSince(cutoff)
	result.DiaryCount = len(entries)

	// Stage 2: Generate / refine insights
	insights, err := p.insight.Analyse(ctx, entries)
	if err != nil {
		result.Errors = append(result.Errors, fmt.Errorf("insight analysis: %w", err))
	}
	result.InsightsNew = insights

	// Stage 3: Write blog posts
	if len(insights) > 0 {
		posts, err := p.blog.Generate(ctx, insights)
		if err != nil {
			result.Errors = append(result.Errors, fmt.Errorf("blog generation: %w", err))
		}
		result.BlogPostsNew = posts

		// Cross-link diary entries to insights and insights to posts
		for _, ins := range insights {
			for _, entryID := range ins.Evidence {
				p.diary.MarkInsightGenerated(entryID, ins.ID)
			}
		}
		for _, post := range posts {
			for _, insID := range post.InsightIDs {
				p.insight.MarkBlogGenerated(insID, post.ID)
			}
		}
	}

	result.Duration = time.Since(start)

	p.mu.Lock()
	p.lastResult = result
	p.mu.Unlock()

	if p.config.OnRunComplete != nil {
		p.config.OnRunComplete(result)
	}

	return result, nil
}

// Start launches the autonomous pipeline loop (AutoRunInterval must be > 0).
// It is safe to call multiple times; subsequent calls are no-ops.
func (p *NarrativePipeline) Start(ctx context.Context) error {
	if p.config.AutoRunInterval <= 0 {
		return fmt.Errorf("AutoRunInterval must be > 0 to use Start()")
	}

	p.mu.Lock()
	defer p.mu.Unlock()
	if p.running {
		return nil
	}

	p.wg.Add(1)
	go p.loop(ctx)
	return nil
}

// Stop signals the autonomous loop to halt and waits for it to finish.
func (p *NarrativePipeline) Stop() {
	close(p.stopCh)
	p.wg.Wait()
}

// LastResult returns the result of the most recent pipeline run, or nil.
func (p *NarrativePipeline) LastResult() *PipelineResult {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.lastResult
}

// Stats aggregates statistics from all three pipeline stages.
func (p *NarrativePipeline) Stats() map[string]interface{} {
	stats := map[string]interface{}{
		"diary":   p.diary.Stats(),
		"insight": p.insight.Stats(),
		"blog":    p.blog.Stats(),
	}
	if r := p.LastResult(); r != nil {
		stats["last_run"] = map[string]interface{}{
			"at":             r.RunAt,
			"duration_ms":    r.Duration.Milliseconds(),
			"diary_entries":  r.DiaryCount,
			"insights_new":   len(r.InsightsNew),
			"blog_posts_new": len(r.BlogPostsNew),
			"errors":         len(r.Errors),
		}
	}
	return stats
}

// --- autonomous loop ---

func (p *NarrativePipeline) loop(ctx context.Context) {
	defer p.wg.Done()
	ticker := time.NewTicker(p.config.AutoRunInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-p.stopCh:
			return
		case <-ticker.C:
			_, _ = p.Run(ctx)
		}
	}
}
