# Quick Start: ABP_DeepTreeEcho_Avatar Animation Blueprint

**Estimated Time:** 2-3 hours  
**Difficulty:** Intermediate  
**Priority:** CRITICAL - Must be created first

---

## What You're Building

The ABP_DeepTreeEcho_Avatar is the animation blueprint that brings the Deep Tree Echo avatar to life. It connects the C++ `UAvatarAnimInstance` class to the skeletal mesh, enabling dynamic animations, facial expressions, and personality-driven behaviors.

---

## Prerequisites Checklist

Before you begin, make sure you have:

- [ ] Unreal Engine 5.3+ installed
- [ ] UnrealEngineCog project opened
- [ ] C++ code compiled successfully
- [ ] Skeletal mesh with proper rigging (or use placeholder)
- [ ] At least one idle animation (or use UE5 mannequin animations as placeholder)

---

## Step-by-Step Implementation

### Part 1: Create the Animation Blueprint (5 minutes)

1. **Open Unreal Engine** and load the UnrealEngineCog project

2. **Navigate to Content Browser:**
   - Create folder structure: `/Game/DeepTreeEcho/Animations/`

3. **Create Animation Blueprint:**
   - Right-click in `/Game/DeepTreeEcho/Animations/`
   - Select `Animation → Animation Blueprint`
   
4. **Configure Settings:**
   - **Parent Class:** Search for and select `AvatarAnimInstance`
   - **Target Skeleton:** Select your avatar skeleton (or UE5 Mannequin for testing)
   - **Name:** `ABP_DeepTreeEcho_Avatar`
   - Click `OK`

5. **Open the Blueprint:**
   - Double-click `ABP_DeepTreeEcho_Avatar` to open

---

### Part 2: Build the Animation Graph (30 minutes)

#### 2.1 Create Basic Locomotion State Machine

1. **In AnimGraph tab:**
   - Right-click in graph → `Add State Machine`
   - Name it: `Locomotion`
   - Connect `Locomotion` output to `Output Pose` input

2. **Double-click the Locomotion state machine** to enter it

3. **Create Idle State:**
   - Right-click → `Add State` → Name: `Idle`
   - Right-click `Idle` → `Set as Entry State` (makes it the starting state)
   - **Double-click Idle state** to edit it
   - Inside: Add `Sequence Player` node (or drag in an idle animation)
   - Connect to `Output Animation Pose`
   - Go back to state machine view (breadcrumb navigation)

4. **Create Walk State:**
   - Right-click → `Add State` → Name: `Walk`
   - **Double-click Walk state** to edit it
   - Inside: Add `Sequence Player` node (use walk animation)
   - Connect to `Output Animation Pose`
   - Go back to state machine view

5. **Create Run State:**
   - Right-click → `Add State` → Name: `Run`
   - **Double-click Run state** to edit it
   - Inside: Add `Sequence Player` node (use run animation)
   - Connect to `Output Animation Pose`
   - Go back to state machine view

#### 2.2 Add State Transitions

1. **Idle → Walk Transition:**
   - Drag from `Idle` to `Walk` to create transition
   - **Double-click the transition arrow** to edit it
   - In the transition graph, add condition:
     - Get `Speed` variable (from parent class)
     - Add `Float > Float` node
     - Set threshold to `10.0`
     - Connect to `Result` (Can Enter Transition)

2. **Walk → Idle Transition:**
   - Drag from `Walk` to `Idle`
   - Double-click transition arrow
   - Add condition: `Speed < 10.0`

3. **Walk → Run Transition:**
   - Drag from `Walk` to `Run`
   - Double-click transition arrow
   - Add condition: `Speed > 300.0`

4. **Run → Walk Transition:**
   - Drag from `Run` to `Walk`
   - Double-click transition arrow
   - Add condition: `Speed < 300.0`

---

### Part 3: Add Facial Expression System (45 minutes)

1. **Go back to AnimGraph** (use breadcrumb navigation)

