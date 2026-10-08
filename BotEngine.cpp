#include "BotEngine.h"
#include <time.h>
#include <math.h>
#include <new>

namespace {
const uint16_t C_BLACK = 0x0000;
const uint16_t C_PANEL = 0x0841;
const uint16_t C_PANEL_EDGE = 0x2128;
const uint16_t C_WHITE = 0xFFFF;
const uint16_t C_CYAN = 0x07FF;
const uint16_t C_MINT = 0x7FEA;
const uint16_t C_TEAL = 0x04F2;
const uint16_t C_PINK = 0xF9B6;
const uint16_t C_AMBER = 0xFDC0;
const uint16_t C_RED = 0xF800;
const uint16_t C_IRIS = 0x2D7F;
const uint16_t C_PUPIL = 0x0823;
const uint16_t C_MUTED = 0x7BEF;
const uint16_t C_TRACK = 0x3186;
const char *GAME_NAMES[] = {"STAR CATCH", "BUBBLE POP", "TINY MAZE", "MOON HOP", "COLOR DOTS"};

String valueOr(const String *value, const char *fallback = "") {
	return value ? *value : String(fallback);
}

String shortValue(const String &value, size_t maxLength) {
	if (value.length() <= maxLength) return value;
	return value.substring(0, maxLength - 1) + "~";
}

uint16_t expressionColor(BotExpression expression) {
	if (expression == BotExpression::Angry) return C_RED;
	if (expression == BotExpression::Love) return C_PINK;
	if (expression == BotExpression::Hungry || expression == BotExpression::Eating) return C_AMBER;
	return C_MINT;
}
}

BotEngine::~BotEngine() {
	delete faceBuffer;
}

bool BotEngine::begin(Adafruit_SPITFT &target) {
	display = &target;
	if (!allocateFaceBuffer()) {
		Serial.println("BOT ENGINE: framebuffer allocation failed; face animation disabled");
		return false;
	}
	Serial.printf("BOT ENGINE: RGB565 face buffer %ux%u (%lu bytes)\n",
		faceBuffer->width(), faceBuffer->height(), static_cast<unsigned long>(framebufferBytes()));
	return true;
}

uint32_t BotEngine::framebufferBytes() const {
	if (!faceBuffer) return 0;
	return static_cast<uint32_t>(faceBuffer->width()) * faceBuffer->height() * sizeof(uint16_t);
}

void BotEngine::prepareForDeepSleep() {
	if (!display) return;
	display->sendCommand(0x28);
	display->sendCommand(0x10);
}

bool BotEngine::allocateFaceBuffer() {
	if (!display) return false;
	const int16_t availableWidth = display->width() - 24;
	const int16_t availableHeight = display->height() - 100;
	if (availableWidth < 120 || availableHeight < 72) return false;

	const uint8_t scalePercent[] = {100, 88, 72, 64};
	for (uint8_t scale : scalePercent) {
		const uint16_t width = static_cast<uint16_t>(min<int16_t>(256, availableWidth) * scale / 100);
		const uint16_t height = static_cast<uint16_t>(min<int16_t>(120, availableHeight) * scale / 100);
		if (width < 120 || height < 72) continue;
		GFXcanvas16 *candidate = new (std::nothrow) GFXcanvas16(width, height);
		if (candidate && candidate->getBuffer()) {
			faceBuffer = candidate;
			faceX = (display->width() - width) / 2;
			faceY = (display->height() - height) / 2 - 3;
			return true;
		}
		delete candidate;
	}
	return false;
}

