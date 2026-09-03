#include "web_server.h"
#include "motion_control.h"
#include "power_measurement.h"
#include "persistence.h"
#include "positioning.h"
#include "sensors.h"
#include "telemetry_helper.h"
#include "melodies.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SD.h>
#include <Update.h>
#include <time.h>
#include <esp_task_wdt.h>

static WebServer server(80);
static DNSServer dnsServer;
static String diagLogBuffer = "";
static const byte DNS_PORT = 53;

bool connectToSavedWiFi() {
    String ssid = getSSID();
    String pass = getPass();

    if (ssid.length() == 0) return false;

    WiFi.begin(ssid.c_str(), pass.c_str());
    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        esp_task_wdt_reset();
        delay(500);
        timeout--;
    }
    return (WiFi.status() == WL_CONNECTED);
}

void appendDiagLog(const String& logMsg) {
    diagLogBuffer += logMsg;
    if (diagLogBuffer.length() > 4096) {
        diagLogBuffer = diagLogBuffer.substring(diagLogBuffer.length() - 4096);
    }
}

static String getSDFilesListHTML() {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) return "<p style='color:#ef4444;'>SD Card Offline.</p>";

    String html = "<ul style='list-style:none; padding-left:0; margin:0;'>";
    File file = root.openNextFile();
    while (file) {
        String fileName = String(file.name());
        if (!fileName.startsWith("/")) fileName = "/" + fileName;
        
        html += "<li style='margin-bottom:8px; display:flex; align-items:center; justify-content:space-between; background:rgba(255,255,255,0.02); padding:8px 12px; border-radius:6px; border:1px solid #2e2e36;'>";
        html += "<span>&#128196; <strong style='color:#f8fafc;'>" + fileName + "</strong> <span style='color:#71717a; font-size:0.85em; font-family:monospace;'>(" + String(file.size()) + " B)</span></span>";
        html += "<span style='display:flex; gap:8px;'>";
        html += "<a href='/download?file=" + fileName + "&inline=1' target='_blank' style='color:#fbbf24; text-decoration:none; font-size:0.82em; font-weight:600; background:rgba(251,191,36,0.12); padding:4px 10px; border-radius:6px; border:1px solid rgba(251,191,36,0.25);'>View</a>";
        html += "<a href='/download?file=" + fileName + "' download style='color:#f59e0b; text-decoration:none; font-size:0.82em; font-weight:600; background:rgba(245,158,11,0.12); padding:4px 10px; border-radius:6px; border:1px solid rgba(245,158,11,0.25);'>Download</a>";
        html += "</span></li>";

        file = root.openNextFile();
    }
    root.close();
    html += "</ul>";
    return html;
}

static void handleDownloadFileAPI() {
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing file parameter");
        return;
    }

    String path = server.arg("file");
    if (!path.startsWith("/")) path = "/" + path;

    if (!SD.exists(path)) {
        server.send(404, "text/plain", "File Not Found");
        return;
    }

    File downloadFile = SD.open(path, FILE_READ);
    if (!downloadFile) {
        server.send(500, "text/plain", "Failed to open file");
        return;
    }

    bool isInline = (server.hasArg("inline") && server.arg("inline") == "1");
    String fileNameOnly = path.substring(path.lastIndexOf('/') + 1);

    server.setContentLength(downloadFile.size());
    server.sendHeader("Content-Type", "text/plain");

    if (!isInline) {
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + fileNameOnly + "\"");
    }

    server.sendHeader("Connection", "close");

    uint8_t buffer[512];
    while (downloadFile.available()) {
        size_t bytesRead = downloadFile.read(buffer, sizeof(buffer));
        server.client().write(buffer, bytesRead);
        esp_task_wdt_reset();
    }

    downloadFile.close();
}

