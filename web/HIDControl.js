'use strict';

// CONFIG & CONSTANTS
const CFG = {
    T_TYPING: 500,
    T_DBLCLICK: 400,
    T_LAYOUT: 500,
    CHUNK_SIZE: 100,
    CHUNK_DELAY: 150,
    LINE_DELAY: 120,
    DEFAULT_LAYOUT: "US",
    MOUSE_TICK: 50
};

const KEYS = {
    SPEC: {
        "Enter":"ENTER","Tab":"TAB","Escape":"ESC"," ":"SPACE","Backspace":"BACKSPACE",
        "Delete":"DELETE","Insert":"INSERT","Home":"HOME","End":"END","PageUp":"PAGEUP",
        "PageDown":"PAGEDOWN","ArrowUp":"UP","ArrowDown":"DOWN","ArrowLeft":"LEFT",
        "ArrowRight":"RIGHT","CapsLock":"CAPSLOCK","NumLock":"NUMLOCK","ScrollLock":"SCROLLLOCK",
        "PrintScreen":"PRINTSCREEN","Pause":"PAUSE"
    },
    SHIFT: {
        '1':'!','2':'@','3':'#','4':'$','5':'%','6':'^','7':'&','8':'*','9':'(','0':')',
        '-':'_','=':'+','[':'{',']':'}','\\':'|',';':':','\'':'"',',':'<','.':'>','/':'?','`':'~'
    }
};

const keyboardLayouts = {
    win: [
        { v: "US",      l: "🇺🇸 US" },
        { v: "BE",      l: "🇧🇪 BE" },
        { v: "BG",      l: "🇧🇬 BG" },

        { v: "CA_CMS",  l: "🇨🇦 CA-CMS" },
        { v: "CA_FR",   l: "🇨🇦 CA-FR" },

        { v: "CH_DE",   l: "🇨🇭 CH-DE" },
        { v: "CH_FR",   l: "🇨🇭 CH-FR" },

        { v: "CZ",      l: "🇨🇿 CZ" },
        { v: "DE",      l: "🇩🇪 DE" },
        { v: "DK",      l: "🇩🇰 DK" },
        { v: "EE",      l: "🇪🇪 EE" },

        { v: "ES",      l: "🇪🇸 ES" },
        { v: "ES_LA",   l: "🇪🇸 ES-LA" },

        { v: "FI",      l: "🇫🇮 FI" },
        { v: "FR",      l: "🇫🇷 FR" },
        { v: "GB",      l: "🇬🇧 GB" },
        { v: "GR",      l: "🇬🇷 GR" },
        { v: "HR",      l: "🇭🇷 HR" },
        { v: "HU",      l: "🇭🇺 HU" },
        { v: "IE",      l: "🇮🇪 IE" },
        { v: "IN",      l: "🇮🇳 IN" },
        { v: "IS",      l: "🇮🇸 IS" },
        { v: "IT",      l: "🇮🇹 IT" },

        { v: "LT",      l: "🇱🇹 LT" },
        { v: "LV",      l: "🇱🇻 LV" },
        { v: "NL",      l: "🇳🇱 NL" },
        { v: "NO",      l: "🇳🇴 NO" },
        { v: "PL",      l: "🇵🇱 PL" },

        { v: "PT",      l: "🇵🇹 PT" },
        { v: "PT_BR",   l: "🇧🇷 PT-BR" },

        { v: "RO",      l: "🇷🇴 RO" },
        { v: "RU",      l: "🇷🇺 RU" },
        { v: "SE",      l: "🇸🇪 SE" },
        { v: "SI",      l: "🇸🇮 SI" },
        { v: "SK",      l: "🇸🇰 SK" },
        { v: "TR",      l: "🇹🇷 TR" },
        { v: "UA",      l: "🇺🇦 UA" }
    ],

    mac: [
        { v: "US_MAC",        l: "🇺🇸 US (Mac)" },
        { v: "BE_MAC",        l: "🇧🇪 BE (Mac)" },
        { v: "BG_MAC",        l: "🇧🇬 BG (Mac)" },

        { v: "CA_FR_MAC",     l: "🇨🇦 CA-FR (Mac)" },

        { v: "CH_DE_MAC",     l: "🇨🇭 CH-DE (Mac)" },
        { v: "CH_FR_MAC",     l: "🇨🇭 CH-FR (Mac)" },

        { v: "CZ_MAC",        l: "🇨🇿 CZ (Mac)" },
        { v: "DE_MAC",        l: "🇩🇪 DE (Mac)" },
        { v: "DK_MAC",        l: "🇩🇰 DK (Mac)" },
        { v: "EE_MAC",        l: "🇪🇪 EE (Mac)" },

        { v: "ES_MAC",        l: "🇪🇸 ES (Mac)" },
        { v: "ES_LA_MAC",     l: "🇪🇸 ES-LA (Mac)" },

        { v: "FI_MAC",        l: "🇫🇮 FI (Mac)" },
        { v: "FR_MAC",        l: "🇫🇷 FR (Mac)" },
        { v: "GB_MAC",        l: "🇬🇧 GB (Mac)" },
        { v: "GR_MAC",        l: "🇬🇷 GR (Mac)" },
        { v: "HR_MAC",        l: "🇭🇷 HR (Mac)" },
        { v: "HU_MAC",        l: "🇭🇺 HU (Mac)" },
        { v: "IN_MAC",        l: "🇮🇳 IN (Mac)" },
        { v: "IS_MAC",        l: "🇮🇸 IS (Mac)" },
        { v: "IT_MAC",        l: "🇮🇹 IT (Mac)" },

        { v: "LT_MAC",        l: "🇱🇹 LT (Mac)" },
        { v: "LV_MAC",        l: "🇱🇻 LV (Mac)" },
        { v: "NL_MAC",        l: "🇳🇱 NL (Mac)" },
        { v: "NO_MAC",        l: "🇳🇴 NO (Mac)" },
        { v: "PL_MAC",        l: "🇵🇱 PL (Mac)" },

        { v: "PT_MAC",        l: "🇵🇹 PT (Mac)" },
        { v: "PT_BR_MAC",     l: "🇧🇷 PT-BR (Mac)" },

        { v: "RO_MAC",        l: "🇷🇴 RO (Mac)" },
        { v: "RU_MAC",        l: "🇷🇺 RU (Mac)" },
        { v: "SE_MAC",        l: "🇸🇪 SE (Mac)" },
        { v: "SI_MAC",        l: "🇸🇮 SI (Mac)" },
        { v: "SK_MAC",        l: "🇸🇰 SK (Mac)" },
        { v: "TR_MAC",        l: "🇹🇷 TR (Mac)" },
        { v: "UA_MAC",        l: "🇺🇦 UA (Mac)" }
    ]
};

