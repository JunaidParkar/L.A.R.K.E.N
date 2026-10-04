#include "WifiEngine.h"
#include <time.h>

namespace {
const char SETUP_PAGE[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>Larken setup</title>
<style>body{font:16px system-ui;max-width:460px;margin:40px auto;padding:0 18px;background:#101820;color:#f5f7f8}h1{color:#78e0cb}label{display:block;margin:18px 0 6px}input{box-sizing:border-box;width:100%;padding:12px;border:1px solid #667680;border-radius:6px;background:#202d35;color:white}button{margin-top:20px;padding:12px 18px;border:0;border-radius:6px;background:#78e0cb;color:#10201e;font-weight:700}</style></head>
<body><h1>Larken setup</h1><p>Enter your details and home Wi-Fi. Credentials are saved on this device.</p>
<form method="post" action="/save"><label>Bot name</label><input name="name" maxlength="24" value="Larken"><label>Your name</label><input name="owner" maxlength="24"><label>Time zone (UTC offset in hours)</label><input name="timezone" type="number" min="-12" max="14" step="1" value="0"><label>Wi-Fi network name</label><input name="ssid" maxlength="32" required><label>Wi-Fi password</label><input name="password" type="password" maxlength="63"><button type="submit">Save and connect</button></form>
<p><a style="color:#78e0cb" href="/status">Connection status</a></p></body></html>
)rawliteral";
const wifi_power_t LARKEN_WIFI_TX_POWER = WIFI_POWER_8_5dBm;
}

void WifiEngine::begin() {
	preferences.begin("larken", false);
	savedSsid = preferences.getString("ssid", "");
	configuredBotName = preferences.getString("name", "Larken");
	configuredOwnerName = preferences.getString("owner", "");
	timezoneOffsetHours = preferences.getInt("timezone", 0);

	server.on("/", HTTP_GET, [this]() { handleRoot(); });
	server.on("/save", HTTP_POST, [this]() { handleSave(); });
	server.on("/status", HTTP_GET, [this]() { handlePortalStatus(); });
	server.onNotFound([this]() {
		server.sendHeader("Location", "/", true);
		server.send(302, "text/plain", "");
	});

	if (savedSsid.isEmpty()) {
		Serial.println("Wi-Fi: no saved network; starting first-boot setup AP");
		startSetupPortal();
	} else {
		startHomeWifi();
	}
}

void WifiEngine::update(uint32_t now) {
	if (portalActive) server.handleClient();

	if (WiFi.status() == WL_CONNECTED) {
		if (!connected || connectedSsidValue != WiFi.SSID()) {
			connectedSsidValue = WiFi.SSID();
			message = "Connected to " + connectedSsidValue;
		}
		connected = true;
		connecting = false;
		if (!ntpConfigured) {
			configTime(timezoneOffsetHours * 3600, 0, "pool.ntp.org", "time.nist.gov");
			ntpConfigured = true;
		}
		struct tm timeInfo;
		timeSynced = getLocalTime(&timeInfo, 5) && timeInfo.tm_year >= 124;
		return;
	}

	connected = false;
	connectedSsidValue = "";
	timeSynced = false;
	if (connecting && now - attemptStarted > 20000) {
		connecting = false;
		message = "Could not connect; check Wi-Fi details";
	}
	if (!connecting && !savedSsid.isEmpty() && now - lastAttempt > 60000) startHomeWifi();
}

void WifiEngine::toggleSetupPortal() {
	if (portalActive) stopSetupPortal();
	else startSetupPortal();
}

void WifiEngine::startHomeWifi() {
	if (savedSsid.isEmpty()) {
		connected = false;
		connecting = false;
		message = "Not configured";
		return;
	}

	WiFi.mode(portalActive ? WIFI_AP_STA : WIFI_STA);
	WiFi.setTxPower(LARKEN_WIFI_TX_POWER);
	WiFi.begin(savedSsid.c_str(), preferences.getString("password", "").c_str());
	connecting = true;
	connected = false;
	message = "Connecting to home Wi-Fi";
	attemptStarted = millis();
	lastAttempt = attemptStarted;
}

void WifiEngine::startSetupPortal() {
	if (portalActive) return;

	const wifi_mode_t requestedMode = connected || connecting ? WIFI_AP_STA : WIFI_AP;
	const bool modeReady = WiFi.mode(requestedMode);
	const bool powerReady = WiFi.setTxPower(LARKEN_WIFI_TX_POWER);
	const bool started = WiFi.softAP("L.A.R.K.E.N Setup", nullptr, 1, false, 4);
	portalActive = started;

	if (started) {
		const String ip = WiFi.softAPIP().toString();
		Serial.printf("Wi-Fi AP started: SSID=L.A.R.K.E.N Setup, IP=%s, mode=%s, tx-power=%s\n",
			ip.c_str(), modeReady ? (requestedMode == WIFI_AP ? "AP" : "AP+STA") : "mode warning",
			powerReady ? "8.5 dBm" : "default (set failed)");
		if (!routesStarted) {
			server.begin();
			routesStarted = true;
		}
		message = "Setup AP active";
	} else {
		message = "Could not start setup network";
		Serial.printf("Wi-Fi AP FAILED: mode=%s, status=%d, heap=%u\n",
			modeReady ? "OK" : "failed", WiFi.status(), static_cast<unsigned int>(ESP.getFreeHeap()));
	}
}

void WifiEngine::stopSetupPortal() {
	if (!portalActive) return;
	server.stop();
	routesStarted = false;
	WiFi.softAPdisconnect(true);
	portalActive = false;
	WiFi.mode(WIFI_STA);
}

void WifiEngine::handleRoot() {
	String page = FPSTR(SETUP_PAGE);
	page.replace("value=\"Larken\"", "value=\"" + escapeHtml(configuredBotName) + "\"");
	page.replace("name=\"owner\" maxlength=\"24\"", "name=\"owner\" maxlength=\"24\" value=\"" + escapeHtml(configuredOwnerName) + "\"");
	page.replace("value=\"0\"", "value=\"" + String(timezoneOffsetHours) + "\"");
	server.send(200, "text/html", page);
}

void WifiEngine::handleSave() {
	String ssid = server.arg("ssid");
	String password = server.arg("password");
	String name = server.arg("name");
	String owner = server.arg("owner");
	const String timezone = server.arg("timezone");
	ssid.trim();
	name.trim();
	owner.trim();

	if (ssid.isEmpty()) {
		server.send(400, "text/plain", "Wi-Fi network name cannot be empty.");
		return;
	}
	const int timezoneHours = timezone.toInt();
	if (timezone.isEmpty() || timezoneHours < -12 || timezoneHours > 14) {
		server.send(400, "text/plain", "Time zone must be a whole-hour UTC offset from -12 to +14.");
		return;
	}
	if (name.isEmpty()) name = "Larken";

	preferences.putString("ssid", ssid);
	preferences.putString("password", password);
	preferences.putString("name", name);
	preferences.putString("owner", owner);
	preferences.putInt("timezone", timezoneHours);
	savedSsid = ssid;
	configuredBotName = name;
	configuredOwnerName = owner;
	timezoneOffsetHours = timezoneHours;
	ntpConfigured = false;
	message = "Connecting to " + savedSsid;
	startHomeWifi();
	server.send(200, "text/html", "<!doctype html><meta name='viewport' content='width=device-width'><body style='font:16px system-ui;background:#101820;color:white;padding:24px'><h2>Saved</h2><p>Larken is connecting to your network. Keep this page open and check <a style='color:#78e0cb' href='/status'>connection status</a>.</p></body>");
}

void WifiEngine::handlePortalStatus() {
	const String status = connected ? "Connected to " + connectedSsidValue : message;
	String page = "<!doctype html><meta http-equiv='refresh' content='5'><meta name='viewport' content='width=device-width'><body style='font:16px system-ui;background:#101820;color:white;padding:24px'><h2>Larken status</h2><p>";
	page += escapeHtml(status);
	page += F("</p><p><a style='color:#78e0cb' href='/'>Back to setup</a></p></body>");
	server.send(200, "text/html", page);
}

String WifiEngine::escapeHtml(const String &value) const {
	String escaped;
	escaped.reserve(value.length() + 8);
	for (size_t i = 0; i < value.length(); ++i) {
		const char c = value[i];
		if (c == '&') escaped += F("&amp;");
		else if (c == '<') escaped += F("&lt;");
		else if (c == '>') escaped += F("&gt;");
		else if (c == '"') escaped += F("&quot;");
		else escaped += c;
	}
	return escaped;
}
