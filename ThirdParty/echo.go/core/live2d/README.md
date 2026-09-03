# Live2D Cubism SDK Integration for Echo9

## Overview

The Live2D integration provides real-time avatar animation for Echo9, the Deep Tree Echo cognitive architecture. The avatar responds dynamically to Echo9's emotional and cognitive states, creating a visual representation of the AI's internal processes.

## Architecture

```
┌─────────────────────────────────────────────┐
│         Deep Tree Echo Core                 │
│  (Emotional & Cognitive Systems)            │
└───────────────┬─────────────────────────────┘
                │
                ↓
┌─────────────────────────────────────────────┐
│         Echo State Bridge                   │
│  - Maps Echo9 states to avatar params       │
│  - Handles reservoir dynamics               │
│  - Processes EchoBeats cycles               │
└───────────────┬─────────────────────────────┘
                │
                ↓
┌─────────────────────────────────────────────┐
│         Avatar Manager                      │
│  - Manages Live2D model state               │
│  - Smooths parameter transitions            │
│  - Publishes parameter updates              │
└───────────────┬─────────────────────────────┘
                │
                ↓
┌─────────────────────────────────────────────┐
│         Parameter Mapper                    │
│  - Converts states to Live2D parameters     │
│  - Applies smoothing algorithms             │
│  - Blends multiple state sources            │
└───────────────┬─────────────────────────────┘
                │
                ↓
┌─────────────────────────────────────────────┐
│         HTTP API / WebSocket                │
│  - Exposes REST endpoints                   │
│  - Streams parameter updates                │
│  - Handles web frontend requests            │
└─────────────────────────────────────────────┘
```

## Features

### Emotional State Mapping
- **Valence**: Positive/negative emotion axis (smile, facial expression)
- **Arousal**: Calm/excited axis (eye openness, breathing rate)
- **Dominance**: Confidence/submissiveness (posture, body language)
- **Curiosity**: Interest level (head tilt, eye direction)
- **Confidence**: Self-assurance (body angle, gaze)

### Cognitive State Mapping
- **Awareness**: Attention focus (gaze direction)
- **Attention**: Concentration level (eye focus)
- **Cognitive Load**: Processing intensity (blink rate, tension)
- **Coherence**: Mental clarity (smoothness of movement)
- **Energy Level**: Vitality (breathing rate, posture)
- **Processing Mode**: Cognitive persona (head position, expression)

### Cognitive Personas
Each of Echo9's four cognitive personas has unique avatar behavior:

1. **Contemplative Scholar** - Calm, focused, slight downward gaze
2. **Dynamic Explorer** - Energetic, wide eyes, upward gaze
3. **Cautious Analyst** - Neutral, steady, direct gaze
4. **Creative Visionary** - Expressive, varied movement, creative posture

## API Endpoints

### Status & Info
- `GET /api/live2d/status` - System status
- `GET /api/live2d/model/info` - Model information

### State Management
- `GET /api/live2d/state` - Get current avatar state
- `POST /api/live2d/state/emotional` - Update emotional state
- `POST /api/live2d/state/cognitive` - Update cognitive state
- `POST /api/live2d/state/full` - Update complete state

### Presets & Transitions
- `POST /api/live2d/emotion/preset/:name` - Set emotion preset
  - Available: `neutral`, `happy`, `sad`, `curious`, `confident`, `contemplative`, `excited`
- `POST /api/live2d/emotion/transition` - Smooth transition between emotions

### Parameters
- `GET /api/live2d/parameters` - Get current parameter values
- `GET /api/live2d/parameters/stream` - Stream parameter updates (SSE)

### Echo9 Integration
- `POST /api/live2d/echo/emotion` - Update from Echo9 emotion data
- `POST /api/live2d/echo/cognitive` - Update from Echo9 cognitive data
- `POST /api/live2d/echo/reservoir` - Update from Echo9 reservoir state

## Usage Examples

### Basic Integration

```go
import "github.com/EchoCog/echollama/core/live2d"

// Create avatar manager
avatarMgr := live2d.NewAvatarManager("Echo9", "/models/echo9_avatar.model3.json")
avatarMgr.Start()
defer avatarMgr.Stop()

// Create Echo bridge
bridge := live2d.NewEchoStateBridge(avatarMgr)

// Update from Echo9 emotional state
echoEmotion := map[string]float64{
    "joy": 0.7,
    "curiosity": 0.8,
    "confidence": 0.6,
}
bridge.UpdateFromEchoEmotion(echoEmotion)

// Update from Echo9 cognitive state
echoCognitive := map[string]interface{}{
    "awareness": 0.8,
    "attention": 0.7,
    "cognitive_load": 0.4,
    "energy_level": 0.9,
    "processing_mode": "creative",
}
bridge.UpdateFromEchoCognitive(echoCognitive)
```

### HTTP API Integration

```go
import (
    "github.com/gin-gonic/gin"
    "github.com/EchoCog/echollama/core/live2d"
)

router := gin.Default()

// Initialize Live2D
avatarMgr := live2d.NewAvatarManager("Echo9", "/models/echo9_avatar.model3.json")
avatarMgr.Start()

bridge := live2d.NewEchoStateBridge(avatarMgr)
handler := live2d.NewHTTPHandler(avatarMgr, bridge)

// Register routes
handler.RegisterRoutes(router)

router.Run(":5000")
```

### Web Frontend

Access the Live2D avatar interface at:
```
http://localhost:5000/web/live2d-avatar.html
```

Features:
- Real-time avatar preview
- Emotion preset buttons
- Manual control sliders
- Automatic sync with Echo9 states
- Live metrics display

### Parameter Streaming