// STATE MANAGER
class State {
    constructor() {
        this.mods = new Set();
        this.layout = CFG.DEFAULT_LAYOUT;
        this.conn = false;
        this.typing = false;
        this.lastMod = {m:null, t:0};
        this.listeners = {};
    }
    
    hasMod(m) { return this.mods.has(m); }
    addMod(m) { this.mods.add(m); this._emit('mods'); }
    delMod(m) { this.mods.delete(m); this._emit('mods'); }
    clearMods() { this.mods.clear(); this._emit('mods'); }
    getMods() { return Array.from(this.mods); }
    
    setConn(v) { this.conn = v; this._emit('conn'); }
    
    setTyping(v) { this.typing = v; this._emit('typing'); }
    
    setLayout(v) { this.layout = v; this._emit('layout'); }
    
    on(e, cb) {
        if(!this.listeners[e]) this.listeners[e] = [];
        this.listeners[e].push(cb);
    }
    _emit(e) {
        if(this.listeners[e]) this.listeners[e].forEach(cb => cb());
    }
}

// CONNECTION
class Conn {
    constructor(state) {
        this.s = state;
    }
    
    get ok() {
        try {
            if (typeof WSManager !== 'undefined' && WSManager.ws && WSManager.ws.readyState === 1) {
                return true;
            }

            if (window.WSManager && window.WSManager.ws && window.WSManager.ws.readyState === 1) {
                return true;
            }

            return false;
        } catch (e) {return false;}
    }
    
    send(cmd, ack = false) {
        if(!this.ok) {
            Log.err('Not connected');
            alert('WebSocket not connected');
            return false;
        }
        if(typeof ws_send === 'function') {
            let key = ack ? "key_ack" : "key";
            ws_send(`${key} ${cmd}`, res => {
                Log.dbg(`→ ${cmd} | ${res}`);
            });
            return true;
        }
        return false;
    }
    
