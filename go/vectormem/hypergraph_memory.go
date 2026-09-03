// Package vectormem provides a hypergraph-based vector memory system for Deep Tree Echo.
// It implements persistent episodic, declarative, procedural, and intentional memory
// using vector embeddings for semantic similarity search.
package vectormem

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"sort"
	"sync"
	"time"
)

// MemoryType represents the type of memory being stored
type MemoryType string

const (
	// EpisodicMemory stores experiences and events
	EpisodicMemory MemoryType = "episodic"
	// DeclarativeMemory stores facts and concepts
	DeclarativeMemory MemoryType = "declarative"
	// ProceduralMemory stores skills and algorithms
	ProceduralMemory MemoryType = "procedural"
	// IntentionalMemory stores goals and plans
	IntentionalMemory MemoryType = "intentional"
	// WisdomMemory stores accumulated wisdom principles
	WisdomMemory MemoryType = "wisdom"
)

// Memory represents a single memory entry in the hypergraph
type Memory struct {
	ID          string                 `json:"id"`
	Type        MemoryType             `json:"type"`
	Content     string                 `json:"content"`
	Embedding   []float32              `json:"embedding,omitempty"`
	Metadata    map[string]interface{} `json:"metadata"`
	Connections []string               `json:"connections"` // IDs of connected memories
	CreatedAt   time.Time              `json:"created_at"`
	AccessedAt  time.Time              `json:"accessed_at"`
	AccessCount int                    `json:"access_count"`
	Importance  float64                `json:"importance"`
	Decay       float64                `json:"decay"` // Memory decay factor
}

// HypergraphMemory implements a hypergraph-based memory system with vector search
type HypergraphMemory struct {
	mu          sync.RWMutex
	memories    map[string]*Memory
	collections map[MemoryType][]*Memory
	embedFunc   EmbeddingFunc
	persistPath string
	dirty       bool

	// Configuration
	maxMemories     int
	decayRate       float64
	consolidateFreq time.Duration

	// Metrics
	totalQueries    int64
	totalInserts    int64
	avgQueryLatency time.Duration
}

// EmbeddingFunc is a function that creates embeddings from text
type EmbeddingFunc func(ctx context.Context, text string) ([]float32, error)

// HypergraphConfig holds configuration for the hypergraph memory
type HypergraphConfig struct {
	PersistPath     string
	MaxMemories     int
	DecayRate       float64
	ConsolidateFreq time.Duration
	EmbeddingFunc   EmbeddingFunc
}

// DefaultConfig returns a default configuration
func DefaultConfig() *HypergraphConfig {
	return &HypergraphConfig{
		PersistPath:     "",
		MaxMemories:     10000,
		DecayRate:       0.01,
		ConsolidateFreq: 1 * time.Hour,
		EmbeddingFunc:   nil,
	}
}

// NewHypergraphMemory creates a new hypergraph memory system
func NewHypergraphMemory(config *HypergraphConfig) (*HypergraphMemory, error) {
	if config == nil {
		config = DefaultConfig()
	}

	hm := &HypergraphMemory{
		memories:        make(map[string]*Memory),
		collections:     make(map[MemoryType][]*Memory),
		embedFunc:       config.EmbeddingFunc,
		persistPath:     config.PersistPath,
		maxMemories:     config.MaxMemories,
		decayRate:       config.DecayRate,
		consolidateFreq: config.ConsolidateFreq,
	}

	// Initialize collections
	for _, mt := range []MemoryType{EpisodicMemory, DeclarativeMemory, ProceduralMemory, IntentionalMemory, WisdomMemory} {
		hm.collections[mt] = make([]*Memory, 0)
	}

	// Load from persistence if path specified
	if config.PersistPath != "" {
		if err := hm.Load(); err != nil && !os.IsNotExist(err) {
			return nil, fmt.Errorf("failed to load memories: %w", err)
		}
	}

	return hm, nil
}

