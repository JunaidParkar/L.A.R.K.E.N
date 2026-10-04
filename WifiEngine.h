#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

class WifiEngine {
public:
	void begin();
	void update(uint32_t now);
	void toggleSetupPortal();

	bool setupPortalActive() const { return portalActive; }
	bool isConnecting() const { return connecting; }
	bool isConnected() const { return connected; }
	bool hasTime() const { return timeSynced; }
	bool hasCredentials() const { return !savedSsid.isEmpty(); }
	const String &statusMessage() const { return message; }
	const String &connectedSsid() const { return connectedSsidValue; }
	const String &botName() const { return configuredBotName; }
	const String &ownerName() const { return configuredOwnerName; }

private:
	Preferences preferences;
	WebServer server{80};
	bool portalActive = false;
	bool connecting = false;
	bool connected = false;
	bool ntpConfigured = false;
	bool timeSynced = false;
	bool routesStarted = false;
	uint32_t attemptStarted = 0;
	uint32_t lastAttempt = 0;
	int16_t timezoneOffsetHours = 0;
	String savedSsid;
	String message = "Not configured";
	String connectedSsidValue;
	String configuredBotName = "Larken";
	String configuredOwnerName;

	void startHomeWifi();
	void startSetupPortal();
	void stopSetupPortal();
	void handleRoot();
	void handleSave();
	void handlePortalStatus();
	String escapeHtml(const String &value) const;
};