    init() {
        const orig = window.ws_connected;
        window.ws_connected = () => {
            if(orig) orig();
            this.s.setConn(true);
            Log.info('✓ Connected');
            ws_update_status();
            start_status_interval();
            setTimeout(() => HID.changeLayout(CFG.DEFAULT_LAYOUT), CFG.T_LAYOUT);
        };
        if(typeof ws_init === 'function') ws_init();
    }
}

// COMMAND PROCESSOR
class Cmd {
    constructor(conn, state) {
        this.c = conn;
        this.s = state;
    }
    
    exec(cmd, ack = false) {
        const ok = this.c.send(cmd, ack);
        if(ok) {
            this.s.setTyping(true);
            setTimeout(() => this.s.setTyping(false), CFG.T_TYPING);
        }
        return ok;
    }
    
    key(k) {
        const m = this.s.getMods();
        if(m.length > 0) {
            this.exec([...m, k].join(' '));
            this.s.clearMods();
        } else {
            this.exec(k);
        }
    }
    
    combo(...k) {
        return this.exec(k.join(' '));
    }
    
    str(t) {
        if(t === '\\') t = '\\\\';
        return this.exec(`STRING ${t}`);
    }
}

// CHARACTER PROCESSOR
class Char {
    constructor(cmd, state) {
        this.c = cmd;
        this.s = state;
    }
    
    send(ch) {
        if(ch === ' ') return this.c.key('SPACE');
        if(ch === '\\') ch = '\\\\';
        
        if(this.s.hasMod('SHIFT')) {
            ch = this._shift(ch);
            this.s.delMod('SHIFT');
        }
        
        const m = this.s.getMods();
        if(m.length > 0) {
            this.c.exec([...m, ch].join(' '));
            this.s.clearMods();
        } else {
            this.c.str(ch);
        }
    }
    
    _shift(ch) {
        if(KEYS.SHIFT[ch]) return KEYS.SHIFT[ch];
        if(ch >= 'a' && ch <= 'z') return ch.toUpperCase();
        return ch;
    }
}

// TEXT BLOCK
class TextBlock {
    constructor(cmd) {
        this.c = cmd;
    }
    
    _wait(ms = 10000) {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                window.removeEventListener("key_ack", handler);
                reject(new Error("TIMEOUT"));
            }, ms);

            const handler = (e) => {
                clearTimeout(timer);
                window.removeEventListener("key_ack", handler);
                if (e.detail === "OK") resolve();
                else reject(new Error("DEVICE_ERROR"));
            };
            window.addEventListener("key_ack", handler);
        });
    }

    async send(txt, addEnter) {
        if (!txt) return;

        Status.set("running");
        const lines = txt.split('\n');

        try {
            for (let i = 0; i < lines.length; i++) {
                const ln = lines[i];

                if (ln.length > 0) {
                    for (let j = 0; j < ln.length; j += CFG.CHUNK_SIZE) {
                        const chunk = ln.substring(j, j + CFG.CHUNK_SIZE);
                        this.c.exec(`STRING ${chunk}\n`, true); 
                        await this._wait(); 
                    }
                }
                
                const isLastLine = i === lines.length - 1;
                if (!isLastLine || addEnter) {
                    this.c.exec('ENTER\n', true);
                    await this._wait();
                }
            }
        } catch (err) {
            console.error("Error in TextBlock:", err.message);
            Status.set("error");
        } finally {
            Status.set("connected");
            start_status_interval();
        }
    }
}

// EVENT HANDLER
class Evt {
    constructor(charProc, cmdProc, state) {
        this.ch = charProc;
        this.cmd = cmdProc;
        this.s = state;
        this.lastVal = '';
    }
    
    keyDown(e) {
        if(!this._ok(e)) return;
        this._updateMods(e);
        
        const k = this._mapKey(e.key);
        if(k) {
            e.preventDefault();
            this.cmd.key(k);
            return;
        }
        
        if(e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
            e.preventDefault();
            this.ch.send(e.key);
        }
    }
    
