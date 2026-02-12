// ===== SD Card Handler =====
class SDCardHandler {
  constructor() {
    this.MODES = { IDLE: 0, LISTING: 1, READING: 2, WRITING: 3 };
    this.mode = this.MODES.IDLE;
    this.buffer = "";
    this.targetFile = "";

    this.writeBuffer = "";
    this.writeOffset = 0;
    this.writeChunkSize = 126; //125 
    this.writeInProgress = false;
    this.writeCallback = null;
    this.isJS = false;
  }
  
  startCollection(mode, targetFile = "", js = false) {
    this.mode = mode;
    this.buffer = "";
    this.targetFile = targetFile;
    this.isJS = js;
  }
  
  appendData(data) {
    this.buffer += data;
  }
  
  finishCollection() {
    const data = this.buffer;
    const mode = this.mode;
    const file = this.targetFile;
    
    this.mode = this.MODES.IDLE;
    this.buffer = "";
    this.targetFile = "";
    
    if (mode === this.MODES.LISTING) this.processList(data);
    else if (mode === this.MODES.READING) this.processRead(data, file);
  }
  
  processList(data) {
    Status.startInterval();
    
    if (!data || data.trim().length === 0) {
      if (E("scriptSDTable")) E("scriptSDTable").innerHTML = "<p>No files on SD card</p>";
      return;
    }

    const lines = data.split(/\n/).filter(l => {
      const t = l.trim();
      return t && !t.startsWith(">") && !t.startsWith("#");
    });
    
    let html = "<table><thead><tr><th>File (SD)</th><th>Size</th><th>Actions</th></tr></thead><tbody>";
    let count = 0;
    
    lines.forEach(line => {
      const parts = line.split(",");
      if (parts.length < 2) return;
      const name = parts[0].trim();
      const size = Utils.formatBytes(parseInt(parts[1].trim()));
      count++;
      html += `<tr><td><span class='file-icon'>📄</span> ${name}</td><td>${size}</td><td>`;
      html += `<button class='danger' onclick="rm_sd_file('${name}')">delete</button> `
      html += `<button class='primary' onclick="read_sd_file('${name}')">edit</button> `;
      html += `<button class='warn' onclick="run_sd_script('${name}')">Run</button></td></tr>`;
    });

    html += "</tbody></table>";
    if (E("scriptSDTable")) {
      E("scriptSDTable").innerHTML = count > 0 ? html : "<p>No valid files found</p>";
    }
  }
  
  processRead(data, fileName) {

    Status.startInterval();

    if (E("editor") && !this.isJS) {
      Editor.currentStorage = "SDCARD";
      Editor.updateSourceUI("SDCARD");

      E("editor").value = data;
      E("editorFile").value = fileName;

      Editor.fileOpened = true;
    } else if (E("editorJS") && this.isJS) {
      Editor.currentStorage = "SDCARD"; 
      Editor.updateSourceUI("SDCARD", this.isJS);

      E("editorJS").value = data;
      E("editorFileJS").value = fileName;

      Editor.fileOpenedJS = true;
    }
  }
  
  listFiles() {
    if(E("scriptSDTable")) E("scriptSDTable").innerHTML = "<i>Loading SD files...</i>";
    this.startCollection(this.MODES.LISTING);
    WSManager.send("sd_ls", null);
    Status.set("SD_STATUS: enumerating...");
    //Status.forceCheck = 4;
    //Status.startInterval();
  }
  
  readFile(fileName) {
    let js = false;
    if (fileName.toLowerCase().endsWith(".js")) js = true;
    Editor.updateSourceUI("SDCARD", js);
    this.startCollection(this.MODES.READING, fileName, js);
    WSManager.send(`sd_cat "${fileName}"`, null);
    Status.set("SD_STATUS: reading...");
    //Status.forceCheck = 4;
    //Status.startInterval();
  }
  