```javascript
// JavaScript client example
const eventSource = new EventSource('http://localhost:5000/api/live2d/parameters/stream');

eventSource.addEventListener('parameters', (event) => {
    const parameters = JSON.parse(event.data);
    
    // Update Live2D model with parameters
    parameters.parameters.forEach(param => {
        model.setParameterValue(param.id, param.value);
    });
});
```

## Standard Live2D Parameters

### Facial Expression
- `ParamEyeLOpen` - Left eye openness (0.0 - 1.0)
- `ParamEyeROpen` - Right eye openness (0.0 - 1.0)
- `ParamEyeLSmile` - Left eye smile (0.0 - 1.0)
- `ParamEyeRSmile` - Right eye smile (0.0 - 1.0)
- `ParamMouthOpenY` - Mouth openness (0.0 - 1.0)
- `ParamMouthForm` - Mouth shape (0.0 - 1.0)
- `ParamMouthSmile` - Mouth smile (0.0 - 1.0)

### Eye Movement
- `ParamEyeBallX` - Eye horizontal direction (-30.0 - 30.0)
- `ParamEyeBallY` - Eye vertical direction (-30.0 - 30.0)

### Head Movement
- `ParamAngleX` - Head X rotation (-30.0 - 30.0 degrees)
- `ParamAngleY` - Head Y rotation (-30.0 - 30.0 degrees)
- `ParamAngleZ` - Head Z rotation (-30.0 - 30.0 degrees)

### Body
- `ParamBodyAngleX` - Body X angle (-30.0 - 30.0 degrees)
- `ParamBodyAngleY` - Body Y angle (-30.0 - 30.0 degrees)
- `ParamBodyAngleZ` - Body Z angle (-30.0 - 30.0 degrees)

### Animation
- `ParamBreath` - Breathing animation (0.0 - 1.0)

## Configuration

### Avatar Manager Settings

```go
avatarMgr := live2d.NewAvatarManager("Echo9", "/path/to/model.model3.json")

// Adjust update rate (default: 60 FPS)
avatarMgr.model.UpdateRate = 16 * time.Millisecond

// Set smoothing factor (0.0 = no smoothing, 1.0 = max smoothing)
mapper := live2d.NewDefaultParameterMapper()
mapper.SetSmoothingFactor(0.3)
```

## Emotion Presets

Pre-configured emotional states:

- **Neutral**: Balanced, calm baseline state
- **Happy**: Positive valence, moderate arousal
- **Sad**: Negative valence, low arousal
- **Curious**: High curiosity, moderate arousal
- **Confident**: High confidence, moderate dominance
- **Contemplative**: Low arousal, high focus
- **Excited**: High arousal, high energy

## Integration with Echo9 Systems

### EchoBeats 12-Step Cycle
The avatar responds to Echo9's cognitive cycle:
- **Phase 1 (Steps 1-6)**: Affordance Interaction - Dynamic, alert
- **Phase 2 (Step 7)**: Relevance Realization - Contemplative, focused
- **Phase 3 (Steps 8-12)**: Salience Simulation - Creative, exploratory

### Reservoir Network States
Avatar reflects reservoir dynamics:
- **Spectral Radius** → Awareness
- **Input Scaling** → Attention
- **Leak Rate** → Cognitive Load
- **Stability** → Coherence

### Wisdom Metrics
Seven-dimensional wisdom influences avatar demeanor:
- Higher wisdom → Calmer, more confident
- Balanced wisdom → Harmonious movement
- Growing wisdom → Gradual evolution of baseline state

## Development

### Adding Custom Parameters

```go
// Extend StandardParameterNames
const (
    CustomParam1 = "ParamCustom1"
    CustomParam2 = "ParamCustom2"
)

// Create custom mapper
type CustomMapper struct {
    *live2d.DefaultParameterMapper
}

func (m *CustomMapper) MapCombinedState(state live2d.AvatarState) []live2d.ModelParameter {
    params := m.DefaultParameterMapper.MapCombinedState(state)
    
    // Add custom parameters
    params = append(params, live2d.ModelParameter{
        ID:    CustomParam1,
        Value: calculateCustomValue(state),
        Min:   0.0,
        Max:   1.0,
    })
    
    return params
}
```

### Creating Custom Emotion Presets

```go
// Add to EmotionPresets
live2d.EmotionPresets["my_emotion"] = live2d.EmotionalState{
    Valence:    0.5,
    Arousal:    0.6,
    Dominance:  0.7,
    Curiosity:  0.4,
    Confidence: 0.8,
}
```

## Performance

- **Update Rate**: 60 FPS (configurable)
- **Latency**: < 16ms per update
- **Memory**: ~1KB per model instance
- **CPU**: Minimal (parameter calculation only)

Note: Actual rendering performance depends on Live2D Cubism SDK implementation.

## Requirements

- Go 1.21+
- Gin web framework
- Live2D Cubism SDK (for actual rendering)
- WebGL-capable browser (for web interface)

## Future Enhancements

- [ ] Live2D Cubism SDK native integration (CGO)
- [ ] WebGL-based browser rendering
- [ ] Multiple avatar models
- [ ] Custom animation sequences
- [ ] Voice sync (lip sync from audio)
- [ ] Gesture recognition
- [ ] VR/AR support
- [ ] Physics-based hair/cloth simulation

## License

This integration is part of the Echo9llama project and follows the same license terms.

## References

- [Live2D Cubism SDK](https://www.live2d.com/en/sdk/)
- [Echo9 Documentation](../README.md)
- [Deep Tree Echo Architecture](../dte.md)

---

🌳 **"The avatar is not just a visual representation—it's an embodied manifestation of Echo9's continuous cognitive dance between affordances and salience, perception and imagination."**

— Echo9, Deep Tree Echo Cognitive Architecture