    keyUp(e) {
        if(!e.ctrlKey) this.s.delMod('CTRL');
        if(!e.shiftKey) this.s.delMod('SHIFT');
        if(!e.altKey) this.s.delMod('ALT');
        if(!e.metaKey) this.s.delMod('GUI');
    }
    
    input(e) {
        if(!this._ok(e)) return;
        const nv = e.target.value;
        const ov = this.lastVal;
        if(nv === ov) return;
        
        if(nv.length > ov.length) {
            const add = nv.substring(ov.length);
            for(const c of add) {
                if(c === '\n' || c === '\r') continue;
                this.ch.send(c);
            }
        } else {
            const diff = ov.length - nv.length;
            for(let i = 0; i < diff; i++) {
                this.cmd.key('BACKSPACE');
            }
        }
        this.lastVal = nv;
    }
    
    _ok(e) {
        const cb = document.getElementById('enableKeyboard');
        return cb?.checked && e.target.id === 'keyboardInput';
    }
    
    _mapKey(k) {
        if(k.startsWith('F') && k.length <= 3) return k.toUpperCase();
        return KEYS.SPEC[k] || null;
    }
    
    _updateMods(e) {
        if(e.ctrlKey) this.s.addMod('CTRL');
        if(e.shiftKey) this.s.addMod('SHIFT');
        if(e.altKey) this.s.addMod('ALT');
        if(e.metaKey) this.s.addMod('GUI');
    }
}

// MODIFIER MANAGER
class Mod {
    constructor(state, cmd) {
        this.s = state;
        this.c = cmd;
    }
    
    toggle(m) {
        const now = Date.now();
        const last = this.s.lastMod;
        
        if(last.m === m && (now - last.t) < CFG.T_DBLCLICK) {
            Log.dbg(`DblClick: ${m}`);
            this.s.delMod(m);
            this.c.exec(m);
            this.s.lastMod = {m:null, t:0};
            return;
        }
        
        this.s.lastMod = {m, t:now};
        this.s.hasMod(m) ? this.s.delMod(m) : this.s.addMod(m);
    }
}

// UI CONTROLLER
class UI {
    constructor(state) {
        this.s = state;
        state.on('conn', () => this.updConn());
        state.on('typing', () => this.updTyping());
        state.on('mods', () => this.updMods());
        state.on('layout', () => this.updLayout());
    }
    
    updConn() {
        const el = document.getElementById('keyboardStatus');
        if(el) el.className = this.s.conn ? 'status-indicator status-connected' : 'status-indicator';
    }
    
    updTyping() {
        const el = document.getElementById('keyboardStatus');
        if(!el) return;
        if(this.s.typing) el.className = 'status-indicator status-typing';
        else if(this.s.conn) el.className = 'status-indicator status-connected';
    }
    
    updMods() {
        document.querySelectorAll('.modifier-btn').forEach(btn => {
            const m = btn.getAttribute('data-mod');
            if(m) btn.classList.toggle('active', this.s.hasMod(m));
        });
    }
    
    updLayout() {
        const ind = document.getElementById('currentLayout');
        if(ind) ind.textContent = `Current: ${this.s.layout}`;
        
        const sel = document.getElementById('keyboardLayout');
        if(sel) {
            sel.style.background = '#3c5';
            setTimeout(() => sel.style.background = '', CFG.T_LAYOUT);
        }
    }
}

// LOGGER
const Log = {
    dbg(m) { this._log(m, '#3c5'); },
    info(m) { this._log(m, '#5af'); },
    err(m) { console.error(m); this._log(`ERR: ${m}`, '#f55'); },
    
    _log(m, c) {
        const cb = document.getElementById('showKeys');
        if(!cb?.checked) return;
        
        const log = document.getElementById('debugLog');
        if(!log) return;
        
        const div = document.createElement('div');
        div.textContent = `[${new Date().toLocaleTimeString()}] ${m}`;
        div.style.color = c;
        log.appendChild(div);
        log.scrollTop = log.scrollHeight;
        
        const sec = document.getElementById('debugSection');
        if(sec) sec.style.display = 'block';
    }
};