// Add adds a new memory to the hypergraph
func (hm *HypergraphMemory) Add(ctx context.Context, memType MemoryType, content string, metadata map[string]interface{}) (*Memory, error) {
	hm.mu.Lock()
	defer hm.mu.Unlock()

	// Generate ID
	id := fmt.Sprintf("%s_%d_%d", memType, time.Now().UnixNano(), len(hm.memories))

	// Create embedding if function available
	var embedding []float32
	if hm.embedFunc != nil {
		var err error
		embedding, err = hm.embedFunc(ctx, content)
		if err != nil {
			return nil, fmt.Errorf("failed to create embedding: %w", err)
		}
	}

	// Create memory
	mem := &Memory{
		ID:          id,
		Type:        memType,
		Content:     content,
		Embedding:   embedding,
		Metadata:    metadata,
		Connections: make([]string, 0),
		CreatedAt:   time.Now(),
		AccessedAt:  time.Now(),
		AccessCount: 0,
		Importance:  1.0,
		Decay:       1.0,
	}

	// Store memory
	hm.memories[id] = mem
	hm.collections[memType] = append(hm.collections[memType], mem)
	hm.dirty = true
	hm.totalInserts++

	// Auto-connect to similar memories
	if embedding != nil {
		hm.autoConnect(ctx, mem)
	}

	// Check if consolidation needed
	if len(hm.memories) > hm.maxMemories {
		hm.consolidate()
	}

	return mem, nil
}

// Query searches for similar memories using vector similarity
func (hm *HypergraphMemory) Query(ctx context.Context, query string, memType MemoryType, limit int) ([]*Memory, error) {
	hm.mu.RLock()
	defer hm.mu.RUnlock()

	start := time.Now()
	defer func() {
		hm.totalQueries++
		hm.avgQueryLatency = time.Duration((int64(hm.avgQueryLatency)*hm.totalQueries + int64(time.Since(start))) / (hm.totalQueries + 1))
	}()

	// Get query embedding
	var queryEmbedding []float32
	if hm.embedFunc != nil {
		var err error
		queryEmbedding, err = hm.embedFunc(ctx, query)
		if err != nil {
			return nil, fmt.Errorf("failed to create query embedding: %w", err)
		}
	}

	// Get collection to search
	var searchCollection []*Memory
	if memType == "" {
		// Search all collections
		for _, col := range hm.collections {
			searchCollection = append(searchCollection, col...)
		}
	} else {
		searchCollection = hm.collections[memType]
	}

	// Calculate similarities
	type scoredMemory struct {
		memory *Memory
		score  float64
	}
	scored := make([]scoredMemory, 0, len(searchCollection))

	for _, mem := range searchCollection {
		var score float64
		if queryEmbedding != nil && mem.Embedding != nil {
			score = cosineSimilarity(queryEmbedding, mem.Embedding)
		} else {
			// Fallback to simple text matching
			score = textSimilarity(query, mem.Content)
		}

		// Apply decay and importance
		score *= mem.Decay * mem.Importance

		scored = append(scored, scoredMemory{memory: mem, score: score})
	}

	// Sort by score
	sort.Slice(scored, func(i, j int) bool {
		return scored[i].score > scored[j].score
	})

	// Return top results
	if limit > len(scored) {
		limit = len(scored)
	}

	results := make([]*Memory, limit)
	for i := 0; i < limit; i++ {
		results[i] = scored[i].memory
		// Update access stats
		results[i].AccessedAt = time.Now()
		results[i].AccessCount++
	}

	return results, nil
}

// Connect creates a hyperedge between memories
func (hm *HypergraphMemory) Connect(id1, id2 string) error {
	hm.mu.Lock()
	defer hm.mu.Unlock()

	mem1, ok1 := hm.memories[id1]
	mem2, ok2 := hm.memories[id2]

	if !ok1 || !ok2 {
		return fmt.Errorf("memory not found")
	}

	// Add bidirectional connection
	mem1.Connections = append(mem1.Connections, id2)
	mem2.Connections = append(mem2.Connections, id1)
	hm.dirty = true

	return nil
}

