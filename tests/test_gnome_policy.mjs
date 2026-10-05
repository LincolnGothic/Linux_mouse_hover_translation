// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict';
import {captureArea, HoverTracker} from '../gnome-extension/hover-translate@lincolngothic.github.io/policy.js';
const tracker = new HoverTracker(600);
assert.equal(tracker.update([100,100], false, 0), 'invalidate');
assert.equal(tracker.update([102,100], false, 599), 'idle');
assert.equal(tracker.update([100,100], false, 600), 'capture');
assert.equal(tracker.update([100,100], false, 1600), 'idle');
tracker.dismiss();
assert.equal(tracker.update([100,100], false, 2000), 'idle');
assert.equal(tracker.update([120,100], false, 2100), 'invalidate');
assert.equal(tracker.update([120,100], true, 2800), 'invalidate');
assert.equal(tracker.update([120,100], true, 4000), 'idle');
assert.equal(tracker.update([120,100], false, 4000), 'invalidate');
assert.equal(tracker.update([120,100], false, 4600), 'capture');
const monitors = [{x:0,y:0,width:1920,height:1080}, {x:1920,y:0,width:2560,height:1440}];
assert.deepEqual(captureArea([1950,20],monitors), {x:1920,y:0,width:380,height:100});
assert.deepEqual(captureArea([100,100],monitors,{x:50,y:80,width:160,height:100}),
    {x:50,y:80,width:160,height:100});
assert.equal(captureArea([-500,100],monitors),null);
assert.deepEqual(captureArea([-20,40],[{x:-1920,y:0,width:1920,height:1080}]),
    {x:-370,y:0,width:370,height:120});
console.log('PASS GNOME dwell, dismissal, blocking, monitor edges and window clipping');
