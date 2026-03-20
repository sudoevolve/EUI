# EUI Replica Preflight Checklist

Read this before coding and again before closing the task.

## Screenshot Decomposition

- Identify the exact target size and aspect ratio.
- Count primary rows and columns before coding.
- Mark which areas are real controls versus decorative shapes.
- Decide which surfaces need blur, which need shadow, and which need neither.

## Layout Failure Prediction

Check these before writing detailed internals:

- Is any header/search/action row using the full parent slot height when the control should be shorter?
- Does any multi-line title sit in a track that was sized like a single-line label?
- Do small cards contain more text than their current height can support?
- Will hover lift collide with neighboring cards because the gap is too small?
- Are chart labels and values sharing a region that is too shallow?

If any answer is "yes", fix the track heights first.

## Input Failure Prediction

- Does a search or input field have a leading icon?
- If yes, did you reserve left content padding inside the text field?
- Is placeholder text still visible without touching the icon?
- Is the clickable input rect still the full visible field?

If icon and text share the same default inset, they will overlap.

## Overlay Failure Prediction

- Is a dropdown or tooltip being drawn inline inside a card function?
- Are there sibling cards painted after it?
- If yes, it will likely be covered later.

Use a final overlay pass for:

- dropdowns
- popovers
- quick-action menus
- tooltips that must stay topmost

Also confirm:

- open overlay stores a blocking rect
- hover/click checks ignore covered content
- outside click closes the overlay

## Blur Failure Prediction

- Is the blur attached to the panel/surface that should own it?
- Are data marks being drawn after the blur layer?
- Is the blur subtle enough not to wash out foreground content?
- Are you blurring the intended backdrop, not a decorative shape in front?

If a blur looks colored, first check the fill/gradient color of the blur host.

## Typography Failure Prediction

- Title, subtitle, value, meta sizes are tokenized and consistent.
- Large numbers have enough width and height.
- Tiny meta text is not stacked too close to stronger headings.
- Labels do not depend on lucky hard-coded y offsets.

## Interaction Failure Prediction

- Every visible button changes real state.
- Every selected state has a visible animated confirmation.
- Hover states do not remain after the cursor leaves.
- Open menus do not allow hidden controls behind them to hover.
- Charts, ratings, toggles, and pills all have usable hit areas.

## Scaling Failure Prediction

Before finishing, imagine the same scene at:

- target size
- about 10% narrower
- about 10% shorter

Then check:

- title wrapping
- card body crowding
- control row heights
- chart labels
- overlay placement near the shell edge

## Verification Sequence

1. Compile the example source directly.
2. If compile is clean, link it or build the exact target.
3. Interact with:
   - search fields
   - dropdowns
   - toggle pills
   - charts
   - rating / card selection
4. Re-open all overlays and confirm nothing behind them still reacts.