void BotEngine::render(const BotVisualState &state, uint32_t now) {
	if (!display) return;
	if (!screenInitialized || activeScreen != state.screen) {
		activeScreen = state.screen;
		screenInitialized = true;
		lastInfoUpdate = 0;
		drawScreenBase(state);
	}

	switch (state.screen) {
	case LarkenScreen::Companion:
		drawCompanion(state, now);
		break;
	case LarkenScreen::Clock:
		drawClock(state, now);
		break;
	case LarkenScreen::Status:
		drawStatus(state, now);
		break;
	case LarkenScreen::Games:
		drawGame(state, now);
		break;
	case LarkenScreen::Setup:
		drawSetup(state, now);
		break;
	case LarkenScreen::Developer:
		drawDeveloper(state, now);
		break;
	case LarkenScreen::PowerMenu:
	case LarkenScreen::ResetConfirm:
		break;
	}

	if (now - fpsWindowStarted >= 1000) {
		fps = framesInWindow;
		framesInWindow = 0;
		fpsWindowStarted = now;
	}
}

void BotEngine::drawScreenBase(const BotVisualState &state) {
	display->fillScreen(C_BLACK);
	lastFood = -1;
	lastWater = -1;
	lastEnergy = -1;
	previousGameX = -1;
	previousGameY = -1;
	lastGameScore = 0xFFFF;

	switch (state.screen) {
	case LarkenScreen::Companion: drawCompanionBase(state); break;
	case LarkenScreen::Clock: drawClockBase(); break;
	case LarkenScreen::Status: drawStatusBase(); break;
	case LarkenScreen::Games: drawGamesBase(state); break;
	case LarkenScreen::Setup: drawSetupBase(); break;
	case LarkenScreen::Developer: drawDeveloperBase(); break;
	case LarkenScreen::PowerMenu: drawPowerMenuBase(); break;
	case LarkenScreen::ResetConfirm: drawResetConfirmBase(); break;
	}
}

void BotEngine::drawCompanionBase(const BotVisualState &state) {
	display->setTextSize(1);
	display->setTextColor(C_MINT, C_BLACK);
	display->setCursor(12, 9);
	display->print(valueOr(state.botName, "Larken"));
	display->setTextColor(C_MUTED, C_BLACK);
	display->setCursor(display->width() - 72, 9);
	display->print(shortValue(valueOr(state.ownerName, "FRIEND"), 8));

	const int16_t panelHeight = display->height() - 78;
	display->drawRoundRect(10, 32, display->width() - 20, panelHeight, 22, C_PANEL_EDGE);
	const int16_t barX = 12;
	const int16_t barWidth = display->width() - 26;
	const int16_t barY = display->height() - 29;
	display->setTextColor(C_WHITE, C_BLACK);
	display->setCursor(barX, barY);
	display->print("FOOD");
	display->setCursor(barX, barY + 11);
	display->print("WATER");
	display->drawRoundRect(barX + 52, barY, barWidth - 52, 8, 3, C_TRACK);
	display->drawRoundRect(barX + 52, barY + 11, barWidth - 52, 8, 3, C_TRACK);
}

void BotEngine::drawClockBase() {
	drawCentered("LARKEN CLOCK", 14, 2, C_MINT);
	display->drawFastHLine(18, 43, display->width() - 36, C_PANEL_EDGE);
	drawCentered("T1 x2: enter setup to adjust time", display->height() - 22, 1, C_MUTED);
}

void BotEngine::drawStatusBase() {
	drawCentered("SYSTEM STATUS", 10, 2, C_MINT);
	const char *labels[] = {"Wi-Fi", "Network", "Setup AP", "Portal", "Time sync", "Food", "Water", "Energy"};
	for (uint8_t i = 0; i < 8; ++i) {
		display->setTextSize(1);
		display->setTextColor(C_MUTED, C_BLACK);
		display->setCursor(14, 42 + i * 19);
		display->print(labels[i]);
	}
	display->drawFastHLine(14, 201, display->width() - 28, C_PANEL_EDGE);
	drawCentered("T1 interact  T2 next", display->height() - 18, 1, C_MUTED);
}

