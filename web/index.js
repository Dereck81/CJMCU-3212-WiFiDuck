/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck

   Modified and adapted by:
    - Dereck81
 */
// ===== File Manager (SPIFFS) =====
class FileManager {
  constructor() {
    this.fileList = "";
  }
  
  updateList() {
    ws_send("mem", (msg) => {
      const lines = msg.split(/\n/);
      if(lines.length === 1) {
        console.error("Malformed response:", msg);
        return;
      }

      const byte = lines[0].split(" ")[0];
      const used = lines[1].split(" ")[0];
      const free = lines[2].split(" ")[0];
      const percent = Math.floor(byte / 100);
      const freepercent = Math.floor(free / percent);

      E("freeMemory").innerHTML = `${used} byte used (${freepercent}% free)`;
      this.fileList = "";

      ws_send("ls", (csv) => {
        this.fileList += csv;
        this.renderTable();
      });
    });
  }
  
  renderTable() {
    const lines = this.fileList.split(/\n/);
    let html = "<thead><tr><th>File</th><th>Size</th><th>Actions</th></tr></thead><tbody>";

    lines.forEach((line, i) => {
      const data = line.split(" ");
      const fileName = data[0];
      const fileSize = Utils.formatBytes(parseInt(data[1]));

      if (fileName.length > 0) {
        if (i === 0 && !Editor.fileOpened) this.read(fileName);
        html += `<tr><td><span class='file-icon'>📄</span> ${fileName}</td><td>${fileSize}</td><td>`;
        html += `<button class="danger" onclick="delete_('${fileName}')">delete</button> `
        html += `<button class="primary" onclick="read('${fileName}')">edit</button> `;
        html += `<button class="primary" onclick="autorun('${fileName}')">set autorun</button> `;
        html += `<button class="warn" onclick="run('${fileName}')">run</button></td></tr>`;
      }
    });
    html += "</tbody>";
    E("scriptTable").innerHTML = html;
  }
  
  read(fileName) {
    Editor.currentStorage = "SPIFFS";
    
    let js = false;
    
    if (fileName.toLowerCase().endsWith(".js")) js = true;
    
    Editor.updateSourceUI("SPIFFS", js);
    
    this.stop(fileName);
    stopInterpreter();
    
    fileName = fixFileName(fileName);
    
    if (js) {
      E("editorFileJS").value = fileName;
      E("editorJS").value = "";
    } else {
      E("editorFile").value = fileName;
      E("editor").value = "";
    }
    
    ws_send(`stream "${fileName}"`, log_ws);
    this.readStream(js);
    
    if (js) Editor.fileOpenedJS = true;
    else Editor.fileOpened = true;
  }
  
  readStream(js = false) {
    ws_send("read", (content) => {
      if (content !== "> END") {
        if (js) E("editorJS").value += content;
        else E("editor").value += content;
        this.readStream(js);
        status("reading...");
      } else {
        ws_send("close", log_ws);
        ws_update_status();
      }
    });
  }
  
  create(fileName) {
    const validatedName = Utils.isValidName(fileName, "SPIFFS");
    if (!validatedName) return;

    this.stop(fileName);
    if (this.fileList.includes(fileName + " ")) {
      this.read(fileName);
    } else {
      E("editorFile").value = fileName;
      E("editor").value = "";
      ws_send(`create "${fileName}"`, log_ws);
      E("newFile").value = "/";
      this.updateList();
    }
  }
  
  write(fileName, content) {
    this.stop(fileName);
    fileName = fixFileName(fileName);
    ws_send('remove "/temporary_script"', log_ws);
    ws_send('create "/temporary_script"', log_ws);
    ws_send('stream "/temporary_script"', log_ws);

    const pktsize = 1024;
    for (let i = 0; i < Math.ceil(content.length / pktsize); i++) {
      const begin = i * pktsize;
      const end = Math.min(begin + pktsize, content.length);
      ws_send_raw(content.substring(begin, end), () => status("saving..."));
    }

    ws_send("close", log_ws);
    ws_send(`remove "${fileName}"`, log_ws);
    ws_send(`rename "/temporary_script" "${fileName}"`, log_ws);
    ws_update_status();
  }
  