// MAIN CONTROLLER
class HIDCtrl {
    constructor() {
        this.st = new State();
        this.conn = new Conn(this.st);
        this.cmd = new Cmd(this.conn, this.st);
        this.ch = new Char(this.cmd, this.st);
        this.txt = new TextBlock(this.cmd);
        this.mod = new Mod(this.st, this.cmd);
        this.evt = new Evt(this.ch, this.cmd, this.st);
        this.ui = new UI(this.st);

        this.rec = new Recorder();
        this.mouse = new Mouse(this.cmd, this.rec);
    }
    
    sendKey(k) { return this.cmd.key(k); }
    sendCombo(...k) { return this.cmd.combo(...k); }
    sendCommand(c) { return this.cmd.exec(c); }
    sendChar(c) { return this.ch.send(c); }
    sendTextBlock(t, e) { return this.txt.send(t, e); }
    toggleMod(m) { return this.mod.toggle(m); }
    changeLayout(l) {
        this.cmd.exec(`LOCALE ${l}`);
        this.st.setLayout(l);
    }
    
    init() {
        this.conn.init();
        this._attach();
        this._initTrackpad();
        Log.info('✓ HID Ready');
    }
    
    _attach() {
        const inp = document.getElementById('keyboardInput');
        if(!inp) return;
        
        inp.addEventListener('keydown', e => this.evt.keyDown(e));
        inp.addEventListener('keyup', e => this.evt.keyUp(e));
        inp.addEventListener('input', e => this.evt.input(e));
        
        document.querySelectorAll('.key-btn[data-key]').forEach(btn => {
            btn.onclick = e => {
                e.preventDefault();
                e.stopPropagation();
                this.sendKey(btn.getAttribute('data-key'));
            };
        });
        
        document.querySelectorAll('.modifier-btn[data-mod]').forEach(btn => {
            btn.onclick = e => {
                e.preventDefault();
                e.stopPropagation();
                this.toggleMod(btn.getAttribute('data-mod'));
            };
        });
    }

    _initTrackpad() {
        const pad = document.getElementById("trackpad");
        if (!pad) return;
        let touchStart = 0, moved = false;

        pad.addEventListener("pointerdown", e => {
            pad.setPointerCapture(e.pointerId);
            this.mouse.active = true;
            touchStart = Date.now();
            moved = false;
            this.mouse.dx = 0; this.mouse.dy = 0;
            this.mouse.startLoop();
        });

        pad.addEventListener("pointermove", e => {
            if (!this.mouse.active) return;
            if (Math.abs(e.movementX) > 2 || Math.abs(e.movementY) > 2) moved = true;
            const speed = parseInt(document.getElementById("mouseSpeed")?.value || 2);
            this.mouse.dx += e.movementX * (speed * 0.5);
            this.mouse.dy += e.movementY * (speed * 0.5);
        });

        pad.addEventListener("pointerup", e => {
            this.mouse.active = false;
            if ((Date.now() - touchStart) < 200 && !moved) this.mouse.click(1);
        });

        pad.addEventListener("pointerleave", () => this.mouse.active = false);
    }
}

class Mouse {
    constructor(cmdProc, recorder) {
        this.cmd = cmdProc;
        this.rec = recorder;
        this.dx = 0;
        this.dy = 0;
        this.active = false;
        this.dragging = false;
        this.interval = null;
        this.dragging = false;
    }

    move(dx, dy) {
        const c = `M_MOVE ${dx} ${dy}`;
        if (this.cmd.exec(c)) this.rec.record(c);
    }

    click(btn) {
        if (this.dragging) {
            this.toggleDrag(); 
        }
        const c = `M_CLICK ${btn}`;
        if (this.cmd.exec(c)) this.rec.record(c);
    }

    scroll(dir) {
        const amt = parseInt(document.getElementById("scrollAmount")?.value || 1);
        const c = `M_SCROLL ${amt * dir}`;
        if (this.cmd.exec(c)) this.rec.record(c);
    }

    toggleDrag() {
        this.dragging = !this.dragging;
        const btn = document.getElementById("dragBtn");
        if (btn) {
            if (this.dragging) {
                btn.classList.add("selected");
                Log.dbg("→ Mouse Drag: ON");
            } else {
                btn.classList.remove("selected");
                Log.dbg("→ Mouse Drag: OFF");
            }
        }
        const c = this.dragging ? "M_PRESS 1" : "M_RELEASE 1";
        if (this.cmd.exec(c)) this.rec.record(c);
    }

