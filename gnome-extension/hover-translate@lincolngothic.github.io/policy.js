// SPDX-License-Identifier: GPL-3.0-or-later
// Coordinates are GNOME stage (logical) pixels. Capture ratios survive HiDPI.
export function moved(a, b) {
    return !a || (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 > 16;
}

export function contains(rect, point) {
    return point[0] >= rect.x && point[1] >= rect.y &&
        point[0] < rect.x + rect.width && point[1] < rect.y + rect.height;
}

export function captureArea(point, monitors, window = null) {
    const monitor = monitors.find(m => contains(m, point));
    if (!monitor)
        return null;
    let left = Math.max(monitor.x, point[0] - 350);
    let top = Math.max(monitor.y, point[1] - 80);
    let right = Math.min(monitor.x + monitor.width, point[0] + 350);
    let bottom = Math.min(monitor.y + monitor.height, point[1] + 80);
    if (window && contains(window, point)) {
        left = Math.max(left, window.x);
        top = Math.max(top, window.y);
        right = Math.min(right, window.x + window.width);
        bottom = Math.min(bottom, window.y + window.height);
    }
    return {x: Math.floor(left), y: Math.floor(top),
        width: Math.ceil(right) - Math.floor(left), height: Math.ceil(bottom) - Math.floor(top)};
}

export class HoverTracker {
    constructor(delay = 600) {
        this.delay = delay;
        this.reset();
    }

    reset() {
        this.anchor = null;
        this.blocked = true;
        this.attempted = false;
        this.since = 0;
    }

    update(point, blocked, now) {
        if (moved(this.anchor, point) || blocked !== this.blocked) {
            this.anchor = [...point];
            this.blocked = blocked;
            this.since = now;
            this.attempted = false;
            return 'invalidate';
        }
        if (blocked || this.attempted || now - this.since < this.delay)
            return 'idle';
        this.attempted = true;
        return 'capture';
    }

    dismiss() {
        this.attempted = true;
    }
}