  remove(fileName) {
    this.stop(fileName);
    stopInterpreter();
    ws_send(`remove "${fixFileName(fileName)}"`, log_ws);
    this.updateList();
    Editor.unsavedChanged = true;
    Editor.unsavedChangedJS = true;
  }

  delete(fileName) {
    if (confirm(`Delete ${fileName}?`)) remove(fileName);
  }
  
  run(fileName) {
    if (fileName.toLowerCase().endsWith(".js")) {
      alert("To run a JS file, you first have to read the file and then press the run button in the JS Interpreter.");
      return;
    }

    ws_send(`run "${fixFileName(fileName)}"`, log_ws);
    start_status_interval();
  }
  
  stop(fileName) {
    ws_send(`stop "${fixFileName(fileName)}"`, log_ws, true);
  }
  
  stopAll() {
    ws_send("stop", log_ws, true);
  }
  
  format() {
    if (confirm("Format SPIFFS? This will delete all scripts!")) {
      ws_send("format", log_ws);
      alert("Formatting will take a minute.\nYou have to reconnect afterwards.");
    }
  }

  autorun(fileName) {
    if (confirm("Run this script automatically on startup?\nYou can disable it in the settings."))
      ws_send(`set autorun "${fixFileName(fileName)}"`, log_ws);
  }
}

const Files = new FileManager();

const update_file_list = () => Files.updateList();
const read = (name) => Files.read(name);
const create = (name) => Files.create(name);
const remove = (name) => Files.remove(name);
const delete_ = (name) => Files.delete(name);
const run = (name) => Files.run(name);
const stop = (name) => Files.stop(name);
const stopAll = () => Files.stopAll();
const format = () => Files.format();
const autorun = (name) => Files.autorun(name);

let file_list = "";

Object.defineProperty(window, 'file_list', {
  get: () => Files.fileList
});

// ===== Editor =====
class EditorManager {
  constructor() {
    this.unsavedChanged = false;
    this.fileOpened = false;
    this.unsavedChangedJS = false;
    this.fileOpenedJS = false;
    this.currentStorage = "SPIFFS";
    this.currentStorageJS = "SPIFFS";
  }