void BotEngine::drawGamesBase(const BotVisualState &state) {
	drawCentered("LARKEN ARCADE", 10, 2, C_MINT);
	const uint8_t index = state.gameIndex % (sizeof(GAME_NAMES) / sizeof(GAME_NAMES[0]));
	drawCentered(GAME_NAMES[index], 42, 2, C_CYAN);
	const int16_t centerX = display->width() / 2;
	const int16_t centerY = display->height() / 2;
	const int16_t radius = min<int16_t>(display->width() / 5, display->height() / 4);
	display->drawCircle(centerX, centerY, radius, C_PANEL_EDGE);
	display->drawCircle(centerX, centerY, radius - 7, C_CYAN);
	drawCentered("T1 tap to play", display->height() - 42, 1, C_WHITE);
}

void BotEngine::drawSetupBase() {
	drawCentered("LARKEN SETUP", 14, 2, C_MINT);
	drawCentered("Connect to L.A.R.K.E.N Setup", 56, 1, C_WHITE);
	drawCentered("Open http://192.168.4.1", 82, 1, C_CYAN);
	display->drawFastHLine(20, 117, display->width() - 40, C_PANEL_EDGE);
	drawCentered("T1 double tap or T2 hold to exit", display->height() - 24, 1, C_MUTED);
}

void BotEngine::drawDeveloperBase() {
	drawCentered("ENGINEER HUD", 8, 2, C_PINK);
	display->drawFastHLine(16, 35, display->width() - 32, C_PANEL_EDGE);
	const char *labels[] = {"Uptime", "Frame rate", "Heap", "Face buffer", "Touch T1 / T2", "Wi-Fi", "Access point", "Needs F/W/E"};
	for (uint8_t i = 0; i < 8; ++i) {
		display->setTextSize(1);
		display->setTextColor(C_MUTED, C_BLACK);
		display->setCursor(14, 45 + i * 19);
		display->print(labels[i]);
	}
}

void BotEngine::drawPowerMenuBase() {
	drawCentered("POWER MENU", 28, 2, C_MINT);
	drawCentered("Single click: power off", 82, 1, C_WHITE);
	drawCentered("Double click: reboot", 108, 1, C_WHITE);
	drawCentered("Triple click: reset options", 134, 1, C_AMBER);
	drawCentered("Hold 3 seconds to return", display->height() - 28, 1, C_MUTED);
}

void BotEngine::drawResetConfirmBase() {
	drawCentered("RESET USER SETTINGS?", 30, 2, C_AMBER);
	drawCentered("Single click: cancel", 88, 1, C_WHITE);
	drawCentered("Double click: factory reset", 116, 1, C_RED);
	drawCentered("Firmware and chip stay intact", 154, 1, C_MUTED);
}

void BotEngine::drawCompanion(const BotVisualState &state, uint32_t now) {
	if (lastFood != state.hunger) {
		drawNeedBar(display->height() - 29, state.hunger, C_AMBER);
		lastFood = state.hunger;
	}
	if (lastWater != state.thirst) {
		drawNeedBar(display->height() - 18, state.thirst, C_CYAN);
		lastWater = state.thirst;
	}
	if (lastEnergy != state.energy) {
		lastEnergy = state.energy;
	}
	if (!faceBuffer || now - lastFrameAt < 33) return;

	drawFaceFrame(state.expression, now);
	display->drawRGBBitmap(faceX, faceY, faceBuffer->getBuffer(), faceBuffer->width(), faceBuffer->height());
	lastFrameAt = now;
	++framesInWindow;
}

void BotEngine::drawFaceFrame(BotExpression expression, uint32_t now) {
	GFXcanvas16 &canvas = *faceBuffer;
	canvas.fillScreen(C_BLACK);
	const int16_t centerX = canvas.width() / 2;
	const int16_t centerY = canvas.height() * 45 / 100;
	const int16_t eyeWidth = min<int16_t>(68, canvas.width() / 3);
	const int16_t eyeHeight = canvas.height() * 58 / 100;
	const int16_t eyeOffset = canvas.width() * 19 / 100;
	const float phase = now / 1000.0f;

	canvas.drawFastHLine(canvas.width() / 2 - 9, canvas.height() - 13, 18, 0x18E3);
	drawEye(centerX - eyeOffset, centerY, eyeWidth, eyeHeight, expression, phase, true);
	drawEye(centerX + eyeOffset, centerY, eyeWidth, eyeHeight, expression, phase, false);
	drawMouth(centerX, canvas.height() * 78 / 100, canvas.width() / 7, expression, phase);
	drawHands(expression, centerX, canvas.height() - 20, phase);
}