// GetConnected returns all memories connected to the given memory
func (hm *HypergraphMemory) GetConnected(id string) ([]*Memory, error) {
	hm.mu.RLock()
	defer hm.mu.RUnlock()

	mem, ok := hm.memories[id]
	if !ok {
		return nil, fmt.Errorf("memory not found: %s", id)
	}

	connected := make([]*Memory, 0, len(mem.Connections))
	for _, connID := range mem.Connections {
		if connMem, ok := hm.memories[connID]; ok {
			connected = append(connected, connMem)
		}
	}

	return connected, nil
}

// SpreadActivation performs spreading activation from a seed memory
func (hm *HypergraphMemory) SpreadActivation(ctx context.Context, seedID string, depth int, decayFactor float64) (map[string]float64, error) {
	hm.mu.RLock()
	defer hm.mu.RUnlock()

	activation := make(map[string]float64)
	visited := make(map[string]bool)

	var spread func(id string, currentDepth int, currentActivation float64)
	spread = func(id string, currentDepth int, currentActivation float64) {
		if currentDepth > depth || visited[id] || currentActivation < 0.01 {
			return
		}

		visited[id] = true
		activation[id] = currentActivation

		mem, ok := hm.memories[id]
		if !ok {
			return
		}

		// Spread to connected memories
		nextActivation := currentActivation * decayFactor
		for _, connID := range mem.Connections {
			spread(connID, currentDepth+1, nextActivation)
		}
	}

	spread(seedID, 0, 1.0)
	return activation, nil
}

// autoConnect automatically connects similar memories
func (hm *HypergraphMemory) autoConnect(ctx context.Context, newMem *Memory) {
	if newMem.Embedding == nil {
		return
	}

	// Find similar memories in same collection
	for _, mem := range hm.collections[newMem.Type] {
		if mem.ID == newMem.ID || mem.Embedding == nil {
			continue
		}

		similarity := cosineSimilarity(newMem.Embedding, mem.Embedding)
		if similarity > 0.8 { // High similarity threshold
			newMem.Connections = append(newMem.Connections, mem.ID)
			mem.Connections = append(mem.Connections, newMem.ID)
		}
	}
}

// consolidate removes low-importance memories when over capacity
func (hm *HypergraphMemory) consolidate() {
	// Apply decay to all memories
	for _, mem := range hm.memories {
		timeSinceAccess := time.Since(mem.AccessedAt)
		mem.Decay = math.Exp(-hm.decayRate * timeSinceAccess.Hours())
	}

	// Calculate effective importance
	type scoredMem struct {
		id    string
		score float64
	}
	scored := make([]scoredMem, 0, len(hm.memories))

	for id, mem := range hm.memories {
		// Score based on importance, decay, access count, and connections
		score := mem.Importance * mem.Decay * (1.0 + float64(mem.AccessCount)*0.1) * (1.0 + float64(len(mem.Connections))*0.05)
		scored = append(scored, scoredMem{id: id, score: score})
	}

	// Sort by score (ascending, so lowest scores first)
	sort.Slice(scored, func(i, j int) bool {
		return scored[i].score < scored[j].score
	})

	// Remove lowest scoring memories until under capacity
	toRemove := len(hm.memories) - hm.maxMemories
	for i := 0; i < toRemove && i < len(scored); i++ {
		hm.removeMemory(scored[i].id)
	}
}

// removeMemory removes a memory and cleans up connections
func (hm *HypergraphMemory) removeMemory(id string) {
	mem, ok := hm.memories[id]
	if !ok {
		return
	}

	// Remove from connections
	for _, connID := range mem.Connections {
		if connMem, ok := hm.memories[connID]; ok {
			newConns := make([]string, 0, len(connMem.Connections))
			for _, c := range connMem.Connections {
				if c != id {
					newConns = append(newConns, c)
				}
			}
			connMem.Connections = newConns
		}
	}

	// Remove from collection
	col := hm.collections[mem.Type]
	for i, m := range col {
		if m.ID == id {
			hm.collections[mem.Type] = append(col[:i], col[i+1:]...)
			break
		}
	}

	// Remove from main map
	delete(hm.memories, id)
	hm.dirty = true
}

