#include "web_server.h"
#include "motion_control.h"
#include "power_measurement.h"
#include "persistence.h"
#include "positioning.h"
#include "sensors.h"
#include "test_routines.h"
#include "melodies.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SD.h>
#include <Update.h>
#include <time.h>
#include <esp_task_wdt.h> // Add at top of web_server.cpp if not present

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
}

bool connectToSavedWiFi() {
    prefs.begin("wifi_config", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();

    if (ssid.length() == 0) return false;

    WiFi.begin(ssid.c_str(), pass.c_str());
    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        timeout--;
    }
    return (WiFi.status() == WL_CONNECTED);
}

static String getSDFilesListHTML() {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) return "<p style='color:#ff5555;'>SD Card Offline.</p>";

    String html = "<ul>";
    File file = root.openNextFile();
    while (file) {
        html += "<li><strong>" + String(file.name()) + "</strong> (" + String(file.size()) + " B)</li>";
        file = root.openNextFile();
    }
    root.close();
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
        body { font-family: 'Segoe UI', sans-serif; margin: 20px; background: #121212; color: #e0e0e0; }
        h2, h3 { color: #00adb5; margin-top: 0; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 15px; }
        .card { background: #1e1e1e; padding: 18px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.4); }
        .val { font-weight: bold; color: #00ffcc; }
        .badge { background: #00adb5; color: #fff; padding: 4px 8px; border-radius: 4px; font-weight: bold; }
        .btn { padding: 10px 16px; font-size: 14px; margin: 4px; border: none; border-radius: 4px; cursor: pointer; color: #fff; font-weight: bold; }
        .btn-fw { background: #28a745; } .btn-rev { background: #17a2b8; } .btn-stop { background: #dc3545; }
        .btn-test { background: #ffc107; color: #000; } .btn-ctrl { background: #6f42c1; }
        .btn-ntp { background: #fd7e14; } .btn-ota { background: #00adb5; }
        input[type=number], input[type=text] { background: #2b2b2b; color: #fff; padding: 6px; border-radius: 4px; border: 1px solid #444; width: 100px; }
        select { background: #2b2b2b; color: #fff; padding: 6px; border-radius: 4px; border: 1px solid #444; width: 100%; margin: 4px 0; }
        input[type=file] { background: #2b2b2b; color: #fff; padding: 8px; border-radius: 4px; border: 1px solid #444; width: 100%; box-sizing: border-box; margin-bottom: 10px; }
        pre { background: #000; color: #00ff66; padding: 10px; border-radius: 5px; height: 200px; overflow-y: auto; white-space: pre-wrap; word-wrap: break-word; }
    </style>
</head>
<body>
    <h2>EHM-MENTAL PCB Control & System Telemetry</h2>
    
    <div class="grid">
        <div class="card">
            <h3>System Mode & RTC Sync</h3>
            <p>RTC PCB Time: <span id="rtc-time" class="val">--</span></p>
            <p>Power State: <span id="pwr-mode" class="badge">--</span></p>
            <p>Control Mode: <span id="ctrl-mode" class="val">--</span></p>
            <button class="btn btn-ctrl" onclick="sendCmd('/api/ctrl_mode?mode=direct')">Direct Mode</button>
            <button class="btn btn-ctrl" onclick="sendCmd('/api/ctrl_mode?mode=low_voltage')">Low Voltage Mode</button>
            <button class="btn btn-ntp" onclick="sendCmd('/api/sync_rtc')">Sync RTC via Internet</button>
        </div>

        <div class="card">
            <h3>Environment & Accelerometer</h3>
            <p>Temperature: <span id="env-temp" class="val">0.00</span> &deg;C</p>
            <p>Humidity: <span id="env-hum" class="val">0.00</span> %</p>
            <p>Pressure: <span id="env-pres" class="val">0.00</span> hPa</p>
            <p>Accel (mm/s&sup2;): X: <span id="acc-x" class="val">0.0</span> | Y: <span id="acc-y" class="val">0.0</span> | Z: <span id="acc-z" class="val">0.0</span></p>
            <p>Inclination: Pitch: <span id="pitch-deg" class="val">0</span>&deg; | Tilt: <span id="tilt-deg" class="val">0</span>&deg;</p>
        </div>

        <div class="card">
            <h3>Motor Telemetry & Power</h3>
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
            <hr style="border-color:#333;">
            <input type="number" id="new-pos" placeholder="New Pos">
            <button class="btn btn-fw" onclick="setPos()">Set Position</button><br><br>
            <input type="number" step="0.001" id="new-scale" placeholder="Counts/mm">
            <button class="btn btn-rev" onclick="setScale()">Set Scale</button><br><br>
            <input type="number" id="new-upper" placeholder="Upper Lim">
            <button class="btn btn-test" onclick="setUpper()">Set Upper Lim</button><br><br>
            <input type="number" id="new-lower" placeholder="Lower Lim">
            <button class="btn btn-test" onclick="setLower()">Set Lower Lim</button>
        </div>

        <div class="card">
            <h3>Motion Control</h3>
            <button class="btn btn-fw" onclick="sendCmd('/api/motion?dir=fw')">FORWARD</button>
            <button class="btn btn-rev" onclick="sendCmd('/api/motion?dir=rev')">REVERSE</button>
            <button class="btn btn-stop" onclick="sendCmd('/api/motion?dir=stop')">STOP</button>
            
            <h3 style="margin-top:12px;">Run to Target Position</h3>
            <input type="number" id="target-pos" placeholder="Target (mm)">
            <button class="btn btn-ctrl" onclick="runToTarget()">RUN TO TARGET</button>

            <h3 style="margin-top:12px;">Brake Testing Modes</h3>
            <button class="btn btn-test" onclick="sendCmd('/api/brake?mode=br1')">TEST BR1</button>
            <button class="btn btn-test" onclick="sendCmd('/api/brake?mode=br2')">TEST BR2</button>
            <button class="btn btn-ctrl" onclick="sendCmd('/api/brake?mode=none')">NORMAL</button>
        </div>

        <div class="card">
            <h3>Buzzer Melodies Configuration</h3>
            <label>Startup Melody:</label><br>
            <select id="sel-startup"></select><br>
            
            <label>Upper Limit Melody:</label><br>
            <select id="sel-upper"></select><br>
            
            <label>Lower Limit Melody:</label><br>
            <select id="sel-lower"></select><br><br>
            
            <button class="btn btn-fw" onclick="saveMelodies()">SAVE MELODIES</button>
            <button class="btn btn-test" onclick="previewMelody('startup')">PREVIEW STARTUP</button>
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

    <div class="grid" style="margin-top:15px;">
        <div class="card">
            <h3>Firmware OTA Update (.bin)</h3>
            <input type="file" id="ota-file" accept=".bin">
            <button class="btn btn-ota" onclick="uploadFirmware()">UPLOAD & FLASH FIRMWARE</button>
            <p id="ota-status" style="margin-top:10px; font-weight:bold;"></p>
        </div>
        <div class="card"><h3>SD Logs</h3><div id="sd-list">Loading...</div></div>
        <div class="card"><h3>Live Console Log</h3><pre id="diag-log">Loading...</pre></div>
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
        
        function runToTarget() {
            sendCmd('/api/run_to_pos?target=' + document.getElementById('target-pos').value);
        }

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

    // Feed Watchdog on every incoming chunk to prevent OTA reboot
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
    updateAllPowerMeasurements();
    
    float temp = 0, hum = 0, pres = 0;
    readWeatherSensor(temp, hum, pres);

    int16_t ax = 0, ay = 0, az = 0;
    readAccelerometer(ax, ay, az);

    DateTime now = rtc.now();

    char rtcBuf[25];
    snprintf(rtcBuf, sizeof(rtcBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    String json = "{";
    json += "\"rtc\":\"" + String(rtcBuf) + "\",";
    json += "\"pwrStr\":\"" + getPowerModeString() + "\",";
    json += "\"ctrlMode\":\"" + String(currentCtrlMode == CTRL_MODE_DIRECT ? "DIRECT" : "LOW_VOLTAGE") + "\",";
    json += "\"temp\":" + String(temp, 2) + ",";
    json += "\"hum\":" + String(hum, 2) + ",";
    json += "\"pres\":" + String(pres, 1) + ",";
    json += "\"ax_mms2\":" + String(accelX_mms2, 1) + ",";
    json += "\"ay_mms2\":" + String(accelY_mms2, 1) + ",";
    json += "\"az_mms2\":" + String(accelZ_mms2, 1) + ",";
    json += "\"pitch\":" + String(pitchDeg) + ",";
    json += "\"tilt\":" + String(tiltDeg) + ",";
    json += "\"curU\":" + String(motorCurrentU, 2) + ",";
    json += "\"curV\":" + String(motorCurrentV, 2) + ",";
    json += "\"curW\":" + String(motorCurrentW, 2) + ",";
    json += "\"vL1L2\":" + String(vL1L2_RMS, 2) + ",";
    json += "\"vL3L2\":" + String(vL3L2_RMS, 2) + ",";
    json += "\"vL1L3\":" + String(vL1L3_RMS, 2) + ",";
    json += "\"freq\":" + String(phaseFrequencyHz, 2) + ",";
    json += "\"motPwr\":" + String(motorPower, 2) + ",";
    json += "\"dcV\":" + String(inaBusVoltage, 2) + ",";
    json += "\"dcI\":" + String(inaCurrent, 2) + ",";
    json += "\"dcP\":" + String(inaPower, 2) + ",";
    json += "\"devRun\":" + String(sysStats.deviceRuntimeSec) + ",";
    json += "\"motRun\":" + String(sysStats.motorRuntimeSec) + ",";
    json += "\"br1C\":" + String(sysStats.br1Cycles) + ",";
    json += "\"br2C\":" + String(sysStats.br2Cycles) + ",";
    json += "\"encRaw\":" + String(getRawEncoderCount()) + ",";
    json += "\"encPos\":" + String(getCalculatedPosition()) + ",";
    json += "\"encScale\":" + String(sysStats.encoderScale, 4) + ",";
    json += "\"upperLim\":" + String(sysStats.upperLimit) + ",";
    json += "\"lowerLim\":" + String(sysStats.lowerLimit);
    json += "}";

    server.send(200, "application/json", json);
}

static void handleSyncRTCAPI() {
    if (WiFi.status() != WL_CONNECTED) {
        server.send(400, "text/plain", "Error: Wi-Fi STA not connected to internet.");
        return;
    }

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5000)) {
        server.send(500, "text/plain", "Error: NTP Sync Timed Out.");
        return;
    }

    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));

    appendDiagLog("[RTC] Time synced via NTP server successfully.\n");
    server.send(200, "text/plain", "RTC time synced with NTP server!");
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

static void handleSetCtrlModeAPI() {
    if (server.hasArg("mode")) {
        String mode = server.arg("mode");
        if (mode == "direct") setOperationControlMode(CTRL_MODE_DIRECT);
        else setOperationControlMode(CTRL_MODE_LOW_VOLTAGE);
        server.send(200, "text/plain", "Control mode updated.");
    } else server.send(400, "text/plain", "Missing mode arg");
}

static void handleMotionAPI() {
    if (!server.hasArg("dir")) return server.send(400, "text/plain", "Missing dir");
    String dir = server.arg("dir");
    bool ok = true;
    if (dir == "fw") ok = setMotionState(MOTION_FORWARD);
    else if (dir == "rev") ok = setMotionState(MOTION_REVERSE);
    else ok = setMotionState(MOTION_STOP);

    if (ok) server.send(200, "text/plain", "Motion updated: " + dir);
    else server.send(403, "text/plain", "Direct Control Blocked: Limits or Power Interlock!");
}

static void handleRunToPosAPI() {
    if (server.hasArg("target")) {
        int32_t t = server.arg("target").toInt();
        bool ok = runToTargetPosition(t);
        if (ok) server.send(200, "text/plain", "Running to target position: " + String(t));
        else server.send(400, "text/plain", "Run to target blocked (limits or interlock)");
    } else server.send(400, "text/plain", "Missing target arg");
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
        playMelody(id); // Send response FIRST, then execute sequence
    } else {
        server.send(400, "text/plain", "Missing id");
    }
}

static void handleSDAPI() { server.send(200, "text/html", getSDFilesListHTML()); }
static void handleDiagAPI() { server.send(200, "text/plain", diagLogBuffer); }

void initWebServer(const char* apSSID, const char* apPassword) {
    diagLogBuffer.reserve(4096);

    WiFi.mode(WIFI_AP_STA);
    if (apPassword != NULL) WiFi.softAP(apSSID, apPassword);
    else WiFi.softAP(apSSID);

    connectToSavedWiFi();

    server.on("/", handleRoot);
    server.on("/api/telemetry", handleTelemetryAPI);
    server.on("/api/sync_rtc", handleSyncRTCAPI);
    server.on("/api/set_position", handleSetPositionAPI);
    server.on("/api/set_scale", handleSetScaleAPI);
    server.on("/api/set_upper_limit", handleSetUpperLimitAPI);
    server.on("/api/set_lower_limit", handleSetLowerLimitAPI);
    server.on("/api/ctrl_mode", handleSetCtrlModeAPI);
    server.on("/api/motion", handleMotionAPI);
    server.on("/api/run_to_pos", handleRunToPosAPI);
    server.on("/api/brake", handleBrakeAPI);
    server.on("/api/get_melodies", handleGetMelodiesAPI);
    server.on("/api/set_melodies", handleSetMelodiesAPI);
    server.on("/api/preview_melody", handlePreviewMelodyAPI);
    server.on("/api/sd", handleSDAPI);
    server.on("/api/diag", handleDiagAPI);

    server.on("/update", HTTP_POST, handleUpdateResponse, handleUpdateUpload);

    server.begin();
}

void handleWebServer() { server.handleClient(); }