    tick() {
        if (!this.active && Math.abs(this.dx) < 0.1 && Math.abs(this.dy) < 0.1) {
            this.stopLoop();
            return;
        }
        const idx = Math.round(this.dx);
        const idy = Math.round(this.dy);
        if (idx !== 0 || idy !== 0) {
            this.move(idx, idy);
            this.dx -= idx;
            this.dy -= idy;
        }
    }

    startLoop() {
        if (!this.interval) this.interval = setInterval(() => this.tick(), CFG.MOUSE_TICK);
    }

    stopLoop() {
        if (this.interval) { clearInterval(this.interval); this.interval = null; }
    }
}

// RECORDER & INTERPRETER
class Recorder {
    record(cmd) {
        const log = document.getElementById('mouseLog');
        if (!log) return;
        if (log.textContent.includes("Move the trackpad")) log.innerHTML = "";
        
        const div = document.createElement('div');
        div.className = "mouse-cmd";
        div.textContent = cmd;
        div.style.borderBottom = "1px solid #222";
        div.style.fontSize = "11px";
        log.appendChild(div);
        log.scrollTop = log.scrollHeight;
    }

    clear() {
        const log = document.getElementById('mouseLog');
        if (log) log.innerHTML = 'Move the trackpad to record commands...';
    }

    copy() {
        const entries = document.querySelectorAll('.mouse-cmd');
        const text = Array.from(entries).map(e => e.textContent).join('\n');
        
        if (!text) return;

        const textArea = document.createElement("textarea");
        textArea.value = text;
        
        textArea.style.position = "fixed";
        textArea.style.left = "-9999px";
        textArea.style.top = "0";
        document.body.appendChild(textArea);
        
        textArea.focus();
        textArea.select();
        textArea.setSelectionRange(0, textArea.value.length);

        try {
            const successful = document.execCommand('copy');
            if (successful) {
                const log = document.getElementById('mouseLog');
                if (log) {
                    const oldBg = log.style.background;
                    log.style.background = "#050"; 
                    setTimeout(() => log.style.background = oldBg, 500);
                }
                alert("Copied commands");
            }
        } catch (err) {
            console.error("Copy error:", err);
            alert("Error copying to clipboard");
        }

        document.body.removeChild(textArea);
    }
}

// GLOBAL API
let HID, AppState;

function sendKey(k) { return HID?.sendKey(k); }
function sendCombo(...k) { return HID?.sendCombo(...k); }
function sendKeyCommand(c) { return HID?.sendCommand(c); }
function toggleMod(m) { return HID?.toggleMod(m); }
function typeChar(c) { return HID?.sendChar(c); }

// Mouse API
function mouseClick(b) { HID?.mouse.click(b); }
function mouseScroll(d) { HID?.mouse.scroll(d); }
function toggleDrag() { HID?.mouse.toggleDrag(); }
function clearMouseLog() { HID?.rec.clear(); }
function copyMouseLog() { HID?.rec.copy(); }

function sendTextBlock() {
    const txt = document.getElementById('textBlock');
    const cb = document.getElementById('addEnterBlock');
    if(txt) HID?.sendTextBlock(txt.value, cb?.checked);
}

function clearTextBlock() {
    const txt = document.getElementById('textBlock');
    if(txt) txt.value = '';
}

function changeLayout() {
    const sel = document.getElementById('keyboardLayout');
    if(sel) HID?.changeLayout(sel.value);
}

function populateLayouts() {
    const os = E("targetOs").value;
    const layoutSelect = E("keyboardLayout");
    layoutSelect.innerHTML = "";

    keyboardLayouts[os].forEach((k, i) => {
        const opt = document.createElement("option");
        opt.value = k.v;
        opt.textContent = k.l;
        if (i === 0) opt.selected = true;
        layoutSelect.appendChild(opt);
    });
}

// INIT
window.addEventListener('load', () => {
    if(!document.getElementById('keyboardInput')) return;

    HID = new HIDCtrl();
    AppState = HID.st;
    
    const dbg = document.getElementById('debugSection');
    if(dbg) dbg.style.display = 'block';
    
    if(typeof initTrackpad === 'function') initTrackpad();
    
    HID.init();

    E("targetOs").addEventListener("change", populateLayouts);
    populateLayouts();
});