2. **After the Locomotion state machine, add Layered Blend:**
   - Right-click → Search for `Layered blend per bone`
   - Connect `Locomotion` output to `Base Pose` input
   - Connect `Layered blend` output to `Output Pose`

3. **Add Facial Expression Slot:**
   - Right-click → `Add Slot` node
   - Name: `FacialExpressions`
   - Connect between `Layered blend` and `Output Pose`

4. **Add Morph Target (Blend Shape) Control:**
   - Right-click → Search for `Set Morph Target`
   - Add multiple `Set Morph Target` nodes for:
     - `MouthSmile`
     - `MouthFrown`
     - `EyeWide`
     - `BrowUp`
   - Chain them together (each output connects to next input)
   - Connect final output to `Output Pose`

5. **Connect Morph Target Values:**
   - For each `Set Morph Target` node:
     - Get corresponding variable from parent class (e.g., `SmileAmount`)
     - Connect to `Value` input

---

### Part 4: Add Gesture System (30 minutes)

1. **Create Montage Slot:**
   - In AnimGraph, add `Slot` node
   - Name: `Gestures`
   - Insert between facial expressions and output

2. **Configure Slot Settings:**
   - Select the slot node
   - In Details panel, set:
     - **Blend In Time:** 0.2
     - **Blend Out Time:** 0.2

---

### Part 5: Event Graph Setup (30 minutes)

1. **Switch to Event Graph tab**

2. **Find Event Blueprint Update Animation:**
   - This event is called every frame
   - Should already exist (from parent class)

3. **Add Speed Calculation:**
   - From `Event Blueprint Update Animation`
   - Get `Pawn Owner` → `Get Velocity` → `Vector Length`
   - Set `Speed` variable (create if doesn't exist)

4. **Add Direction Calculation:**
   - Get `Pawn Owner` → `Get Velocity`
   - Get `Pawn Owner` → `Get Actor Forward Vector`
   - Add `Dot Product` node
   - Set `Direction` variable

5. **Test Variables:**
   - Compile and save
   - In viewport, check that Speed updates when moving

---

### Part 6: Testing (15 minutes)

1. **Compile the Blueprint:**
   - Click `Compile` button (top toolbar)
   - Fix any errors (usually missing animations or variables)

2. **Assign to Character:**
   - Open your avatar character blueprint
   - Select the Skeletal Mesh Component
   - In Details panel, find `Anim Class`
   - Set to `ABP_DeepTreeEcho_Avatar`

3. **Test in Viewport:**
   - Click `Play` in editor
   - Move the character
   - Verify state transitions work (Idle → Walk → Run)

4. **Debug if needed:**
   - Enable `Show Debug` in Animation Blueprint
   - This displays current state and variable values

---

## Troubleshooting

### Problem: "AvatarAnimInstance not found"
**Solution:** Make sure C++ code is compiled. In Visual Studio/Rider, build the project, then restart Unreal Engine.

### Problem: "No animations playing"
**Solution:** Check that animations are assigned in each state. Use placeholder animations from UE5 Mannequin if needed.

### Problem: "State transitions not working"
**Solution:** Check that Speed variable is being updated in Event Graph. Add Print String nodes to debug values.

### Problem: "Facial expressions not working"
**Solution:** Verify your skeletal mesh has morph targets. Check names match exactly (case-sensitive).

---

## Next Steps

After completing this blueprint:

1. **Test with different animations** - Replace placeholder animations with final assets
2. **Add more gestures** - Create animation montages and play them through the Gestures slot
3. **Refine transitions** - Adjust blend times and conditions for smoother movement
4. **Create M_DTE_Skin material** - The next critical asset

---

## Success Criteria

You've successfully created the ABP if:

- [ ] Blueprint compiles without errors
- [ ] Character animates in viewport
- [ ] State transitions work (Idle/Walk/Run)
- [ ] Speed variable updates correctly
- [ ] Facial expression system is in place (even if not fully functional yet)

---

**Congratulations!** You've created the most critical asset for the Deep Tree Echo avatar. The avatar can now move and respond to input. Next, create the M_DTE_Skin material to make it look amazing!