void BotEngine::drawEye(int16_t cx, int16_t cy, int16_t eyeWidth, int16_t eyeHeight, BotExpression expression, float phase, bool leftEye) {
	GFXcanvas16 &canvas = *faceBuffer;
	const int16_t radius = eyeWidth / 3;
	const uint16_t browColor = expression == BotExpression::Angry ? C_RED : C_MUTED;

	if (expression == BotExpression::Love) {
		const int16_t heartSize = min<int16_t>(13, eyeWidth / 4);
		canvas.fillCircle(cx - heartSize / 2, cy - 2, heartSize / 2, C_PINK);
		canvas.fillCircle(cx + heartSize / 2, cy - 2, heartSize / 2, C_PINK);
		canvas.fillTriangle(cx - heartSize, cy, cx + heartSize, cy, cx, cy + heartSize + 4, C_PINK);
		return;
	}

	if (expression == BotExpression::Sleepy || expression == BotExpression::Happy) {
		const int16_t tilt = leftEye ? -3 : 3;
		drawQuadratic(cx - eyeWidth / 2, cy + 3, cx, cy + tilt, cx + eyeWidth / 2, cy + 3, expression == BotExpression::Happy ? C_WHITE : C_MINT, 3);
		return;
	}

	float openAmount = 1.0f;
	const uint16_t blinkPhase = static_cast<uint16_t>(static_cast<uint32_t>(phase * 1000.0f) % 4800);
	if (blinkPhase < 150) {
		const float t = blinkPhase / 150.0f;
		openAmount = 1.0f - 0.94f * sinf(t * 3.14159f);
	}
	if (expression == BotExpression::Bored) openAmount *= 0.62f;
	if (expression == BotExpression::Angry) openAmount *= 0.72f;
	if (expression == BotExpression::Thirsty) openAmount *= 0.88f;
	const int16_t visibleHeight = max<int16_t>(3, static_cast<int16_t>(eyeHeight * openAmount));
	const int16_t eyeTop = cy - visibleHeight / 2;

	canvas.fillRoundRect(cx - eyeWidth / 2 - 3, eyeTop - 3, eyeWidth + 6, visibleHeight + 6, radius + 4, C_TEAL);
	canvas.fillRoundRect(cx - eyeWidth / 2, eyeTop, eyeWidth, visibleHeight, radius, C_WHITE);

	if (visibleHeight > eyeHeight / 3) {
		const int16_t gazeX = static_cast<int16_t>(sinf(phase * 0.7f) * 3.0f);
		const int16_t gazeY = static_cast<int16_t>(sinf(phase * 0.43f + (leftEye ? 0.0f : 0.4f)) * 2.0f);
		const int16_t irisRadius = max<int16_t>(7, min<int16_t>(eyeWidth / 3, visibleHeight / 3));
		const int16_t irisY = cy + gazeY + visibleHeight / 13;
		canvas.fillCircle(cx + gazeX, irisY, irisRadius, C_IRIS);
		canvas.fillCircle(cx + gazeX, irisY + 2, irisRadius * 2 / 3, C_PUPIL);
		canvas.fillCircle(cx + gazeX - irisRadius / 3, irisY - irisRadius / 3, max<int16_t>(2, irisRadius / 4), C_WHITE);
		canvas.fillCircle(cx + gazeX + irisRadius / 3, irisY + irisRadius / 3, max<int16_t>(1, irisRadius / 8), C_CYAN);
	}

	if (expression == BotExpression::Angry || expression == BotExpression::Hungry) {
		const int16_t browY = eyeTop - 7;
		if (leftEye) drawQuadratic(cx - eyeWidth / 2 - 2, browY - 2, cx, browY + 1, cx + eyeWidth / 2 + 2, browY + 8, browColor, 2);
		else drawQuadratic(cx - eyeWidth / 2 - 2, browY + 8, cx, browY + 1, cx + eyeWidth / 2 + 2, browY - 2, browColor, 2);
	}

	if (expression == BotExpression::Happy || expression == BotExpression::Love) {
		canvas.fillCircle(cx - eyeWidth / 2 - 2, cy + eyeHeight / 2, 4, C_PINK);
		canvas.fillCircle(cx + eyeWidth / 2 + 2, cy + eyeHeight / 2, 4, C_PINK);
	}
}

