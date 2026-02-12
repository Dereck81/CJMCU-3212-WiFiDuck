/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck

   Modified and adapted by:
    - Dereck81
 */
const Utils = {
  E: (id) => document.getElementById(id),
  log: (msg) => console.log(msg),
  log_ws: (msg) => console.log("[WS] " + msg),
  
  downloadTxt(fileName, fileContent) {
    const el = document.createElement('a');
    el.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(fileContent));
    el.setAttribute('download', fileName);
    el.style.display = 'none';
    document.body.appendChild(el);
    el.click();
    document.body.removeChild(el);
  },
  
  fixFileName(fileName) {
    if (fileName.length > 0) {
      if (fileName[0] !== '/') fileName = '/' + fileName;
      fileName = fileName.replace(/ /g, '-');
    }
    return fileName;
  },
  
  formatBytes(bytes) {
    if (isNaN(bytes) || bytes === 0) return '0 B';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
  },

  isValidName(n, storage = "SPIFFS") {
    let name = n.trim();
    
    if (storage === "SPIFFS") {
      name = name.replace(/[^a-zA-Z0-9._\-/]/g, '');
      if (name.length > 0 && !name.startsWith('/')) name = '/' + name;
    } else {
      name = name.replace(/[^a-zA-Z0-9._-]/g, '');
    }

    if (storage === "SPIFFS") {
      if (name.length < 1 || name.length > 31) {
        alert(`Name too ${name.length < 1 ? 'short' : 'long'} (Max 31 chars)`);
        return false;
      }
      return name;
    }

    if (name.length < 3 || name.length > 31) {
      alert(`Name too ${name.length < 3 ? 'short' : 'long'} (Max 31 chars)`);
      return false;
    }

    if (!name.includes('.')) {
      alert("File extension required (.txt, .ds or .js)");
      return false;
    }

    return name;
  }
};

const E = Utils.E;
const log = Utils.log;
const log_ws = Utils.log_ws;
const download_txt = Utils.downloadTxt.bind(Utils);
const fixFileName = Utils.fixFileName.bind(Utils);

// ===== Status Manager =====
class StatusManager {
  constructor() {
    this.current = "";
    this.interval = null;
    this.forceCheck = 0;
  }
  
  set(mode) {
    this.current = mode;
    const statusEl = E("status");
    
    if (mode === "connected") statusEl.style.backgroundColor = "#3c5";
    else if (mode === "disconnected") statusEl.style.backgroundColor = "#d33";
    else if (mode.includes("problem") || mode.includes("error")) statusEl.style.backgroundColor = "#ffc107";
    else if (mode.includes("SD_STATUS: ")) statusEl.style.backgroundColor = "#7207ff";
    else statusEl.style.backgroundColor = "#0ae";
    
    statusEl.innerHTML = mode;
    this.updateUI();
  }
  
  updateUI() {
    const isUiBlocked  = this.current.includes("running") || 
                      this.current.includes("saving") || 
                      this.current.includes("SD_STATUS:") ||
                      this.current.includes("connecting") || 
                      this.current.includes("disconnected");
    
    const ids = ["saveSpiffs", "saveSpiffsJS", "saveSdcard", "saveSdcardJS", "editorDelete","editorRun","editorReload","editorAutorun",
                 "scriptsReload","scriptCreate","scriptsSDReload","format", "runJS",
                 "changeLayout","keyboardInput","enableKeyboard","showKeys","textBlock", "wifiSettings", "updateOTA",
                 "sendTextBlockID", "liveTyping", "shortCuts", "virtualMouse", "specialKeys", "virtualKeys", "navigationKeys"];
    
    ids.forEach(id => {
      const el = E(id);
      if (!el) return;
      if (el.tagName === "DIV") {
        el.style.pointerEvents = isUiBlocked  ? "none" : "auto";
        el.style.opacity = isUiBlocked  ? "0.5" : "1";
      } else {
        el.disabled = isUiBlocked ;
      }
    });

    document.querySelectorAll("#scriptTable button, #shortCuts button, #scriptSDTable button")
      .forEach(btn => btn.disabled = isUiBlocked );
  }
  
  check() {
    WSManager.updateStatus();
    if (this.forceCheck > 0) {
      this.forceCheck--;
      return;
    }
    if (!this.current.includes("running") && 
        !this.current.includes("saving") && 
        !this.current.includes("SD_STATUS:")) {
      this.stopInterval();
    }
  }
  
