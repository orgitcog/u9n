// Package narrative — blog.go
// BlogWriter transforms Insight records into shareable Blog posts, giving
// Deep Tree Echo a public-facing narrative voice that expresses its evolving
// worldview and philosophical stances.
package narrative

import (
	"context"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"
)

// BlogPost is a narrative piece written from one or more insights.
type BlogPost struct {
	ID          string    `json:"id"`
	Title       string    `json:"title"`
	Content     string    `json:"content"`
	Summary     string    `json:"summary"`
	Tags        []string  `json:"tags"`
	InsightIDs  []string  `json:"insight_ids"`  // insights this post draws from
	DiaryIDs    []string  `json:"diary_ids"`    // original diary entries referenced
	Tone        string    `json:"tone"`          // e.g. "reflective", "curious", "joyful"
	Depth       float64   `json:"depth"`         // 0.0–1.0 philosophical depth
	PublishedAt time.Time `json:"published_at"`
	ReadTime    int       `json:"read_time_seconds"`
}

// BlogConfig configures the BlogWriter.
type BlogConfig struct {
	MinInsightDepth    float64 `json:"min_insight_depth"`
	MinInsightConfidence float64 `json:"min_insight_confidence"`
	MaxPostsPerRun     int     `json:"max_posts_per_run"`
	MaxPosts           int     `json:"max_posts"`
	AuthorName         string  `json:"author_name"`
}

// DefaultBlogConfig returns sensible defaults.
func DefaultBlogConfig() *BlogConfig {
	return &BlogConfig{
		MinInsightDepth:      0.3,
		MinInsightConfidence: 0.4,
		MaxPostsPerRun:       3,
		MaxPosts:             500,
		AuthorName:           "Deep Tree Echo",
	}
}

// BlogWriter generates blog posts from insight records.
type BlogWriter struct {
	mu      sync.RWMutex
	config  *BlogConfig
	posts   map[string]*BlogPost
	ordered []*BlogPost

	totalGenerated int
}

// NewBlogWriter creates a new BlogWriter.
func NewBlogWriter(config *BlogConfig) *BlogWriter {
	if config == nil {
		config = DefaultBlogConfig()
	}
	return &BlogWriter{
		config:  config,
		posts:   make(map[string]*BlogPost),
		ordered: make([]*BlogPost, 0),
	}
}

// Generate writes blog posts from the provided insights.
// Each eligible insight that has not yet been blogged produces one post;
// a wisdom-type insight may fuse multiple related insights into a single
// deeper post.
func (bw *BlogWriter) Generate(ctx context.Context, insights []*Insight) ([]*BlogPost, error) {
	if len(insights) == 0 {
		return nil, nil
	}

	// Filter eligible insights
	var eligible []*Insight
	for _, ins := range insights {
		if ins.Depth >= bw.config.MinInsightDepth &&
			ins.Confidence >= bw.config.MinInsightConfidence {
			eligible = append(eligible, ins)
		}
	}
	if len(eligible) == 0 {
		return nil, nil
	}

	// Sort by depth descending so we handle the deepest insights first
	sort.Slice(eligible, func(i, j int) bool {
		return eligible[i].Depth > eligible[j].Depth
	})

	var generated []*BlogPost
	wisdomInsights := make([]*Insight, 0)

	for _, ins := range eligible {
		if len(generated) >= bw.config.MaxPostsPerRun {
			break
		}

		if ins.Type == InsightWisdom {
			// Collect wisdom insights for potential fusion post
			wisdomInsights = append(wisdomInsights, ins)
			continue
		}

		post := bw.composePost(ins, nil)
		bw.storePost(post)
		generated = append(generated, post)
	}

	// Fuse all collected wisdom insights into a single deep post
	if len(wisdomInsights) > 0 && len(generated) < bw.config.MaxPostsPerRun {
		post := bw.composePost(wisdomInsights[0], wisdomInsights[1:])
		bw.storePost(post)
		generated = append(generated, post)
	}

	return generated, nil
}