  write(fileName, content, callback = null) {
    if (this.writeInProgress) {
      console.error("Write already in progress!");
      return;
    }
    
    this.mode = this.MODES.WRITING;
    this.targetFile = fileName;
    this.writeBuffer = content;
    this.writeOffset = 0;
    this.writeInProgress = true;
    this.writeCallback = callback;
    
    console.log(`Starting SD write stream for: ${fileName}`);
    Status.set("saving to SD...");
    
    Status.set("SD_STATUS: writting")
    //Status.startInterval();
    
    WSManager.send(`sd_stream_write_begin "${fileName}"`, null);
    
    setTimeout(() => this.sendNextChunk(), 500);
  }

  sendNextChunk() {
    if (!this.writeInProgress) return;
    
    const remaining = this.writeBuffer.length - this.writeOffset;
    
    if (remaining <= 0) {
      console.log("All chunks sent, waiting for final ACK...");
      return;
    }
    
    const chunkSize = Math.min(this.writeChunkSize, remaining);
    const chunk = this.writeBuffer.substring(this.writeOffset, this.writeOffset + chunkSize);
    
    //const progress = Math.floor((this.writeOffset / this.writeBuffer.length) * 100);
    //Status.set(`saving to SD... ${progress}%`);
    
    console.log(`Sending chunk ${this.writeOffset}-${this.writeOffset + chunkSize} of ${this.writeBuffer.length}`);
    console.log(chunk);

    WSManager.sendRaw(`sd_stream_write ${chunk}`, null);
    
    this.writeOffset += chunkSize;
  }
  
  handleWriteAck(success) {
    if (!this.writeInProgress) return;
    
    if (!success) {
      console.error("Write ACK failed!");
      this.finishWrite("ERROR");
      return;
    }
    
    const remaining = this.writeBuffer.length - this.writeOffset;
    
    if (remaining > 0) {
      this.sendNextChunk();
    } else {
      console.log("All data sent, closing stream...");
      WSManager.send("sd_stop", null);
      this.finishWrite("OK");
    }
  }
  
  finishWrite(status) {
    console.log(`Write finished with status: ${status}`);
    
    this.writeInProgress = false;
    this.mode = this.MODES.IDLE;
    
    const success = status === "OK";
    
    if (success) {
      Status.set("saved to SD!");
      alert(`File "${this.targetFile}" saved successfully to SD card!`);
      setTimeout(() => this.listFiles(), 500);
    } else {
      Status.set("SD write error!");
      alert(`Error saving file to SD card: ${status}`);
      Status.startInterval();
    }
    
    this.writeBuffer = "";
    this.writeOffset = 0;
    this.targetFile = "";
    
    if (this.writeCallback) {
      this.writeCallback(success);
      this.writeCallback = null;
    }
    
    setTimeout(() => Status.stopInterval(), 1000);
  }

  runScript(fileName) {
    if (fileName.toLowerCase().endsWith(".js")) {
      alert("To run a JS file, you first have to read the file and then press the run button in the JS Interpreter.");
      return;
    }
    WSManager.send(`sd_run "${fileName}"`, null);
    Status.forceCheck = 4;
    Status.startInterval();
  }

  stopSdcard() {
    const prev_ = this.writeInProgress;

    this.writeInProgress = false;

    if (this.mode === this.MODES.IDLE)
      WSManager.send("sd_stop_run", null);
    else 
      WSManager.send("sd_stop", null);

    this.mode = this.MODES.IDLE;

    Status.forceCheck = 4;
    Status.startInterval();

    if (prev_) {
      alert("Writing to the SD card stopped");
    }
  }

  rmFile(fileName) {
    if (confirm(`Are you sure you want to delete "${fileName}" from SD Card?`)) {
      WSManager.send(`sd_rm "${fileName}"`, (msg) => {
        setTimeout(() => {
          this.listFiles();
          alert(`Deleted from SD: ${fileName}`);
        }, 700);
      });

      Status.forceCheck = 4;
      Status.startInterval();
    }
  }
}

const SDCard = new SDCardHandler();

const reload_sd_list = () => SDCard.listFiles();
const read_sd_file = (name) => SDCard.readFile(name);
const run_sd_script = (name) => SDCard.runScript(name);
const write_sd_file = (name, content) => SDCard.write(name, content);
const rm_sd_file = (name) => SDCard.rmFile(name);
const stop_sd = () => SDCard.stopSdcard();