  updateSourceUI(storage, js = false) {
    if (js) this.currentStorageJS = storage;
    else this.currentStorage = storage;
    
    const fileInput = js ? E("editorFileJS") : E("editorFile");
    const badge = js ? E("storageBadgeJS") : E("storageBadge");

    if (badge) {
        badge.innerText = storage;
        badge.className = storage === "SPIFFS" ? "badge-spiffs" : "badge-sdcard";
    }
    
    if (storage === "SDCARD" && fileInput) {
        fileInput.value = fileInput.value.replace(/\//g, '');
    }
  }
  
  getFilename(js = false) {
    if (js) return E("editorFileJS").value; 
    return E("editorFile").value; 
  }

  setFilename(name, js = false) {
    if (js) E("editorFileJS").value = name; 
    else E("editorFile").value = name; 
  }

  getContent(js = false) {
    let content = js ? E("editorJS").value : E("editor").value;
    if (!content.endsWith("\n")) content += "\n";
    return content;
  }

  append(str, js = false) {
    if (js) E("editorJS").value += str; 
    else E("editor").value += str; 
  }
  
  save(target = null, js = false) {

    const dest = target || (js ? this.currentStorageJS : this.currentStorage);
    const rawName = this.getFilename(js);

    const validatedName = Utils.isValidName(rawName, dest);
    
    if (!validatedName) return;

    if (js && !validatedName.toLowerCase().endsWith(".js")) {
      alert("Only files with the .js extension are allowed.");
      return;
    }

    this.setFilename(validatedName, js);
    
    if (dest === "SDCARD") write_sd_file(this.getFilename(js), this.getContent(js));
    else Files.write(this.getFilename(js), this.getContent(js));

    if (js) {
      this.unsavedChangedJS = false;
      E("editorinfoJS").innerHTML = `saved (${dest})`;
    } else {
      this.unsavedChanged = false;
      E("editorinfo").innerHTML = `saved (${dest})`;
    }
  }

  runCurrent() {
    const fileName = this.getFilename();
    if (!fileName) return;

    if (this.unsavedChanged) {
      alert("You must save the file changes before running");
      return;
    }

    if (this.currentStorage === "SDCARD") {
      log("Running from SD: " + fileName);
      run_sd_script(fileName);
    } else {
      log("Running from SPIFFS: " + fileName);
      run(fileName);
    }
  }
  
  markUnsaved(js = false) {
    if (js) {
      this.unsavedChangedJS = true;
      E("editorinfoJS").innerHTML = "unsaved changes";
    }else {
      this.unsavedChanged = true;
      E("editorinfo").innerHTML = "unsaved changes";
    }
  }
}

const Editor = new EditorManager();

const get_editor_filename = (js = false) => Editor.getFilename(js);

const set_editor_filename = (name, js = false) => Editor.setFilename(name, js);

const get_editor_content = (js = false) => Editor.getContent(js);

const append = (str, js = false) => Editor.append(str, js);

const save = (target, js = false) => Editor.save(target, js);

const run_current = (target) => Editor.runCurrent(target);

let unsaved_changed = false;
let unsaved_changed_js = false;

let file_opened = false;
let file_opened_js = false;

Object.defineProperty(window, 'unsaved_changed', {
  get: () => Editor.unsavedChanged,
  set: (val) => Editor.unsavedChanged = val
});
Object.defineProperty(window, 'file_opened', {
  get: () => Editor.fileOpened,
  set: (val) => Editor.fileOpened = val
});

Object.defineProperty(window, 'unsaved_changed_js', {
  get: () => Editor.unsavedChangedJS,
  set: (val) => Editor.unsavedChangedJS = val
});
Object.defineProperty(window, 'file_opened_js', {
  get: () => Editor.fileOpenedJS,
  set: (val) => Editor.fileOpenedJS = val
});

function ws_connected() {
  Files.updateList();
  ws_update_status();
  start_status_interval();
}

window.addEventListener("load", function() {
  E("reconnect").onclick = ws_init;
  E("scriptsReload").onclick = update_file_list;
  E("format").onclick = format;
  E("stop").onclick = stopAll;
  
  E("saveSpiffs").onclick = () => save("SPIFFS");
  E("saveSdcard").onclick = () => save("SDCARD");

  E("editorDownload").onclick = () => download_txt(get_editor_filename(), get_editor_content());
  E("editorStop").onclick = () => stop(get_editor_filename());
  
  E("stopSdcard").onclick = () => stop_sd();

  E("editorRun").onclick = () => run_current();

  E("editor").onkeyup = () => Editor.markUnsaved();

  if (E("scriptsSDReload")) 
    E("scriptsSDReload").onclick = reload_sd_list;

  // JS

  E("saveSpiffsJS").onclick = () => save("SPIFFS", true);
  E("saveSdcardJS").onclick = () => save("SDCARD", true);

  E("editorDownloadJS").onclick = () => download_txt(get_editor_filename(true), get_editor_content(true));
  E("editorJS").onkeyup = () => Editor.markUnsaved(true);

  E("stopSdcardJS").onclick = () => stop_sd();
 
  document.querySelectorAll("code").forEach(code => {
    code.addEventListener("click", function() {
      append(this.innerHTML + " \n");
    });
  });

  ws_init();
}, false);