void BotEngine::drawMouth(int16_t cx, int16_t cy, int16_t width, BotExpression expression, float phase) {
	GFXcanvas16 &canvas = *faceBuffer;
	const uint16_t color = expression == BotExpression::Angry ? C_RED : (expression == BotExpression::Thirsty ? C_CYAN : C_MINT);
	if (expression == BotExpression::Sleepy || expression == BotExpression::Bored) {
		const int16_t yawn = expression == BotExpression::Sleepy ? 7 + static_cast<int16_t>((sinf(phase * 1.7f) + 1.0f) * 5.0f) : 5;
		canvas.fillRoundRect(cx - 5, cy - yawn / 2, 10, yawn, 5, color);
		return;
	}
	if (expression == BotExpression::Hungry || expression == BotExpression::Eating || expression == BotExpression::Drinking) {
		const int16_t openHeight = expression == BotExpression::Hungry ? 12 : 8 + static_cast<int16_t>((sinf(phase * 5.0f) + 1.0f) * 4.0f);
		canvas.fillRoundRect(cx - width / 8, cy - openHeight / 2, width / 4, openHeight, width / 8, C_PINK);
		canvas.drawFastHLine(cx - width / 10, cy + openHeight / 5, width / 5, C_RED);
		return;
	}
	if (expression == BotExpression::Angry) {
		canvas.drawLine(cx - 12, cy - 3, cx - 5, cy + 2, color);
		canvas.drawLine(cx - 5, cy + 2, cx + 2, cy - 3, color);
		canvas.drawLine(cx + 2, cy - 3, cx + 10, cy + 2, color);
		return;
	}
	const int16_t smile = (expression == BotExpression::Happy || expression == BotExpression::Love || expression == BotExpression::Playful) ? 8 : 3;
	drawQuadratic(cx - width / 5, cy - 2, cx, cy + smile, cx + width / 5, cy - 2, color, 2);
}

void BotEngine::drawHands(BotExpression expression, int16_t centerX, int16_t centerY, float phase) {
	if (expression != BotExpression::Eating && expression != BotExpression::Drinking && expression != BotExpression::Angry && expression != BotExpression::Playful) return;
	GFXcanvas16 &canvas = *faceBuffer;
	const int16_t armLift = static_cast<int16_t>(sinf(phase * (expression == BotExpression::Angry ? 7.0f : 4.0f)) * 4.0f);
	const uint16_t color = expression == BotExpression::Angry ? C_RED : C_WHITE;
	const int16_t armY = centerY + armLift;
	const int16_t spread = canvas.width() * 39 / 100;
	drawQuadratic(centerX - spread, armY, centerX - spread - 9, armY - 16, centerX - spread + 1, armY - 22, color, 3);
	drawQuadratic(centerX + spread, armY, centerX + spread + 9, armY - 16, centerX + spread - 1, armY - 22, color, 3);
	canvas.fillCircle(centerX - spread + 1, armY - 22, 5, color);
	canvas.fillCircle(centerX + spread - 1, armY - 22, 5, color);
	canvas.drawFastVLine(centerX - spread - 2, armY - 27, 8, color);
	canvas.drawFastVLine(centerX + spread + 2, armY - 27, 8, color);
	if (expression == BotExpression::Drinking) {
		canvas.drawRoundRect(centerX + spread - 1, armY - 37, 9, 13, 3, C_CYAN);
		canvas.drawFastVLine(centerX + spread + 3, armY - 42, 6, C_CYAN);
	}
	if (expression == BotExpression::Eating) {
		canvas.fillCircle(centerX - spread + 1, armY - 31, 3, C_AMBER);
	}
}

