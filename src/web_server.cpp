#include "web_server.h"
#include "motion_control.h"
#include "power_measurement.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SD.h>
#include <Update.h> // Required for OTA Firmware Updates

static WebServer server(80);
static Preferences prefs;
static String diagLogBuffer = "";

void appendDiagLog(const String& logMsg) {
    diagLogBuffer += logMsg;
    if (diagLogBuffer.length() > 4096) {
        diagLogBuffer = diagLogBuffer.substring(diagLogBuffer.length() - 4096);
    }
}

void saveWiFiCredentials(const String& ssid, const String& password) {
    prefs.begin("wifi_config", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
    appendDiagLog("[NVS] New Wi-Fi credentials saved for SSID: " + ssid + "\n");
}

void clearWiFiCredentials() {
    prefs.begin("wifi_config", false);
    prefs.clear();
    prefs.end();
    appendDiagLog("[NVS] Saved Wi-Fi credentials erased.\n");
}

bool connectToSavedWiFi() {
    prefs.begin("wifi_config", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();

    if (ssid.length() == 0) {
        appendDiagLog("[WIFI] No saved STA credentials in NVS memory.\n");
        return false;
    }

    appendDiagLog("[WIFI] Connecting to saved network: " + ssid + "...\n");
    WiFi.begin(ssid.c_str(), pass.c_str());

    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        appendDiagLog("[WIFI] SUCCESS! Joined network: " + ssid + "\n");
        appendDiagLog("[WIFI] Station DHCP IP Address: " + WiFi.localIP().toString() + "\n");
        return true;
    } else {
        appendDiagLog("[WIFI] FAILED to connect to saved network.\n");
        return false;
    }
}

static String getSDFilesListHTML() {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        return "<p style='color:#ff5555;'>SD Card not mounted or directory invalid.</p>";
    }

    String html = "<ul>";
    File file = root.openNextFile();
    bool filesFound = false;

    while (file) {
        filesFound = true;
        html += "<li><strong>" + String(file.name()) + "</strong> (" + String(file.size()) + " bytes)</li>";
        file = root.openNextFile();
    }
    root.close();

    if (!filesFound) html += "<li><em>No files found in root.</em></li>";
    html += "</ul>";
    return html;
}