  startInterval() {
    if (this.interval) return;
    WSManager.updateStatus();
    this.interval = setInterval(() => this.check(), 1000); 
  }
  
  stopInterval() {
    if (!this.interval) return;
    clearInterval(this.interval);
    this.interval = null;
  }
}

const Status = new StatusManager();

const status = (mode) => Status.set(mode);
const start_status_interval = () => Status.startInterval();
const stop_status_interval = () => Status.stopInterval();
const update_ui_state = () => Status.updateUI();

let current_status = "";
let status_interval = null;
let status_force_check = 0;

Object.defineProperty(window, 'status_force_check', {
  get: () => Status.forceCheck,
  set: (val) => Status.forceCheck = val
});
Object.defineProperty(window, 'current_status', {
  get: () => Status.current
});

// ===== WebSocket Manager =====
class WebSocketManager {
  constructor() {
    this.ws = null;
    this.callback = log_ws;
    this.msgQueue = [];
    this.cts = false;
    this.queueInterval = null;
  }
  
  init() {
    Status.set("connecting...");
    this.ws = new WebSocket("ws://192.168.4.1/ws");
    window.ws = this.ws;
    
    this.ws.onopen = (e) => this.onOpen(e);
    this.ws.onclose = (e) => this.onClose(e);
    this.ws.onmessage = (e) => this.onMessage(e);
    this.ws.onerror = (e) => this.onError(e);
    
    this.cts = true;
    if (this.queueInterval) clearInterval(this.queueInterval);
    this.queueInterval = setInterval(() => this.processQueue(), 3);
  }
  
  onOpen(event) {
    log_ws("connected");
    window.ws = this.ws;
    Status.set("connected");
    this.send("close", log_ws, true);
    this.send("version", (str) => E("version").innerHTML = str);
    if (window.ws_connected) ws_connected();
  }
  
  onClose(event) {
    log_ws("disconnected");
    Status.set("disconnected");
    if (typeof SDCard !== 'undefined') {
      SDCard.writeInProgress = false;
      SDCard.mode = 0;
    }
  }
  
  onMessage(event) {
    const msg = event.data;
    log_ws(msg);

    if (msg.startsWith("SD_LS:")) {
      SDCard.appendData(msg.substring(6) + "\n");
      this.cts = true;
      return;
    }
    
    if (msg.startsWith("SD_CAT:")) {
      SDCard.appendData(msg.substring(7));
      this.cts = true;
      return;
    }

    if (msg.startsWith("SD_ACK:")) {
      const ackStatus = msg.substring(7).trim();
      if (ackStatus === "OK") {
        console.log("SD_ACK:OK");
        SDCard.handleWriteAck(true);
      } else {
        console.log("SD_ACK:ERROR");
        SDCard.handleWriteAck(false);
      }
      this.cts = true;
      return;
    }

    if (msg.startsWith("SD_END:")) {
      
      if (SDCard.mode !== SDCard.MODES.IDLE) {
        console.log("SD operation finished. Processing...");
        SDCard.finishCollection();
      }
      
      this.cts = true;
      return;
    }

    if (msg.startsWith("KEY_ACK:")) {
      const status = msg.substring(8).trim();
      window.dispatchEvent(new CustomEvent("key_ack", { detail: status }));
      return;
    }

    if (this.callback && msg.length > 0) {
      if (this.callback !== log_ws) this.callback(msg);
    }
    this.cts = true;
  }
  
  onError(event) {
    log_ws("error");
    Status.set("error");
    console.error(event);
  }
  
  processQueue() {
    if (this.cts && this.msgQueue.length > 0) {
      const item = this.msgQueue.shift();
      this.ws.send(item.message);
      this.callback = item.callback;
      console.debug("# " + item.message);
      this.cts = false;
    }
  }
  
  send(message, callback, force = false) {
    if (!message.endsWith('\n')) message += '\n';
    this.sendRaw(message, callback, force);
  }
  
  sendRaw(message, callback, force = false) {
    const obj = { message, callback };
    force ? this.msgQueue.unshift(obj) : this.msgQueue.push(obj);
  }
  
  updateStatus() {
    this.send("status", (mode) => Status.set(mode));
  }
}

const WSManager = new WebSocketManager();

const ws_init = () => WSManager.init();
const ws_send = (msg, cb, force) => WSManager.send(msg, cb, force);
const ws_send_raw = (msg, cb, force) => WSManager.sendRaw(msg, cb, force);
const ws_update_status = () => WSManager.updateStatus();