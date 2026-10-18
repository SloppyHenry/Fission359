// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Where a label (a measure, a value bubble) of the 3D view can go so that nothing floating over
// the view hides it: the tool palette, the display bar, the view cube, the navigation buttons.
.pragma library

const kGap = 6

function overlaps(x, y, w, h, r) {
    return x < r.x + r.width + kGap && x + w > r.x - kGap &&
           y < r.y + r.height + kGap && y + h > r.y - kGap
}

// The middle (cx, cy) of a label of size w × h in a view of size viewWidth × viewHeight, moved
// out from under the `obstacles` (rectangles in the same coordinates) the shortest way that stays
// in the view. `mirror` (optional) is a point the label may jump across instead (the dot a label
// sits beside): its other side is tried first, so the label stays next to its dot.
function place(cx, cy, w, h, obstacles, viewWidth, viewHeight, mirror) {
    const inside = (x, y) => ({
        x: Math.min(Math.max(x, w / 2 + 4), viewWidth - w / 2 - 4),
        y: Math.min(Math.max(y, h / 2 + 4), viewHeight - h / 2 - 4)
    })
    const free = p => !(obstacles || []).some(r => r && r.width > 0 && r.height > 0
                                                   && overlaps(p.x - w / 2, p.y - h / 2, w, h, r))
    let p = inside(cx, cy)
    if (free(p)) {
        return p
    }
    if (mirror) {
        const across = inside(2 * mirror.x - cx, cy)
        if (free(across)) {
            return across
        }
    }
    // Out of each obstacle in the way, to whichever side is nearest, a few rounds.
    for (let round = 0; round < 4 && !free(p); ++round) {
        for (const r of obstacles || []) {
            if (!r || r.width <= 0 || r.height <= 0 || !overlaps(p.x - w / 2, p.y - h / 2, w, h, r)) {
                continue
            }
            const ways = [
                { x: r.x + r.width + kGap + w / 2, y: p.y },  // right of it
                { x: r.x - kGap - w / 2, y: p.y },            // left of it
                { x: p.x, y: r.y + r.height + kGap + h / 2 }, // below it
                { x: p.x, y: r.y - kGap - h / 2 }             // above it
            ].map(q => inside(q.x, q.y))
             .filter(q => !overlaps(q.x - w / 2, q.y - h / 2, w, h, r))
            ways.sort((a, b) => Math.hypot(a.x - p.x, a.y - p.y) - Math.hypot(b.x - p.x, b.y - p.y))
            if (ways.length > 0) {
                p = ways[0]
            }
        }
    }
    return p
}