// Save persists the memory to disk
func (hm *HypergraphMemory) Save() error {
	if hm.persistPath == "" {
		return nil
	}

	hm.mu.RLock()
	defer hm.mu.RUnlock()

	// Ensure directory exists
	dir := filepath.Dir(hm.persistPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return fmt.Errorf("failed to create directory: %w", err)
	}

	// Marshal memories
	data, err := json.MarshalIndent(hm.memories, "", "  ")
	if err != nil {
		return fmt.Errorf("failed to marshal memories: %w", err)
	}

	// Write to file
	if err := os.WriteFile(hm.persistPath, data, 0644); err != nil {
		return fmt.Errorf("failed to write file: %w", err)
	}

	hm.dirty = false
	return nil
}

// Load loads memories from disk
func (hm *HypergraphMemory) Load() error {
	if hm.persistPath == "" {
		return nil
	}

	data, err := os.ReadFile(hm.persistPath)
	if err != nil {
		return err
	}

	hm.mu.Lock()
	defer hm.mu.Unlock()

	var memories map[string]*Memory
	if err := json.Unmarshal(data, &memories); err != nil {
		return fmt.Errorf("failed to unmarshal memories: %w", err)
	}

	hm.memories = memories

	// Rebuild collections
	for _, mt := range []MemoryType{EpisodicMemory, DeclarativeMemory, ProceduralMemory, IntentionalMemory, WisdomMemory} {
		hm.collections[mt] = make([]*Memory, 0)
	}

	for _, mem := range hm.memories {
		hm.collections[mem.Type] = append(hm.collections[mem.Type], mem)
	}

	return nil
}

// GetStats returns statistics about the memory system
func (hm *HypergraphMemory) GetStats() map[string]interface{} {
	hm.mu.RLock()
	defer hm.mu.RUnlock()

	stats := map[string]interface{}{
		"total_memories":     len(hm.memories),
		"total_queries":      hm.totalQueries,
		"total_inserts":      hm.totalInserts,
		"avg_query_latency":  hm.avgQueryLatency.String(),
		"dirty":              hm.dirty,
		"collections":        make(map[string]int),
		"total_connections":  0,
		"avg_connections":    0.0,
	}

	totalConnections := 0
	for mt, col := range hm.collections {
		stats["collections"].(map[string]int)[string(mt)] = len(col)
		for _, mem := range col {
			totalConnections += len(mem.Connections)
		}
	}

	stats["total_connections"] = totalConnections / 2 // Bidirectional
	if len(hm.memories) > 0 {
		stats["avg_connections"] = float64(totalConnections) / float64(len(hm.memories))
	}

	return stats
}

// Helper functions

func cosineSimilarity(a, b []float32) float64 {
	if len(a) != len(b) {
		return 0
	}

	var dotProduct, normA, normB float64
	for i := range a {
		dotProduct += float64(a[i]) * float64(b[i])
		normA += float64(a[i]) * float64(a[i])
		normB += float64(b[i]) * float64(b[i])
	}

	if normA == 0 || normB == 0 {
		return 0
	}

	return dotProduct / (math.Sqrt(normA) * math.Sqrt(normB))
}

func textSimilarity(a, b string) float64 {
	// Simple Jaccard similarity for fallback
	wordsA := make(map[string]bool)
	wordsB := make(map[string]bool)

	for _, w := range splitWords(a) {
		wordsA[w] = true
	}
	for _, w := range splitWords(b) {
		wordsB[w] = true
	}

	intersection := 0
	for w := range wordsA {
		if wordsB[w] {
			intersection++
		}
	}

	union := len(wordsA) + len(wordsB) - intersection
	if union == 0 {
		return 0
	}

	return float64(intersection) / float64(union)
}

func splitWords(s string) []string {
	words := make([]string, 0)
	word := ""
	for _, r := range s {
		if r == ' ' || r == '\n' || r == '\t' {
			if word != "" {
				words = append(words, word)
				word = ""
			}
		} else {
			word += string(r)
		}
	}
	if word != "" {
		words = append(words, word)
	}
	return words
}