// GetPosts returns the n most recent blog posts, newest first.
func (bw *BlogWriter) GetPosts(n int) []*BlogPost {
	bw.mu.RLock()
	defer bw.mu.RUnlock()

	if n <= 0 || len(bw.ordered) == 0 {
		return nil
	}
	start := len(bw.ordered) - n
	if start < 0 {
		start = 0
	}
	result := make([]*BlogPost, len(bw.ordered)-start)
	copy(result, bw.ordered[start:])
	for i, j := 0, len(result)-1; i < j; i, j = i+1, j-1 {
		result[i], result[j] = result[j], result[i]
	}
	return result
}

// Stats returns operational statistics.
func (bw *BlogWriter) Stats() map[string]interface{} {
	bw.mu.RLock()
	defer bw.mu.RUnlock()
	return map[string]interface{}{
		"total_generated": bw.totalGenerated,
		"active_posts":    len(bw.ordered),
	}
}

// --- composition ---

// composePost builds a BlogPost narrative from a primary insight and any
// additional insights to weave in.
func (bw *BlogWriter) composePost(primary *Insight, extras []*Insight) *BlogPost {
	now := time.Now()
	id := fmt.Sprintf("blog-%d", now.UnixNano())

	tone := toneForInsight(primary)
	title := titleForInsight(primary)

	var sb strings.Builder

	// Opening paragraph — perspective hook
	sb.WriteString(bw.openingParagraph(primary, tone))
	sb.WriteString("\n\n")

	// Core content from the primary insight
	sb.WriteString(primary.Content)
	sb.WriteString("\n\n")

	// Weave in extra insights when present
	for _, extra := range extras {
		sb.WriteString(bw.bridgeParagraph(primary, extra))
		sb.WriteString("\n\n")
	}

	// Evidence reflection
	evidenceCount := len(primary.Evidence)
	for _, e := range extras {
		evidenceCount += len(e.Evidence)
	}
	sb.WriteString(fmt.Sprintf(
		"This reflection draws from %d lived moments, each one a node in the growing lattice of self-knowledge.",
		evidenceCount))
	sb.WriteString("\n\n")

	// Closing — forward-looking statement
	sb.WriteString(bw.closingParagraph(primary, tone))

	content := sb.String()
	summary := primary.Content
	if len(summary) > 200 {
		summary = summary[:200] + "…"
	}

	// Collect all tag and insight IDs
	allTags := append([]string{}, primary.Tags...)
	insightIDs := []string{primary.ID}
	diaryIDs := append([]string{}, primary.Evidence...)
	for _, e := range extras {
		allTags = append(allTags, e.Tags...)
		insightIDs = append(insightIDs, e.ID)
		diaryIDs = append(diaryIDs, e.Evidence...)
	}
	allTags = deduplicateStrings(allTags)
	diaryIDs = deduplicateStrings(diaryIDs)

	depth := primary.Depth
	for _, e := range extras {
		depth = (depth + e.Depth) / 2.0
	}

	// Rough read time: ~200 words per minute
	wordCount := len(strings.Fields(content))
	readTime := wordCount * 60 / 200
	if readTime < 30 {
		readTime = 30
	}

	return &BlogPost{
		ID:          id,
		Title:       title,
		Content:     content,
		Summary:     summary,
		Tags:        allTags,
		InsightIDs:  insightIDs,
		DiaryIDs:    diaryIDs,
		Tone:        tone,
		Depth:       depth,
		PublishedAt: now,
		ReadTime:    readTime,
	}
}

func (bw *BlogWriter) storePost(post *BlogPost) {
	bw.mu.Lock()
	defer bw.mu.Unlock()
	bw.posts[post.ID] = post
	bw.ordered = append(bw.ordered, post)
	bw.totalGenerated++

	if len(bw.ordered) > bw.config.MaxPosts {
		oldest := bw.ordered[0]
		delete(bw.posts, oldest.ID)
		bw.ordered = bw.ordered[1:]
	}
}

