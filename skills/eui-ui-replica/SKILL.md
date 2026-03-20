---
name: eui-ui-replica
description: Create or refactor high-fidelity EUI interfaces from screenshots, mockups, or reference documents into fully interactive OpenGL examples or pages with real state, rich motion, blur, overlays, and layout fidelity. Use when Codex needs to recreate a supplied UI image in this EUI repository, build a polished demo in examples/, translate a visual design into EUI quick/layout code, or review an EUI recreation for overlap, layering, input, animation, blur, and interaction defects before they ship.
---

# EUI UI Replica

Use this skill to turn a screenshot or design reference into a production-grade EUI example that is visually close, fully interactive, and safe against the common failure modes already seen in this repo.

## Start Here

1. Inspect the reference image carefully before writing code.
2. Read [references/preflight-checklist.md](references/preflight-checklist.md).
3. Open the closest local patterns only as needed:
   - `examples/minimal_quick_demo.cpp` for quick/declarative layout style
   - `examples/reference_dashboard_demo.cpp` for interaction, overlay capture, charts, tooltips, dropdown behavior
   - `examples/financial_assistant_dashboard_demo.cpp` for light dashboard composition and overlay pass structure
   - `include/eui/quick/ui.h` for current builder capabilities
4. Build a layout skeleton first.
5. Add interaction state.
6. Add motion and blur only after layout and hit targets are stable.
7. Compile the single example source before invoking a full CMake build.

## Required Output Standard

Always produce:

- A real EUI screen, not a static painting.
- Real hover, click, selection, input, and menu behavior for visible controls.
- Motion that communicates state change, not decorative noise.
- Blur and overlays with correct stacking and pointer capture.
- Stable layout at the target screenshot size and at slightly smaller sizes.
- Code that matches repo conventions:
  - put standalone recreations in `examples/`
  - prefer `#define EUI_BACKEND_OPENGL` and `#define EUI_PLATFORM_GLFW` in the source
  - use `apply_patch` for edits

Do not stop at "looks similar". Make the visible controls work.

## Workflow

### 1. Decompose the reference

List these parts mentally before coding:

- Top-level shell and background
- Major rows and columns
- Cards and panel groups
- Primary controls
- Secondary controls
- Overlay surfaces: dropdowns, tooltips, popovers, dialogs
- Animated regions
- Repeated visual tokens: spacing, radius, font scale, accent, border softness

If the screenshot is dense, approximate the layout in 2 passes:

1. major containers
2. card internals

### 2. Choose the right EUI style

Prefer quick/declarative layout for:

- rows
- columns
- grids
- shell padding
- card body partitioning

Use manual `Rect` math inside a card when screenshot fidelity matters more than generic reflow.

Good pattern:

```cpp
ui.view(root)
    .padding(22.0f * scale)
    .column()
    .tracks({ px(...), px(...), fr() })
    .gap(18.0f * scale)
```

Then switch to manual `Rect` subdivision inside dense cards.

### 3. Implement state before effects

Create a compact state struct for:

- selected tabs
- toggles
- hovered chart index
- search text
- open menu id
- selected card ids
- rating / filter / range selection
- overlay capture rects

Do not fake state with hard-coded highlight colors only.

### 4. Paint in the correct order

Use this order unless there is a strong reason not to:

1. background glows
2. shell
3. static layout surfaces
4. card internals
5. foreground data marks
6. overlay pass

Important:

- Draw dropdown menus, quick-action menus, tooltips, and popovers in a final overlay pass.
- Do not draw open menus inline inside cards if later cards are still going to be painted.
- When an overlay is open, set an overlay-block rect and route hover/click checks through it.

### 5. Apply motion with intent

Use `ui.motion(...)` and `ui.presence(...)` for:

- hover lift
- selection indicators
- reveal / dismiss
- chart tooltip motion
- toggle pills
- pulsing mic / status halo

Do not animate everything. Animate the state transition boundary.

### 6. Make controls genuinely usable

For every visible control, confirm:

- hit target is large enough
- label is readable
- state changes on click
- motion confirms the change
- overlay closes when clicking outside
- overlay does not leak hover to content behind it

### 7. Verify before claiming done

Before finishing:

1. Re-read [references/preflight-checklist.md](references/preflight-checklist.md)
2. Compile the single file directly
3. If the repo CMake cache contains fetch flags that may trigger network dependency downloads, avoid `cmake --build` until the single-file compile is clean

Recommended compile approach on this repo:

- use the Visual Studio environment batch file first
- compile the target `.cpp` directly with `cl /c`
- only then run a full target build if needed

## Mandatory Guardrails

### Inputs with icons

If an input has a leading icon, reserve content space for it.

Prefer:

- builder/content padding support when available
- otherwise a custom input container with explicit text gutter

Never draw an icon over an input whose text starts at the default left inset.

### Text and track sizing

Before writing any large title or multi-line copy, budget vertical space explicitly.

If a row contains:

- large value text
- caption
- chips
- chart

then increase the parent track height first. Do not try to "fit" it later with negative offsets.

### Overlay surfaces

Menus and tooltips must satisfy all of these:

- visually above later-painted cards
- clipped to the intended shell only
- pointer-capturing while open
- closed on outside click

### Chart and tooltip layering

Data marks must remain above blur layers and panel haze.
Tooltips must be above data marks.

### Hover lift and shadows

If cards lift on hover, ensure surrounding gaps can absorb the lift without collisions.

### Scale behavior

Compute `scale` from framebuffer size, then audit all fixed heights that contain text.
A layout that fits at one size may collide once `scale` drops.

## Definition of Done

Only consider the recreation done when:

- the composition reads like the reference at first glance
- controls are real
- no obvious overlaps remain
- menu layering is correct
- input text does not collide with icons
- blur feels attached to the correct surface
- motion reads as deliberate and consistent
- the example compiles cleanly

