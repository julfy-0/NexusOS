# NexusOS GUI Window System 2.0

NexusOS 0.5.7 introduces a small native window-management layer instead of
letting every GUI application own its own unrelated geometry.

## Window contract

Each window has:

- stable window ID
- title
- bounds (`x`, `y`, `width`, `height`)
- visibility
- focus state
- active-window tracking

The manager provides responsive initial placement, close handling, title-bar
focus and mouse dragging. It is intentionally bounded (`GUI_WINDOW_MAX = 8`)
and allocation-free so it can run in the current freestanding kernel GUI.

## Current applications

- Files
- Terminal
- Settings
- Search

These applications now share the same window geometry and title-bar contract.
Desktop remains the root surface and application launch point.

## Limitations

This is not yet a compositing server or userspace GUI protocol. Windows are
still rendered by the existing kernel GUI, and there is no overlapping
multi-window compositor yet. The foundation is deliberately small so it can be
moved to a future process/userspace GUI architecture without replacing the
existing renderer.
