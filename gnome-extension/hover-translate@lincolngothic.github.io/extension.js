// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {HoverTracker, captureArea, contains, moved} from './policy.js';

const SERVICE = 'io.github.LincolnGothic.HoverTranslate.Gnome';
const PATH = '/io/github/LincolnGothic/HoverTranslate/Gnome';
const XML = `<node><interface name="${SERVICE}">
  <method name="Configure"><arg type="b" direction="in"/><arg type="i" direction="in"/></method>
  <method name="Result"><arg type="u" direction="in"/><arg type="s" direction="in"/>
    <arg type="s" direction="in"/><arg type="b" direction="in"/></method>
  <method name="GetStatus"><arg type="b" direction="out"/><arg type="u" direction="out"/>
    <arg type="b" direction="out"/><arg type="s" direction="out"/><arg type="s" direction="out"/>
    <arg type="i" direction="out"/><arg type="i" direction="out"/></method>
  <signal name="Capture"><arg type="u"/><arg type="ay"/><arg type="d"/><arg type="d"/></signal>
  <signal name="Invalidated"><arg type="u"/></signal>
  <signal name="Problem"><arg type="u"/><arg type="s"/></signal>
</interface></node>`;

// GNOME's screenshot module normally installs this promisified method, but do
// not depend on that module having been initialized before the extension.
Gio._promisify(Shell.Screenshot.prototype, 'screenshot_area');

