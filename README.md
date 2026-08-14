# crayon

https://github.com/user-attachments/assets/8bcbdc80-6cfe-4efa-b948-e219e2a11b78

A freehand drawing application written in C on SDL3, with no engine or framework underneath it.

The goal is a fast, minimal canvas where every layer — input handling, brush rasterization, stroke storage, undo — is written explicitly rather than inherited from a toolkit.

## Building

Requires SDL3 and a C compiler.

```sh
cc crayon.c -o crayon $(pkg-config --cflags --libs sdl3) -lm
```

Then:

```sh
./crayon
```

## Controls

| Input | Action |
| --- | --- |
| Left mouse | Draw |
| `F` / `J` (hold) | Draw at the current cursor position |
| Middle mouse (drag) | Pan the canvas |
| Mouse wheel | Resize brush (10–128 px) |
| Mouse 4 / `D` / `K` | Undo |
| Mouse 5 / `G` / `H` | Redo |
| Click the palette | Change brush color |

The palette is a vertical strip of eight swatches on the right edge of the window, sized relative to the smaller window dimension so it scales sanely on resize.

Keyboard drawing and mouse drawing are tracked as distinct input sources, so releasing the mouse won't terminate a stroke started with the keyboard, and vice versa.

## Design

### Stroke storage

Every brush stamp position ever placed lives in a single flat `PointPool` — a heap array of `SDL_FPoint` that doubles when full. A `Stroke` is not a container; it's a `{ start, length }` window into that pool, plus the color and width captured at the moment the stroke began.

This means brush settings are copied into each stroke on creation, so changing the color or width later never retroactively alters strokes already on the canvas.

### Undo and redo

Undo is a watermark rather than a snapshot. Undoing decrements the stroke count and rewinds `pool.count` back to the start index of the removed stroke — the point data is still physically there, just no longer considered live. Redo walks the watermark forward again.

The alternative, snapshotting canvas state per operation, scales badly with stroke count. This approach makes both operations O(1) regardless of how much has been drawn.

Beginning a new stroke resets the redo counter, discarding the redo branch — the usual behavior for a linear undo history.

### Brush rendering

The circle brush is rasterized once at startup into a 512×512 RGBA surface and uploaded as a texture. Alpha falls off linearly with squared distance from the center, giving a soft edge without a separate antialiasing pass. Per-stroke color is applied at draw time with `SDL_SetTextureColorMod`, so a single texture serves every color.

Stamps are placed along each mouse segment at fixed 2px spacing by `point_pool_add_segment`, so fast cursor movement produces a continuous line rather than a dotted trail.

### Coordinates

Points are stored in world space and converted to screen space at render time using the current pan offset. Panning therefore costs nothing beyond updating two floats — no stored geometry is touched.

### Event loop

Rendering is gated behind a `redraw` flag; the canvas is only redrawn when something has actually changed. When the window loses focus the loop switches from `SDL_PollEvent` to `SDL_WaitEvent`, so an idle unfocused window consumes no CPU. VSync is disabled deliberately to keep input-to-pixel latency as low as possible.

## Known limitations

This is in active development. Currently missing or unfinished:

- No file save or load — closing the window discards the drawing.
- No zoom; the canvas pans but does not scale.
- Only one brush shape. `BrushType` is stubbed out in the struct.
- Initial pool allocations in `app_init` assume success and are not checked.
- No cleanup path on exit: allocations, the texture, the renderer, and the window are left to the OS, and `SDL_Quit` is never called.
- Panning mid-stroke shifts the world position of the last recorded point, which can produce a jump in the next segment.
- Pen input is enumerated but not yet handled.

## Structure

Single translation unit. Rough layout, top to bottom:

- Color presets and UI region types
- `PointPool` / `Stroke` / `StrokeList` / `App` definitions
- UI hit testing and palette geometry
- Coordinate transforms and `rasterize_circle`
- Pool and list growth, stroke lifecycle (`begin` / `extend` / `end`), undo/redo, panning
- `handle_event` — input only, no rendering
- `draw_stroke`, `draw_color_palette_ui`, `render`
- `main` — SDL setup and the event loop