void BotEngine::drawClock(const BotVisualState &state, uint32_t now) {
	if (now - lastInfoUpdate < 250) return;
	lastInfoUpdate = now;
	display->fillRect(12, display->height() / 2 - 37, display->width() - 24, 78, C_BLACK);
	if (!state.timeSynced) {
		drawCentered("TIME NOT SYNCED", display->height() / 2 - 8, 2, C_AMBER);
		drawCentered("Setup Larken to sync", display->height() / 2 + 20, 1, C_WHITE);
		return;
	}
	struct tm timeInfo;
	if (!getLocalTime(&timeInfo, 5)) return;
	char clockText[9];
	char dateText[24];
	strftime(clockText, sizeof(clockText), "%H:%M:%S", &timeInfo);
	strftime(dateText, sizeof(dateText), "%a, %d %b %Y", &timeInfo);
	drawCentered(String(clockText), display->height() / 2 - 25, 3, C_WHITE);
	drawCentered(String(dateText), display->height() / 2 + 16, 1, C_CYAN);
}

void BotEngine::drawStatus(const BotVisualState &state, uint32_t now) {
	if (now - lastInfoUpdate < 1000) return;
	lastInfoUpdate = now;
	const int16_t valuesY = 42;
	const int16_t rowHeight = 19;
	for (uint8_t row = 0; row < 8; ++row) display->fillRect(display->width() / 2 - 4, valuesY + row * rowHeight - 1, display->width() / 2 + 2, 15, C_BLACK);
	const char *wifiState = state.wifiConnected ? "Connected" : (state.wifiConnecting ? "Connecting" : (state.wifiConfigured ? "Offline" : "Not set"));
	display->setTextSize(1);
	display->setTextColor(state.wifiConnected ? C_MINT : C_AMBER, C_BLACK);
	display->setCursor(display->width() / 2 - 2, valuesY);
	display->print(wifiState);
	display->setTextColor(C_WHITE, C_BLACK);
	display->setCursor(display->width() / 2 - 2, valuesY + rowHeight);
	display->print(shortValue(valueOr(state.wifiSsid, "-"), display->width() > 260 ? 22 : 15));
	display->setCursor(display->width() / 2 - 2, valuesY + rowHeight * 2);
	display->print(state.setupApActive ? "Broadcasting" : "Off");
	display->setCursor(display->width() / 2 - 2, valuesY + rowHeight * 3);
	display->print(state.setupApActive ? "192.168.4.1" : "-");
	display->setCursor(display->width() / 2 - 2, valuesY + rowHeight * 4);
	display->print(state.timeSynced ? "Synced" : "Not synced");
	const uint8_t needs[] = {state.hunger, state.thirst, state.energy};
	for (uint8_t i = 0; i < 3; ++i) {
		display->setCursor(display->width() / 2 - 2, valuesY + rowHeight * (5 + i));
		display->print(needs[i]);
		display->print('%');
	}
}

void BotEngine::drawGame(const BotVisualState &state, uint32_t now) {
	if (now - lastFrameAt < 33) return;
	lastFrameAt = now;
	const int16_t centerX = display->width() / 2;
	const int16_t centerY = display->height() / 2;
	const int16_t radius = min<int16_t>(display->width() / 5, display->height() / 4) - 4;
	if (previousGameX >= 0) display->fillCircle(previousGameX, previousGameY, 5, C_BLACK);
	const float angle = now / 500.0f;
	const int16_t x = centerX + static_cast<int16_t>(cosf(angle) * radius);
	const int16_t y = centerY + static_cast<int16_t>(sinf(angle) * radius);
	display->fillCircle(x, y, 5, C_AMBER);
	previousGameX = x;
	previousGameY = y;
	if (lastGameScore != state.gameScore) {
		display->fillRect(0, display->height() - 25, display->width(), 16, C_BLACK);
		drawCentered("Score: " + String(state.gameScore), display->height() - 22, 1, C_MINT);
		lastGameScore = state.gameScore;
	}
	++framesInWindow;
}