export default class HoverTranslateExtension extends Extension {
    enable() {
        this._lifetime = (this._lifetime ?? 0) + 1;
        this._alive = true;
        this._enabled = false;
        this._owner = null;
        this._watch = 0;
        this._sequence = 0;
        this._capturing = false;
        this._escape = 0;
        this._tracker = new HoverTracker();
        this._popup = new St.BoxLayout({vertical: true, reactive: true,
            visible: false, style_class: 'hover-translate-popup'});
        this._source = new St.Label({style_class: 'hover-translate-source'});
        this._translation = new St.Label({style_class: 'hover-translate-result'});
        for (const label of [this._source, this._translation]) {
            label.clutter_text.line_wrap = true;
            label.clutter_text.ellipsize = 0;
            this._popup.add_child(label);
        }
        Main.layoutManager.addChrome(this._popup, {trackFullscreen: false});
        this._dbus = Gio.DBusExportedObject.wrapJSObject(XML, this);
        this._dbus.export(Gio.DBus.session, PATH);
        this._name = Gio.bus_own_name_on_connection(Gio.DBus.session, SERVICE,
            Gio.BusNameOwnerFlags.NONE, null, null);
        this._events = global.stage.connect('captured-event', (_stage, event) => {
            if (!this._enabled)
                return Clutter.EVENT_PROPAGATE;
            if (event.type() === Clutter.EventType.KEY_PRESS &&
                event.get_key_symbol() === Clutter.KEY_Escape) {
                this._tracker.dismiss();
                this._invalidate();
            } else if (event.type() === Clutter.EventType.SCROLL ||
                event.type() === Clutter.EventType.BUTTON_PRESS) {
                this._tracker.reset();
                this._invalidate();
            }
            return Clutter.EVENT_PROPAGATE;
        });
        this._accelerator = global.display.connect('accelerator-activated', (_display, action) => {
            if (action !== this._escape || !this._escape) return;
            this._tracker.dismiss(); this._invalidate();
        });
        this._timer = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 50, () => {
            try {
                this._poll();
            } catch (error) {
                this._fail(error);
            }
            return GLib.SOURCE_CONTINUE;
        });
    }

    ConfigureAsync([enabled, delay], invocation) {
        const sender = invocation.get_sender();
        if (this._owner && this._owner !== sender) {
            invocation.return_dbus_error(`${SERVICE}.Busy`, 'Another Hover Translate app is connected. Quit it first.');
            return;
        }
        if (delay < 100 || delay > 3000) {
            invocation.return_dbus_error(`${SERVICE}.Invalid`, 'Hover delay must be 100–3000 ms.');
            return;
        }
        this._enabled = enabled;
        this._tracker.delay = delay;
        this._tracker.reset();
        this._invalidate();
        if (enabled && !this._owner) {
            this._owner = sender;
            this._watch = Gio.bus_watch_name_on_connection(Gio.DBus.session, sender,
                Gio.BusNameWatcherFlags.NONE, null, () => this._disconnect());
        } else if (!enabled) {
            this._disconnect();
        }
        invocation.return_value(null);
    }

    ResultAsync([sequence, source, translation, error], invocation) {
        if (invocation.get_sender() !== this._owner) {
            invocation.return_dbus_error(`${SERVICE}.Denied`, 'Only the connected app can supply translations.');
            return;
        }
        invocation.return_value(null);
        if (!this._enabled || sequence !== this._sequence || this._blocked() ||
            moved(this._tracker.anchor, global.get_pointer()))
            return;
        if (!translation.trim())
            return;
        this._show(source, translation, error);
    }

    GetStatusAsync(_params, invocation) {
        if (this._owner && invocation.get_sender() !== this._owner) {
            invocation.return_dbus_error(`${SERVICE}.Denied`, 'Only the connected app can read hover text.');
            return;
        }
        const point = global.get_pointer();
        invocation.return_value(new GLib.Variant('(bubssii)', [this._enabled,
            this._sequence, this._popup.visible, this._source.get_text(),
            this._translation.get_text(), Math.round(point[0]), Math.round(point[1])]));
    }

    _disconnect() {
        this._enabled = false;
        this._owner = null;
        if (this._watch) {
            Gio.bus_unwatch_name(this._watch);
            this._watch = 0;
        }
        this._tracker.reset();
        this._invalidate();
    }

    _invalidate() {
        this._sequence = (this._sequence + 1) >>> 0;
        this._popup.hide();
        this._source.set_text('');
        this._translation.set_text('');
        this._releaseEscape();
        if (this._dbus)
            this._dbus.emit_signal('Invalidated', new GLib.Variant('(u)', [this._sequence]));
    }

    _windowAt(point) {
        return global.get_window_actors().reverse().find(actor =>
            actor.visible && !actor.meta_window.minimized &&
            actor.meta_window.get_workspace() === global.workspace_manager.get_active_workspace() &&
            contains(actor.meta_window.get_frame_rect(), point))?.meta_window;
    }

    _blocked() {
        if (Main.sessionMode.isLocked || Main.sessionMode.isGreeter ||
            Main.overview.visible || Main.modalCount > 0 || Main.screenshotUI?.visible)
            return true;
        const pointer = global.get_pointer();
        const mask = Clutter.ModifierType.BUTTON1_MASK | Clutter.ModifierType.BUTTON2_MASK |
            Clutter.ModifierType.BUTTON3_MASK | Clutter.ModifierType.CONTROL_MASK |
            Clutter.ModifierType.MOD1_MASK | Clutter.ModifierType.SUPER_MASK;
        if (pointer[2] & mask)
            return true;
        if (this._popup.visible && contains({x: this._popup.x, y: this._popup.y,
            width: this._popup.width, height: this._popup.height}, pointer))
            return true;
        const window = this._windowAt(pointer);
        const appId = window?.get_wm_class()?.toLowerCase() ?? '';
        return appId.includes('hovertranslate') || appId.includes('hover-translate');
    }

    _poll() {
        if (!this._enabled)
            return;
        const point = global.get_pointer().slice(0, 2);
        const action = this._tracker.update(point, this._blocked(), GLib.get_monotonic_time() / 1000);
        if (action === 'invalidate')
            this._invalidate();
        if (action === 'capture' && !this._capturing)
            this._capture(this._sequence, point);
        else if (action === 'capture')
            this._tracker.attempted = false;
    }

    async _capture(sequence, point) {
        const lifetime = this._lifetime;
        this._capturing = true;
        const stream = Gio.MemoryOutputStream.new_resizable();
        try {
            const crop = captureArea(point, Main.layoutManager.monitors, this._windowAt(point)?.get_frame_rect());
            if (!crop || crop.width < 1 || crop.height < 1)
                return;
            const shooter = new Shell.Screenshot();
            await shooter.screenshot_area(crop.x, crop.y, crop.width, crop.height, stream);
            stream.close(null);
            if (!this._alive || lifetime !== this._lifetime || !this._enabled || sequence !== this._sequence || this._blocked() ||
                moved(point, global.get_pointer()))
                return;
            const data = stream.steal_as_bytes().get_data();
            // Images stay in memory. No screenshot file or clipboard write.
            this._dbus.emit_signal('Capture', new GLib.Variant('(uaydd)',
                [sequence, data, (point[0] - crop.x) / crop.width, (point[1] - crop.y) / crop.height]));
        } catch (error) {
            if (lifetime === this._lifetime && sequence === this._sequence) this._fail(error);
        } finally {
            if (!stream.is_closed())
                stream.close(null);
            if (lifetime === this._lifetime) this._capturing = false;
        }
    }

    _show(source, translation, error) {
        this._source.set_text(source.slice(0, 1000));
        this._translation.set_text(translation.slice(0, 4000));
        if (error)
            this._translation.add_style_class_name('hover-translate-error');
        else
            this._translation.remove_style_class_name('hover-translate-error');
        const point = global.get_pointer();
        const monitor = Main.layoutManager.monitors.find(m => contains(m, point));
        if (!monitor)
            return;
        this._popup.set_width(Math.min(460, monitor.width - 24));
        this._popup.show();
        const height = this._popup.get_preferred_height(this._popup.width)[1];
        const x = Math.max(monitor.x + 8, Math.min(point[0] + 18, monitor.x + monitor.width - this._popup.width - 8));
        const below = point[1] + 24;
        const y = below + height < monitor.y + monitor.height ? below : Math.max(monitor.y + 8, point[1] - height - 18);
        this._popup.set_position(x, y);
        if (!this._escape) {
            this._escape = global.display.grab_accelerator('Escape', Meta.KeyBindingFlags.NONE);
            if (this._escape)
                Main.wm.allowKeybinding(Meta.external_binding_name_for_action(this._escape), Shell.ActionMode.NORMAL);
        }
    }

    _releaseEscape() {
        if (!this._escape) return;
        Main.wm.allowKeybinding(Meta.external_binding_name_for_action(this._escape), Shell.ActionMode.NONE);
        global.display.ungrab_accelerator(this._escape);
        this._escape = 0;
    }

    _fail(error) {
        if (this._alive && this._enabled) {
            const message = `GNOME hover capture failed: ${error.message}`;
            this._dbus.emit_signal('Problem', new GLib.Variant('(us)', [this._sequence, message]));
            this._show('', message, true);
        }
    }

    disable() {
        ++this._lifetime;
        this._alive = false;
        this._disconnect();
        GLib.source_remove(this._timer);
        global.stage.disconnect(this._events);
        global.display.disconnect(this._accelerator);
        Gio.bus_unown_name(this._name);
        this._dbus.unexport();
        this._dbus = null;
        this._popup.destroy();
    }
}