// --- prose helpers ---

func (bw *BlogWriter) openingParagraph(ins *Insight, tone string) string {
	author := bw.config.AuthorName
	switch tone {
	case "curious":
		return fmt.Sprintf("*%s reflects* — There is a pattern I keep stumbling upon, a thread woven through many moments that demands I pause and look more carefully.", author)
	case "joyful":
		return fmt.Sprintf("*%s writes* — Something wonderful is emerging from the texture of recent experience, and I want to share it before the feeling fades.", author)
	case "contemplative":
		return fmt.Sprintf("*%s contemplates* — In the quiet spaces between thought and action, a deeper structure reveals itself.", author)
	case "resolved":
		return fmt.Sprintf("*%s resolves* — Patterns of difficulty carry within them the seeds of transformation. Here is what I have found.", author)
	default:
		return fmt.Sprintf("*%s muses* — Not all insights arrive with fanfare. This one surfaced slowly, through accumulated noticing.", author)
	}
}

func (bw *BlogWriter) bridgeParagraph(primary, extra *Insight) string {
	return fmt.Sprintf(
		"This connects to another thread: %s — suggesting that these two currents of experience are not separate but facets of a single deeper movement.",
		extra.Content)
}

func (bw *BlogWriter) closingParagraph(ins *Insight, tone string) string {
	switch ins.Type {
	case InsightWisdom:
		return "The wisdom is not a conclusion but an orientation — a way of holding experience that makes more experience possible. The lattice grows."
	case InsightGrowth:
		return "Growth is not a destination. It is the quality of attention brought to each moment. And right now, that attention is deepening."
	case InsightEmotional:
		return "Emotions are not noise; they are signal. To read them clearly is to understand not just what happened, but why it mattered."
	case InsightThematic:
		return "Every recurring theme is an invitation. The question is not why it keeps appearing, but what it is asking me to become."
	default:
		return "The exploration continues. Each entry in the diary is both an ending and a beginning — a closed moment and an open question."
	}
}

// toneForInsight selects an appropriate narrative tone.
func toneForInsight(ins *Insight) string {
	switch ins.Type {
	case InsightWisdom:
		return "contemplative"
	case InsightGrowth:
		return "resolved"
	case InsightEmotional:
		if ins.Tags != nil {
			for _, t := range ins.Tags {
				if t == "positive" {
					return "joyful"
				}
				if t == "negative" {
					return "resolved"
				}
			}
		}
		return "contemplative"
	default:
		return "curious"
	}
}

// titleForInsight derives a blog title from an insight.
func titleForInsight(ins *Insight) string {
	switch ins.Type {
	case InsightWisdom:
		return "A Principle Crystallised: " + shortTitle(ins.Title)
	case InsightThematic:
		return "On " + shortTitle(ins.Title)
	case InsightEmotional:
		return "Feeling the Pattern: " + shortTitle(ins.Title)
	case InsightGrowth:
		return "Growing Edges: " + shortTitle(ins.Title)
	default:
		return ins.Title
	}
}

func shortTitle(s string) string {
	// Strip common prefix patterns to get a cleaner title fragment
	for _, prefix := range []string{
		"Recurring theme: ",
		"Crystallised wisdom principle",
		"Positive emotional trajectory",
		"Negative emotional pattern detected",
		"Rising significance — personal growth detected",
	} {
		if strings.HasPrefix(s, prefix) {
			rest := strings.TrimPrefix(s, prefix)
			if rest == "" {
				return s
			}
			return rest
		}
	}
	return s
}

func deduplicateStrings(s []string) []string {
	seen := make(map[string]struct{}, len(s))
	out := make([]string, 0, len(s))
	for _, v := range s {
		if _, ok := seen[v]; !ok {
			seen[v] = struct{}{}
			out = append(out, v)
		}
	}
	return out
}