void BotEngine::drawSetup(const BotVisualState &state, uint32_t now) {
	if (now - lastInfoUpdate < 500) return;
	lastInfoUpdate = now;
	display->fillRect(18, 125, display->width() - 36, 42, C_BLACK);
	drawCentered(state.setupApActive ? "SETUP NETWORK READY" : "SETUP NETWORK FAILED", 128, 1, state.setupApActive ? C_MINT : C_RED);
	const String status = state.wifiConnected ? "Home Wi-Fi connected" : valueOr(state.wifiMessage, "Waiting for Wi-Fi");
	drawCentered(shortValue(status, display->width() > 260 ? 36 : 25), 148, 1, state.wifiConnected ? C_MINT : C_AMBER);
}

void BotEngine::drawDeveloper(const BotVisualState &state, uint32_t now) {
	if (now - lastInfoUpdate < 500) return;
	lastInfoUpdate = now;
	const int16_t y = 45;
	for (uint8_t row = 0; row < 8; ++row) display->fillRect(display->width() / 2 - 4, y + row * 19 - 1, display->width() / 2 + 2, 15, C_BLACK);
	const String values[] = {
		String(state.uptimeSeconds) + " s",
		String(fps) + " fps",
		String(ESP.getFreeHeap()) + " bytes",
		String(state.framebufferBytes) + " bytes",
		String(state.touch1Active ? "ON" : "off") + " / " + (state.touch2Active ? "ON" : "off"),
		state.wifiConnected ? "Connected" : (state.wifiConnecting ? "Connecting" : "Offline"),
		state.setupApActive ? "Active" : "Off",
		String(state.hunger) + " / " + String(state.thirst) + " / " + String(state.energy)
	};
	for (uint8_t row = 0; row < 8; ++row) {
		display->setTextSize(1);
		display->setTextColor(C_WHITE, C_BLACK);
		display->setCursor(display->width() / 2 - 2, y + row * 19);
		display->print(values[row]);
	}
}

void BotEngine::drawNeedBar(int16_t y, uint8_t value, uint16_t color) {
	const int16_t x = 66;
	const int16_t width = display->width() - x - 14;
	const int16_t fillWidth = (width - 4) * min<uint8_t>(100, value) / 100;
	display->fillRect(x + 2, y + 2, width - 4, 4, C_BLACK);
	if (fillWidth > 0) display->fillRoundRect(x + 2, y + 2, fillWidth, 4, 2, color);
}

void BotEngine::drawCentered(const String &text, int16_t y, uint8_t size, uint16_t color) {
	display->setTextSize(size);
	display->setTextColor(color, C_BLACK);
	int16_t x1, y1;
	uint16_t width, height;
	display->getTextBounds(text, 0, y, &x1, &y1, &width, &height);
	display->setCursor((display->width() - width) / 2, y);
	display->print(text);
}

void BotEngine::drawQuadratic(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, uint8_t thickness) {
	GFXcanvas16 &canvas = *faceBuffer;
	int16_t previousX = x0;
	int16_t previousY = y0;
	for (uint8_t step = 1; step <= 12; ++step) {
		const float t = step / 12.0f;
		const float inverse = 1.0f - t;
		const int16_t x = static_cast<int16_t>(inverse * inverse * x0 + 2 * inverse * t * x1 + t * t * x2);
		const int16_t y = static_cast<int16_t>(inverse * inverse * y0 + 2 * inverse * t * y1 + t * t * y2);
		for (uint8_t offset = 0; offset < thickness; ++offset) canvas.drawLine(previousX, previousY + offset, x, y + offset, color);
		previousX = x;
		previousY = y;
	}
}
