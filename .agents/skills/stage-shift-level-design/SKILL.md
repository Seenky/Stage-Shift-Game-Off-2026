---
name: stage-shift-level-design
description: Plan, place, or review playable Stage Shift layouts in Unreal, including hubs, rehearsal rooms, entrances, routes, cameras, and existing theatre controls.
---

# Stage Shift Level Design

Read [Level Design and Level Art](../../../Docs/LevelDesignAndArt.md) and the active map. Use [Stage Shift Level Art](../stage-shift-level-art/SKILL.md) when placing visible geometry.

1. Trace the task through player starts, entry, target approach, interaction space and return. Record the current camera, pawn clearance and neighboring systems. Establish what the user authorized the room to do.
2. Inspect the closest working map and resolve the actual actor class, mesh and component settings before placing a replacement. Consult the [production controls](../../../Docs/LevelDesignAndArt.md#готовые-механизмы). Place an existing configured actor or reproduce its authored setup. Avoid obsolete static stand-ins. For an art-only pass, keep scenario bindings dormant and local.
3. Resolve the working face of each console from its geometry, controls, grip and live precedent. Approach that face with the actual pawn. Check clearance with openable and rotating parts at their limits. Use the real input or pawn movement in PIE. Check camera transitions both ways, including a pause or immediate reversal at a threshold, and keep other camera owners intact.
4. Verify the saved map again in PIE. Record the walked route and the tested interaction scope. A successful walk verifies collision and access. For walkable floor construction and contact with props, perform the separate visual checks in Level Art.