static void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html><html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>EHM-MENTAL Dashboard</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 20px; background: #121212; color: #e0e0e0; }
        h2, h3 { color: #00adb5; margin-top: 0; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 15px; }
        .card { background: #1e1e1e; padding: 18px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.4); }
        .val { font-weight: bold; color: #00ffcc; }
        .badge-ok { background: #28a745; color: #fff; padding: 4px 8px; border-radius: 4px; font-weight: bold; }
        .badge-err { background: #dc3545; color: #fff; padding: 4px 8px; border-radius: 4px; font-weight: bold; }
        .btn { padding: 10px 16px; font-size: 14px; margin: 4px; border: none; border-radius: 4px; cursor: pointer; color: #fff; font-weight: bold; }
        .btn-fw { background: #28a745; }
        .btn-rev { background: #17a2b8; }
        .btn-stop { background: #dc3545; }
        .btn-test { background: #ffc107; color: #000; }
        .btn-off { background: #6c757d; }
        .btn-ota { background: #00adb5; }
        pre { background: #000; color: #00ff66; padding: 10px; border-radius: 5px; overflow-x: auto; max-height: 200px; }
        input[type=file] { background: #2b2b2b; color: #fff; padding: 8px; border-radius: 4px; border: 1px solid #444; width: 100%; box-sizing: border-box; margin-bottom: 10px; }
    </style>
</head>
<body>
    <h2>EHM-MENTAL PCB Control & Telemetry</h2>
    
    <div class="grid">
        <div class="card">
            <h3>3-Phase AC Diagnostics</h3>
            <p>Phase Status: <span id="p-status" class="badge-err">CHECKING...</span></p>
            <p>Sequence Order: <span id="p-seq" class="val">--</span></p>
            <p>Line Frequency: <span id="p-freq" class="val">0.00</span> Hz</p>
            <p>V (L1 - L2): <span id="v-l1l2" class="val">0.00</span> V RMS</p>
            <p>V (L3 - L2): <span id="v-l3l2" class="val">0.00</span> V RMS</p>
            <p>V (L1 - L3): <span id="v-l1l3" class="val">0.00</span> V RMS</p>
        </div>

        <div class="card">
            <h3>Motor Phase Currents (TMCS1101)</h3>
            <p>Phase U (GPIO 5): <span id="c-u" class="val">0.00</span> A</p>
            <p>Phase V (GPIO 4): <span id="c-v" class="val">0.00</span> A</p>
            <p>Phase W (GPIO 6): <span id="c-w" class="val">0.00</span> A</p>
            <hr style="border-color:#333;">
            <h3>DC Loop Power (INA226)</h3>
            <p>Bus Voltage: <span id="dc-v" class="val">0.00</span> V</p>
            <p>Current: <span id="dc-i" class="val">0.00</span> mA</p>
            <p>Power: <span id="dc-p" class="val">0.00</span> W</p>
        </div>

        <div class="card">
            <h3>Motion Control</h3>
            <button class="btn btn-fw" onclick="sendCmd('/api/motion?dir=fw')">FORWARD</button>
            <button class="btn btn-rev" onclick="sendCmd('/api/motion?dir=rev')">REVERSE</button>
            <button class="btn btn-stop" onclick="sendCmd('/api/motion?dir=stop')">STOP</button>
            
            <h3 style="margin-top:15px;">Brake Testing Modes</h3>
            <button class="btn btn-test" onclick="sendCmd('/api/brake?mode=br1')">TEST BR1 (Release BR2)</button>
            <button class="btn btn-test" onclick="sendCmd('/api/brake?mode=br2')">TEST BR2 (Release BR1)</button>
            <button class="btn btn-off" onclick="sendCmd('/api/brake?mode=none')">NORMAL BRAKES</button>
        </div>
    </div>

    <div class="grid" style="margin-top:15px;">
        <div class="card">
            <h3>Firmware OTA Update (.bin)</h3>
            <input type="file" id="ota-file" accept=".bin">
            <button class="btn btn-ota" onclick="uploadFirmware()">UPLOAD & FLASH FIRMWARE</button>
            <p id="ota-status" style="margin-top:10px; font-weight:bold;"></p>
        </div>
        <div class="card">
            <h3>SD Card Files</h3>
            <div id="sd-list">Loading...</div>
        </div>
        <div class="card">
            <h3>Boot Diagnostics Console</h3>
            <pre id="diag-log">Loading...</pre>
        </div>
    </div>

    <script>
        function sendCmd(url) {
            fetch(url).then(r => r.text()).then(t => alert(t));
        }
        function pollTelemetry() {
            fetch('/api/telemetry').then(r => r.json()).then(d => {
                document.getElementById('p-status').innerHTML = d.phasesOK ? 
                    '<span class="badge-ok">ALL PHASES PRESENT</span>' : '<span class="badge-err">PHASE MISSING / ERROR</span>';
                document.getElementById('p-seq').textContent = d.phaseSeq;
                document.getElementById('p-freq').textContent = d.freq.toFixed(2);
                document.getElementById('v-l1l2').textContent = d.vL1L2.toFixed(2);
                document.getElementById('v-l3l2').textContent = d.vL3L2.toFixed(2);
                document.getElementById('v-l1l3').textContent = d.vL1L3.toFixed(2);
                document.getElementById('c-u').textContent = d.curU.toFixed(2);
                document.getElementById('c-v').textContent = d.curV.toFixed(2);
                document.getElementById('c-w').textContent = d.curW.toFixed(2);
                document.getElementById('dc-v').textContent = d.dcV.toFixed(2);
                document.getElementById('dc-i').textContent = d.dcI.toFixed(2);
                document.getElementById('dc-p').textContent = d.dcP.toFixed(2);
            });
        }
        function uploadFirmware() {
            const fileInput = document.getElementById('ota-file');
            const status = document.getElementById('ota-status');
            if (!fileInput.files.length) {
                alert('Please select a .bin firmware file first!');
                return;
            }
            const file = fileInput.files[0];
            const formData = new FormData();
            formData.append('update', file);

            status.style.color = '#ffc107';
            status.textContent = 'Uploading & Flashing... Do not disconnect power!';

            fetch('/update', { method: 'POST', body: formData })
                .then(r => r.text())
                .then(t => {
                    status.style.color = '#28a745';
                    status.textContent = t + ' Rebooting system...';
                    setTimeout(() => { location.reload(); }, 8000);
                })
                .catch(e => {
                    status.style.color = '#dc3545';
                    status.textContent = 'OTA Flash Failed!';
                });
        }
        function loadStaticData() {
            fetch('/api/sd').then(r => r.text()).then(h => document.getElementById('sd-list').innerHTML = h);
            fetch('/api/diag').then(r => r.text()).then(t => document.getElementById('diag-log').textContent = t);
        }
        window.onload = () => {
            loadStaticData();
            setInterval(pollTelemetry, 300);
        };
    </script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

// --- OTA STREAM & COMPLETION HANDLERS ---
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
    if (upload.status == UPLOAD_FILE_START) {
        appendDiagLog("[OTA] Firmware update started: " + upload.filename + "\n");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            appendDiagLog("[OTA] Flashing successful! Total size: " + String(upload.totalSize) + " bytes\n");
        } else {
            Update.printError(Serial);
        }
    }
}

static void handleTelemetryAPI() {
    updateAllPowerMeasurements();
    
    String json = "{";
    json += "\"phasesOK\":" + String(allPhasesPresent ? "true" : "false") + ",";
    json += "\"phaseSeq\":\"" + phaseSequenceStatus + "\",";
    json += "\"freq\":" + String(phaseFrequencyHz, 2) + ",";
    json += "\"vL1L2\":" + String(vL1L2_RMS, 2) + ",";
    json += "\"vL3L2\":" + String(vL3L2_RMS, 2) + ",";
    json += "\"vL1L3\":" + String(vL1L3_RMS, 2) + ",";
    json += "\"curU\":" + String(motorCurrentU, 2) + ",";
    json += "\"curV\":" + String(motorCurrentV, 2) + ",";
    json += "\"curW\":" + String(motorCurrentW, 2) + ",";
    json += "\"dcV\":" + String(inaBusVoltage, 2) + ",";
    json += "\"dcI\":" + String(inaCurrent, 2) + ",";
    json += "\"dcP\":" + String(inaPower, 2);
    json += "}";
    
    server.send(200, "application/json", json);
}

static void handleMotionAPI() {
    if (!server.hasArg("dir")) return server.send(400, "text/plain", "Missing dir");
    String dir = server.arg("dir");
    if (dir == "fw") setMotionState(MOTION_FORWARD);
    else if (dir == "rev") setMotionState(MOTION_REVERSE);
    else setMotionState(MOTION_STOP);
    server.send(200, "text/plain", "Motion: " + dir);
}

static void handleBrakeAPI() {
    if (!server.hasArg("mode")) return server.send(400, "text/plain", "Missing mode");
    String mode = server.arg("mode");
    if (mode == "br1") setBrakeTestMode(BRAKE_TEST_BR1);
    else if (mode == "br2") setBrakeTestMode(BRAKE_TEST_BR2);
    else setBrakeTestMode(BRAKE_TEST_NONE);
    server.send(200, "text/plain", "Brake Mode Updated (500ms safety delay applied)");
}

static void handleSDAPI() { server.send(200, "text/html", getSDFilesListHTML()); }
static void handleDiagAPI() { server.send(200, "text/plain", diagLogBuffer); }

void initWebServer(const char* apSSID, const char* apPassword) {
    WiFi.mode(WIFI_AP_STA);

    if (apPassword != NULL) WiFi.softAP(apSSID, apPassword);
    else WiFi.softAP(apSSID);

    appendDiagLog("[WIFI] Fallback AP Started: " + String(apSSID) + "\n");
    appendDiagLog("[WIFI] Access Point IP: " + WiFi.softAPIP().toString() + "\n");

    connectToSavedWiFi();

    server.on("/", handleRoot);
    server.on("/api/telemetry", handleTelemetryAPI);
    server.on("/api/motion", handleMotionAPI);
    server.on("/api/brake", handleBrakeAPI);
    server.on("/api/sd", handleSDAPI);
    server.on("/api/diag", handleDiagAPI);

    // Register OTA File Handler
    server.on("/update", HTTP_POST, handleUpdateResponse, handleUpdateUpload);

    server.begin();
}

void handleWebServer() {
    server.handleClient();
}