static void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html><html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ELL HOIST - EHM-MENTAL Dashboard</title>
    <style>
        * { box-sizing: border-box; }
        body { 
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; 
            margin: 0; padding: 24px; 
            background: #111113; 
            color: #f4f4f5; 
            line-height: 1.5;
        }
        .header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 24px;
            padding-bottom: 16px;
            border-bottom: 1px solid #27272a;
        }
        h2 { 
            color: #ffffff; 
            margin: 0; 
            font-size: 1.5rem; 
            font-weight: 700;
            letter-spacing: -0.02em;
        }
        h2 span { color: #f59e0b; }
        h3 { 
            color: #fbbf24; 
            margin-top: 0; 
            margin-bottom: 16px; 
            font-size: 1.02rem; 
            font-weight: 700; 
            letter-spacing: 0.04em;
            text-transform: uppercase;
        }
        .grid { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); 
            gap: 16px; 
        }
        .card { 
            background: #1c1c20; 
            padding: 20px; 
            border-radius: 12px; 
            border: 1px solid #27272a; 
            box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.5); 
            transition: border-color 0.2s ease;
        }
        .card:hover { border-color: #3f3f46; }
        p { margin: 8px 0; color: #a1a1aa; font-size: 0.92rem; }
        .val { font-weight: 600; color: #fbbf24; font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; }
        .badge { 
            background: rgba(245, 158, 11, 0.12); 
            color: #f59e0b; 
            border: 1px solid rgba(245, 158, 11, 0.3);
            padding: 3px 10px; 
            border-radius: 9999px; 
            font-weight: 600; 
            font-size: 0.78rem;
            letter-spacing: 0.05em;
            text-transform: uppercase;
            display: inline-block;
        }
        .btn { 
            padding: 9px 16px; 
            font-size: 13px; 
            margin: 4px; 
            border: none; 
            border-radius: 8px; 
            cursor: pointer; 
            color: #ffffff; 
            font-weight: 600; 
            letter-spacing: 0.02em;
            transition: all 0.2s ease;
            box-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
        .btn:hover { opacity: 0.9; transform: translateY(-1px); }
        .btn:active { transform: translateY(0); }
        .btn-fw { background: linear-gradient(135deg, #16a34a, #22c55e); } 
        .btn-rev { background: linear-gradient(135deg, #d97706, #f59e0b); color: #0a0e1a; } 
        .btn-stop { background: linear-gradient(135deg, #dc2626, #ef4444); } 
        .btn-test { background: linear-gradient(135deg, #d97706, #fbbf24); color: #000000; } 
        .btn-ctrl { background: linear-gradient(135deg, #475569, #64748b); } 
        .btn-ota { background: linear-gradient(135deg, #d97706, #f59e0b); color: #0a0e1a; width: 100%; margin: 8px 0 0 0; }
        input[type=number], input[type=text] { 
            background: #121214; 
            color: #f8fafc; 
            padding: 8px 12px; 
            border-radius: 8px; 
            border: 1px solid #3f3f46; 
            width: 120px; 
            font-size: 0.9rem;
            outline: none;
            transition: border-color 0.2s;
        }
        input[type=number]:focus, input[type=text]:focus, select:focus {
            border-color: #f59e0b;
            box-shadow: 0 0 0 2px rgba(245, 158, 11, 0.2);
        }
        select { 
            background: #121214; 
            color: #f8fafc; 
            padding: 8px 12px; 
            border-radius: 8px; 
            border: 1px solid #3f3f46; 
            width: 100%; 
            margin: 6px 0 12px 0; 
            font-size: 0.9rem;
            outline: none;
        }
        input[type=file] { 
            background: #121214; 
            color: #a1a1aa; 
            padding: 10px; 
            border-radius: 8px; 
            border: 1px dashed #3f3f46; 
            width: 100%; 
            box-sizing: border-box; 
            margin-bottom: 8px; 
            font-size: 0.85rem;
        }
        pre { 
            background: #09090b; 
            color: #10b981; 
            padding: 12px; 
            border-radius: 8px; 
            border: 1px solid #27272a;
            height: 200px; 
            overflow-y: auto; 
            white-space: pre-wrap; 
            word-wrap: break-word; 
            font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
            font-size: 0.82rem;
            margin: 0;
        }
        hr { border: 0; border-top: 1px solid #27272a; margin: 16px 0; }
        label { font-size: 0.85rem; color: #a1a1aa; font-weight: 500; }
    </style>
</head>
<body>
    <div class="header">
        <h2>ELL HOIST <span>EHM-MENTAL</span></h2>
        <span class="badge">Live Connection</span>
    </div>
    
    <div class="grid">
        <div class="card">
            <h3>System Information</h3>
            <p>Device Serial: <span id="dev-sn" class="val">--</span></p>
            <p>Hardware ID: <span id="hw-id" class="val">--</span></p>
            <p>RTC PCB Time: <span id="rtc-time" class="val">--</span></p>
            <p>Power State: <span id="pwr-mode" class="badge">--</span></p>
            <p>Control Mode: <span id="ctrl-mode" class="val">--</span></p>
        </div>

        <div class="card">
            <h3>Environment</h3>
            <p>Temperature: <span id="env-temp" class="val">0.00</span> &deg;C</p>
            <p>Humidity: <span id="env-hum" class="val">0.00</span> %</p>
            <p>Pressure: <span id="env-pres" class="val">0.00</span> hPa</p>
            <p>Accel (mm/s&sup2;): X: <span id="acc-x" class="val">0.0</span> | Y: <span id="acc-y" class="val">0.0</span> | Z: <span id="acc-z" class="val">0.0</span></p>
            <p>Inclination: Pitch: <span id="pitch-deg" class="val">0</span>&deg; | Tilt: <span id="tilt-deg" class="val">0</span>&deg;</p>
        </div>

        <div class="card">
            <h3>Power Telemetry</h3>
            <p>Phase U Current: <span id="cur-u" class="val">0.00</span> A</p>
            <p>Phase V Current: <span id="cur-v" class="val">0.00</span> A</p>
            <p>Phase W Current: <span id="cur-w" class="val">0.00</span> A</p>
            <p>V (L1 - L2): <span id="v-l1l2" class="val">0.00</span> V RMS</p>
            <p>V (L3 - L2): <span id="v-l3l2" class="val">0.00</span> V RMS</p>
            <p>V (L1 - L3): <span id="v-l1l3" class="val">0.00</span> V RMS</p>
            <p>Frequency: <span id="ac-freq" class="val">0.00</span> Hz</p>
            <p>Calculated Motor Power: <span id="mot-pwr" class="val">0.00</span> W</p>
        </div>

        <div class="card">
            <h3>Encoder Positioning</h3>
            <p>Raw Count: <span id="enc-raw" class="val">0</span></p>
            <p>Position: <span id="enc-pos" class="val">0</span> mm</p>
            <p>Scale Factor: <span id="enc-scale" class="val">1.0</span> counts/mm</p>
            <p>Upper Limit: <span id="lim-upper" class="val">0</span> mm</p>
            <p>Lower Limit: <span id="lim-lower" class="val">0</span> mm</p>
            <hr>
            <div style="display:flex; gap:8px; margin-bottom:8px;">
                <input type="number" id="new-pos" placeholder="New Pos">
                <button class="btn btn-fw" onclick="setPos()">Set Position</button>
            </div>
            <div style="display:flex; gap:8px; margin-bottom:8px;">
                <input type="number" step="0.001" id="new-scale" placeholder="Counts/mm">
                <button class="btn btn-rev" onclick="setScale()">Set Scale</button>
            </div>
            <div style="display:flex; gap:8px; margin-bottom:8px;">
                <input type="number" id="new-upper" placeholder="Upper Lim">
                <button class="btn btn-test" onclick="setUpper()">Set Upper</button>
            </div>
            <div style="display:flex; gap:8px;">
                <input type="number" id="new-lower" placeholder="Lower Lim">
                <button class="btn btn-test" onclick="setLower()">Set Lower</button>
            </div>
        </div>

        <div class="card">
            <h3>Brake Testing Modes</h3>
            <button class="btn btn-test" onclick="sendCmd('/api/brake?mode=br1')">TEST BR1</button>
            <button class="btn btn-test" onclick="sendCmd('/api/brake?mode=br2')">TEST BR2</button>
            <button class="btn btn-ctrl" onclick="sendCmd('/api/brake?mode=none')">NORMAL</button>
        </div>

        <div class="card">
            <h3>Buzzer Melodies</h3>
            <label>Startup Melody</label>
            <select id="sel-startup"></select>
            
            <label>Upper Limit Melody</label>
            <select id="sel-upper"></select>
            
            <label>Lower Limit Melody</label>
            <select id="sel-lower"></select>
            
            <div style="margin-top:8px;">
                <button class="btn btn-fw" onclick="saveMelodies()">SAVE MELODIES</button>
                <button class="btn btn-test" onclick="previewMelody('startup')">PREVIEW</button>
            </div>
        </div>

        <div class="card">
            <h3>DC Power Telemetry (INA226)</h3>
            <p>Loop Bus Voltage: <span id="dc-v" class="val">0.00</span> V</p>
            <p>Loop Current: <span id="dc-i" class="val">0.00</span> mA</p>
            <p>Loop Power: <span id="dc-p" class="val">0.00</span> W</p>
        </div>

        <div class="card">
            <h3>Counters & Runtimes</h3>
            <p>Device Runtime: <span id="dev-runtime" class="val">0</span> s</p>
            <p>Motor Runtime: <span id="mot-runtime" class="val">0</span> s</p>
            <p>BR1 Cycle Count: <span id="br1-cycles" class="val">0</span></p>
            <p>BR2 Cycle Count: <span id="br2-cycles" class="val">0</span></p>
        </div>
    </div>

    <div class="grid" style="margin-top:16px;">
        <div class="card">
            <h3>Firmware OTA Update (.bin)</h3>
            <input type="file" id="ota-file" accept=".bin">
            <button class="btn btn-ota" onclick="uploadFirmware()">UPLOAD & FLASH FIRMWARE</button>
            <p id="ota-status" style="margin-top:10px; font-weight:bold;"></p>
        </div>
        <div class="card">
            <h3>SD Logs</h3>
            <div id="sd-list">Loading...</div>
        </div>
        <div class="card">
            <h3>Live Console Log</h3>
            <pre id="diag-log">Loading...</pre>
        </div>
    </div>

    <script>
        let pollInterval = null;

        function sendCmd(url) { 
            fetch(url).then(r => r.text()).then(t => console.log(t)); 
        }
        function setPos() { sendCmd('/api/set_position?pos=' + document.getElementById('new-pos').value); }
        function setScale() { sendCmd('/api/set_scale?scale=' + document.getElementById('new-scale').value); }
        function setUpper() { sendCmd('/api/set_upper_limit?val=' + document.getElementById('new-upper').value); }
        function setLower() { sendCmd('/api/set_lower_limit?val=' + document.getElementById('new-lower').value); }

        function loadMelodies() {
            fetch('/api/get_melodies').then(r => r.json()).then(d => {
                let opts = "";
                d.available.forEach(m => { opts += `<option value="${m.id}">${m.name}</option>`; });
                
                const sStart = document.getElementById('sel-startup');
                const sUpper = document.getElementById('sel-upper');
                const sLower = document.getElementById('sel-lower');
                
                sStart.innerHTML = opts; sUpper.innerHTML = opts; sLower.innerHTML = opts;
                
                sStart.value = d.selected.startup;
                sUpper.value = d.selected.upper;
                sLower.value = d.selected.lower;
            });
        }

        function saveMelodies() {
            const start = document.getElementById('sel-startup').value;
            const upper = document.getElementById('sel-upper').value;
            const lower = document.getElementById('sel-lower').value;
            sendCmd(`/api/set_melodies?start=${start}&upper=${upper}&lower=${lower}`);
        }

        function previewMelody(type) {
            let id = 0;
            if (type === 'startup') id = document.getElementById('sel-startup').value;
            sendCmd(`/api/preview_melody?id=${id}`);
        }

        function poll() {
            fetch('/api/telemetry').then(r => r.json()).then(d => {
                document.getElementById('dev-sn').textContent = d.devSerial || '--';
                document.getElementById('hw-id').textContent = d.hwId || '--';
                document.getElementById('rtc-time').textContent = d.rtc;
                document.getElementById('pwr-mode').textContent = d.pwrStr;
                document.getElementById('ctrl-mode').textContent = d.ctrlMode;
                document.getElementById('env-temp').textContent = d.temp.toFixed(2);
                document.getElementById('env-hum').textContent = d.hum.toFixed(2);
                document.getElementById('env-pres').textContent = d.pres.toFixed(1);
                document.getElementById('acc-x').textContent = d.ax_mms2.toFixed(1);
                document.getElementById('acc-y').textContent = d.ay_mms2.toFixed(1);
                document.getElementById('acc-z').textContent = d.az_mms2.toFixed(1);
                document.getElementById('pitch-deg').textContent = d.pitch;
                document.getElementById('tilt-deg').textContent = d.tilt;
                document.getElementById('cur-u').textContent = d.curU.toFixed(2);
                document.getElementById('cur-v').textContent = d.curV.toFixed(2);
                document.getElementById('cur-w').textContent = d.curW.toFixed(2);
                document.getElementById('v-l1l2').textContent = d.vL1L2.toFixed(2);
                document.getElementById('v-l3l2').textContent = d.vL3L2.toFixed(2);
                document.getElementById('v-l1l3').textContent = d.vL1L3.toFixed(2);
                document.getElementById('ac-freq').textContent = d.freq.toFixed(2);
                document.getElementById('mot-pwr').textContent = d.motPwr.toFixed(2);
                document.getElementById('dc-v').textContent = d.dcV.toFixed(2);
                document.getElementById('dc-i').textContent = d.dcI.toFixed(2);
                document.getElementById('dc-p').textContent = d.dcP.toFixed(2);
                document.getElementById('dev-runtime').textContent = d.devRun;
                document.getElementById('mot-runtime').textContent = d.motRun;
                document.getElementById('br1-cycles').textContent = d.br1C;
                document.getElementById('br2-cycles').textContent = d.br2C;
                document.getElementById('enc-raw').textContent = d.encRaw;
                document.getElementById('enc-pos').textContent = d.encPos;
                document.getElementById('enc-scale').textContent = d.encScale;
                document.getElementById('lim-upper').textContent = d.upperLim;
                document.getElementById('lim-lower').textContent = d.lowerLim;
            });

            fetch('/api/diag').then(r => r.text()).then(t => {
                const logElem = document.getElementById('diag-log');
                logElem.textContent = t;
                logElem.scrollTop = logElem.scrollHeight;
            });
        }

        function uploadFirmware() {
            const fileInput = document.getElementById('ota-file');
            const status = document.getElementById('ota-status');
            if (!fileInput.files.length) {
                alert('Please select a .bin firmware file first!');
                return;
            }

            if (pollInterval) clearInterval(pollInterval);

            const file = fileInput.files[0];
            const formData = new FormData();
            formData.append('update', file);

            status.style.color = '#f59e0b';
            status.textContent = 'Uploading & Flashing... Do not disconnect power!';

            fetch('/update', { method: 'POST', body: formData })
                .then(r => r.text())
                .then(t => {
                    status.style.color = '#22c55e';
                    status.textContent = t + ' Rebooting system...';
                    setTimeout(() => { location.reload(); }, 8000);
                })
                .catch(e => {
                    status.style.color = '#ef4444';
                    status.textContent = 'OTA Flash Failed!';
                });
        }

        window.onload = () => {
            fetch('/api/sd').then(r => r.text()).then(h => document.getElementById('sd-list').innerHTML = h);
            loadMelodies();
            pollInterval = setInterval(poll, 500);
        };
    </script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

static void handleUpdateResponse() {
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
        server.send(500, "text/plain", "OTA FAIL: " + String(Update.getError()));
    } else {
        server.send(200, "text/plain", "OTA SUCCESS!");
        delay(1000);
        ESP.restart();
    }
}

static void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();

    esp_task_wdt_reset();

    if (upload.status == UPLOAD_FILE_START) {
        appendDiagLog("[OTA] Firmware update started: " + upload.filename + "\n");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        yield();
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            appendDiagLog("[OTA] Flashing successful! Size: " + String(upload.totalSize) + " bytes\n");
        } else {
            Update.printError(Serial);
        }
    }
}

static void handleTelemetryAPI() {
    server.send(200, "application/json", buildTelemetryJSON());
}

static void handleSetPositionAPI() {
    if (server.hasArg("pos")) {
        setEncoderPosition(server.arg("pos").toInt());
        server.send(200, "text/plain", "Encoder position updated.");
    } else server.send(400, "text/plain", "Missing pos arg");
}

static void handleSetScaleAPI() {
    if (server.hasArg("scale")) {
        setEncoderScale(server.arg("scale").toFloat());
        server.send(200, "text/plain", "Encoder scale updated.");
    } else server.send(400, "text/plain", "Missing scale arg");
}

static void handleSetUpperLimitAPI() {
    if (server.hasArg("val")) {
        setUpperLimit(server.arg("val").toInt());
        server.send(200, "text/plain", "Upper limit updated.");
    } else server.send(400, "text/plain", "Missing val arg");
}

static void handleSetLowerLimitAPI() {
    if (server.hasArg("val")) {
        setLowerLimit(server.arg("val").toInt());
        server.send(200, "text/plain", "Lower limit updated.");
    } else server.send(400, "text/plain", "Missing val arg");
}

static void handleBrakeAPI() {
    if (!server.hasArg("mode")) return server.send(400, "text/plain", "Missing mode");
    String mode = server.arg("mode");
    if (mode == "br1") setBrakeTestMode(BRAKE_TEST_BR1);
    else if (mode == "br2") setBrakeTestMode(BRAKE_TEST_BR2);
    else setBrakeTestMode(BRAKE_TEST_NONE);
    server.send(200, "text/plain", "Brake mode updated.");
}

static void handleGetMelodiesAPI() {
    String json = "{\"selected\":{";
    json += "\"startup\":" + String(sysStats.startupMelody) + ",";
    json += "\"upper\":" + String(sysStats.upperLimitMelody) + ",";
    json += "\"lower\":" + String(sysStats.lowerLimitMelody) + "},\"available\":[";

    for (int i = 0; i < MELODY_COUNT; i++) {
        json += "{\"id\":" + String(AVAILABLE_MELODIES[i].id) + ",\"name\":\"" + String(AVAILABLE_MELODIES[i].name) + "\"}";
        if (i < MELODY_COUNT - 1) json += ",";
    }
    json += "]}";

    server.send(200, "application/json", json);
}

static void handleSetMelodiesAPI() {
    if (server.hasArg("start")) sysStats.startupMelody = server.arg("start").toInt();
    if (server.hasArg("upper")) sysStats.upperLimitMelody = server.arg("upper").toInt();
    if (server.hasArg("lower")) sysStats.lowerLimitMelody = server.arg("lower").toInt();
    
    saveStatsToEEPROM();
    server.send(200, "text/plain", "Melodies updated successfully.");
}

static void handlePreviewMelodyAPI() {
    if (server.hasArg("id")) {
        MelodyID id = (MelodyID)server.arg("id").toInt();
        server.send(200, "text/plain", "Playing melody preview.");
        playMelody(id);
    } else {
        server.send(400, "text/plain", "Missing id");
    }
}

static void handleSDAPI() { server.send(200, "text/html", getSDFilesListHTML()); }
static void handleDiagAPI() { server.send(200, "text/plain", diagLogBuffer); }

void initWebServer(const char* apSSID, const char* apPassword) {
    diagLogBuffer.reserve(4096);

    WiFi.mode(WIFI_STA);
    bool connected = connectToSavedWiFi();

    if (!connected) {
        WiFi.mode(WIFI_AP_STA);
        if (apPassword != NULL) WiFi.softAP(apSSID, apPassword);
        else WiFi.softAP(apSSID);

        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        appendDiagLog("[WIFI] STA connection failed. SoftAP Captive Portal started at " + WiFi.softAPIP().toString() + "\n");
    } else {
        appendDiagLog("[WIFI] Connected to local network. IP: " + WiFi.localIP().toString() + "\n");
    }

    server.on("/", handleRoot);
    server.on("/download", handleDownloadFileAPI);
    server.on("/api/telemetry", handleTelemetryAPI);
    server.on("/api/set_position", handleSetPositionAPI);
    server.on("/api/set_scale", handleSetScaleAPI);
    server.on("/api/set_upper_limit", handleSetUpperLimitAPI);
    server.on("/api/set_lower_limit", handleSetLowerLimitAPI);
    server.on("/api/brake", handleBrakeAPI);
    server.on("/api/get_melodies", handleGetMelodiesAPI);
    server.on("/api/set_melodies", handleSetMelodiesAPI);
    server.on("/api/preview_melody", handlePreviewMelodyAPI);
    server.on("/api/sd", handleSDAPI);
    server.on("/api/diag", handleDiagAPI);

    server.on("/update", HTTP_POST, handleUpdateResponse, handleUpdateUpload);

    server.on("/hotspot-detect.html", handleRoot);
    server.on("/generate_204", handleRoot);
    server.on("/gen_204", handleRoot);
    server.on("/connecttest.txt", handleRoot);
    server.on("/redirect", handleRoot);

    server.onNotFound([]() {
        handleRoot();
    });

    server.begin();
}

void handleWebServer() {
    dnsServer.processNextRequest();
    server.handleClient(); 
}