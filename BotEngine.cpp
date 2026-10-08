#include "BotEngine.h"
#include <time.h>
#include <math.h>
#include <new>

namespace {
const uint16_t C_BLACK = 0x0000;
const uint16_t C_PANEL = 0x0841;
const uint16_t C_PANEL_EDGE = 0x2128;
const uint16_t C_WHITE = 0xFFFF;
const uint16_t C_SCLERA = 0xEF7D;
const uint16_t C_CYAN = 0x07FF;
const uint16_t C_MINT = 0x7FEA;
const uint16_t C_TEAL = 0x04F2;
const uint16_t C_PINK = 0xF9B6;
const uint16_t C_SKIN = 0xEF9B;
const uint16_t C_AMBER = 0xFDC0;
const uint16_t C_RED = 0xF800;
const uint16_t C_IRIS = 0x2D7F;
const uint16_t C_PUPIL = 0x0823;
const uint16_t C_MOUTH = 0xA9B2;
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
	(void)state;
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
	const float phase = now / 1000.0f;
	const float gazeX = sinf(phase * 0.47f) * 7.0f + sinf(phase * 0.19f) * 2.0f;
	const float gazeY = sinf(phase * 0.31f + 0.7f) * 4.0f;
	const int16_t wholeEyeX = static_cast<int16_t>(gazeX);
	const int16_t wholeEyeY = static_cast<int16_t>(gazeY);
	const int16_t eyeWidth = max<int16_t>(30, min<int16_t>(64, canvas.width() * 25 / 100));
	const int16_t eyeHeight = min<int16_t>(76, eyeWidth * 118 / 100);
	const int16_t eyeOffset = canvas.width() * 17 / 100;
	const int16_t eyeY = canvas.height() * 40 / 100 + wholeEyeY;
	drawEye(centerX - eyeOffset + wholeEyeX, eyeY, eyeWidth, eyeHeight, expression, phase, true);
	drawEye(centerX + eyeOffset + wholeEyeX, eyeY, eyeWidth, eyeHeight, expression, phase, false);

	if (expression == BotExpression::Happy || expression == BotExpression::Love ||
		expression == BotExpression::Hungry || expression == BotExpression::Thirsty ||
		expression == BotExpression::Sleepy || expression == BotExpression::Angry ||
		expression == BotExpression::Eating || expression == BotExpression::Drinking ||
		expression == BotExpression::Playful) {
		drawMouth(centerX, canvas.height() * 77 / 100, canvas.width() / 6, expression, phase);
	}
	drawHands(expression, centerX, canvas.height() * 79 / 100, phase);
}

void BotEngine::drawEye(int16_t cx, int16_t cy, int16_t eyeWidth, int16_t eyeHeight, BotExpression expression, float phase, bool leftEye) {
	GFXcanvas16 &canvas = *faceBuffer;
	const uint16_t blinkPhase = static_cast<uint16_t>(static_cast<uint32_t>(phase * 1000.0f) % 4100);
	float blink = 0.0f;
	if (blinkPhase < 155) blink = sinf((blinkPhase / 155.0f) * 3.14159f);

	if (expression == BotExpression::Sleepy) {
		drawQuadratic(cx - eyeWidth / 2, cy, cx, cy + 3, cx + eyeWidth / 2, cy, C_SCLERA, 4);
		return;
	}

	float openness = 1.0f - blink * 0.94f;
	if (expression == BotExpression::Love) openness *= 0.76f;
	if (expression == BotExpression::Happy) openness *= 1.08f;
	if (expression == BotExpression::Bored) openness *= 0.66f;
	if (expression == BotExpression::Angry) openness *= 0.74f;
	if (expression == BotExpression::Thirsty) openness *= 0.84f;
	const int16_t visibleHeight = max<int16_t>(4, static_cast<int16_t>(eyeHeight * openness));
	const int16_t eyeTop = cy - visibleHeight / 2;
	const int16_t radius = min<int16_t>(eyeWidth / 3, visibleHeight / 2);
	canvas.drawRoundRect(cx - eyeWidth / 2 - 1, eyeTop - 1, eyeWidth + 2, visibleHeight + 2, radius + 1, C_PANEL_EDGE);
	canvas.fillRoundRect(cx - eyeWidth / 2, eyeTop, eyeWidth, visibleHeight, radius, C_SCLERA);

	if (visibleHeight > eyeHeight / 3) {
		const int16_t pupilDriftX = static_cast<int16_t>(sinf(phase * 0.83f + (leftEye ? 0.0f : 0.16f)) * 2.0f);
		const int16_t irisRadius = max<int16_t>(7, min<int16_t>(eyeWidth / 4, visibleHeight / 3));
		const int16_t irisY = cy + visibleHeight / 12;
		canvas.fillCircle(cx + pupilDriftX, irisY, irisRadius, C_IRIS);
		canvas.fillCircle(cx + pupilDriftX, irisY + 2, irisRadius * 2 / 3, C_PUPIL);
		canvas.fillCircle(cx + pupilDriftX - irisRadius / 3, irisY - irisRadius / 3, max<int16_t>(2, irisRadius / 4), C_WHITE);
		canvas.fillCircle(cx + pupilDriftX + irisRadius / 3, irisY + irisRadius / 3, max<int16_t>(1, irisRadius / 8), C_CYAN);
	}

	if (expression == BotExpression::Angry) {
		const int16_t browY = eyeTop - 7;
		if (leftEye) drawQuadratic(cx - eyeWidth / 2 - 2, browY - 2, cx, browY + 1, cx + eyeWidth / 2 + 2, browY + 8, C_RED, 3);
		else drawQuadratic(cx - eyeWidth / 2 - 2, browY + 8, cx, browY + 1, cx + eyeWidth / 2 + 2, browY - 2, C_RED, 3);
	}
	if (expression == BotExpression::Love || expression == BotExpression::Playful) {
		canvas.fillCircle(cx - eyeWidth / 2 + 1, cy + eyeHeight / 2 - 2, 4, C_PINK);
		canvas.fillCircle(cx + eyeWidth / 2 - 1, cy + eyeHeight / 2 - 2, 4, C_PINK);
	}
}

void BotEngine::drawMouth(int16_t cx, int16_t cy, int16_t width, BotExpression expression, float phase) {
	GFXcanvas16 &canvas = *faceBuffer;
	if (expression == BotExpression::Calm || expression == BotExpression::Bored) return;
	const uint16_t lipColor = expression == BotExpression::Angry ? C_RED : C_PINK;
	if (expression == BotExpression::Sleepy || expression == BotExpression::Hungry || expression == BotExpression::Eating || expression == BotExpression::Drinking) {
		const int16_t openHeight = expression == BotExpression::Sleepy
			? 5 + static_cast<int16_t>((sinf(phase * 1.8f) + 1.0f) * 7.0f)
			: (expression == BotExpression::Hungry ? 16 : 6 + static_cast<int16_t>((sinf(phase * 4.8f) + 1.0f) * 5.0f));
		const int16_t openWidth = expression == BotExpression::Sleepy ? 13 : (expression == BotExpression::Hungry ? 21 : 17);
		canvas.fillRoundRect(cx - openWidth / 2, cy - openHeight / 2, openWidth, openHeight, openWidth / 2, lipColor);
		if (openHeight > 10) canvas.fillRoundRect(cx - openWidth / 3, cy + 1, openWidth * 2 / 3, openHeight / 4, 3, C_MOUTH);
		return;
	}
	if (expression == BotExpression::Thirsty) {
		canvas.drawRoundRect(cx - 5, cy - 4, 10, 9, 4, C_CYAN);
		return;
	}
	if (expression == BotExpression::Angry) {
		canvas.drawLine(cx - 12, cy - 2, cx - 5, cy + 2, lipColor);
		canvas.drawLine(cx - 5, cy + 2, cx + 2, cy - 2, lipColor);
		canvas.drawLine(cx + 2, cy - 2, cx + 10, cy + 2, lipColor);
		return;
	}
	const int16_t smile = expression == BotExpression::Happy || expression == BotExpression::Love || expression == BotExpression::Playful ? 7 : 3;
	drawQuadratic(cx - width / 4, cy - 2, cx, cy + smile, cx + width / 4, cy - 2, lipColor, 2);
}

void BotEngine::drawHands(BotExpression expression, int16_t centerX, int16_t centerY, float phase) {
	GFXcanvas16 &canvas = *faceBuffer;
	const int16_t handLift = static_cast<int16_t>(sinf(phase * 2.2f) * 2.0f);
	const int16_t cheekY = canvas.height() * 63 / 100 + handLift;
	const uint16_t handColor = C_SCLERA;
	if (expression == BotExpression::Love) {
		drawHeartHands(centerX, canvas.height() * 65 / 100);
	} else if (expression == BotExpression::Happy || expression == BotExpression::Playful) {
		const int16_t cheekX = canvas.width() * 33 / 100;
		drawHand(centerX - cheekX, cheekY, true, handColor);
		drawHand(centerX + cheekX, cheekY, false, handColor);
	} else if (expression == BotExpression::Hungry || expression == BotExpression::Eating || expression == BotExpression::Drinking) {
		const int16_t reach = expression == BotExpression::Eating ? static_cast<int16_t>((sinf(phase * 3.8f) + 1.0f) * 5.0f) : 0;
		const int16_t handX = centerX + canvas.width() * 23 / 100 - reach;
		const int16_t handY = expression == BotExpression::Hungry ? canvas.height() * 57 / 100 : canvas.height() * 63 / 100 - reach;
		if (expression == BotExpression::Drinking) {
			const int16_t cupX = centerX + 16;
			const int16_t cupY = canvas.height() * 52 / 100;
			canvas.drawRoundRect(cupX, cupY, 16, 21, 3, C_CYAN);
			canvas.drawFastHLine(cupX + 2, cupY + 6, 12, C_CYAN);
			canvas.drawLine(cupX + 9, cupY, cupX + 5, cupY - 7, C_WHITE);
			drawHand(cupX + 17, cupY + 18, false, handColor);
		} else {
			drawHand(handX, handY, false, handColor);
		}
	} else if (expression == BotExpression::Sleepy) {
		const int16_t stretch = static_cast<int16_t>((sinf(phase * 1.1f) + 1.0f) * 5.0f);
		const int16_t raisedY = canvas.height() * 46 / 100 - stretch;
		drawHand(30, raisedY, true, handColor);
		drawHand(canvas.width() - 30, raisedY, false, handColor);
	} else if (expression == BotExpression::Angry) {
		const int16_t fistY = canvas.height() - 12 + static_cast<int16_t>(sinf(phase * 8.0f) * 2.0f);
		canvas.fillRoundRect(7, fistY - 12, 23, 15, 7, C_SCLERA);
		canvas.fillRoundRect(canvas.width() - 30, fistY - 12, 23, 15, 7, C_SCLERA);
		for (int8_t crease = 0; crease < 3; ++crease) {
			canvas.drawFastHLine(12, fistY - 8 + crease * 4, 12, C_TEAL);
			canvas.drawFastHLine(canvas.width() - 24, fistY - 8 + crease * 4, 12, C_TEAL);
		}
	}
}

void BotEngine::drawHand(int16_t palmX, int16_t palmY, bool leftHand, uint16_t skinColor) {
	GFXcanvas16 &canvas = *faceBuffer;
	const int8_t inward = leftHand ? 1 : -1;
	const int16_t palmTop = palmY - 4;
	const int16_t palmLeft = palmX - 19;
	const int16_t palmWidth = 38;
	const int16_t palmHeight = 30;
	const int8_t fingerX[] = {-15, -7, 1, 9};
	const int8_t fingerLength[] = {17, 26, 24, 16};
	for (int8_t finger = 0; finger < 4; ++finger) {
		const int16_t x = palmX + fingerX[finger];
		const int16_t tipY = palmY - fingerLength[finger];
		const int16_t fingerBottom = palmTop + 8;
		canvas.fillRoundRect(x, tipY, 6, fingerBottom - tipY, 3, skinColor);
		canvas.drawRoundRect(x, tipY, 6, fingerBottom - tipY, 3, C_PANEL_EDGE);
		canvas.drawFastHLine(x + 1, tipY + 5, 4, C_WHITE);
	}
	canvas.fillRoundRect(palmLeft, palmTop, palmWidth, palmHeight, 9, skinColor);
	canvas.drawRoundRect(palmLeft, palmTop, palmWidth, palmHeight, 9, C_PANEL_EDGE);
	drawQuadratic(palmX + inward * 16, palmY + 8, palmX + inward * 24, palmY + 2, palmX + inward * 19, palmY - 7, skinColor, 5);
	canvas.drawFastHLine(palmX - 7, palmY + 9, 10, C_PANEL_EDGE);
	canvas.drawFastHLine(palmX - 5, palmY + 16, 7, C_PANEL_EDGE);
}

void BotEngine::drawHeartHands(int16_t centerX, int16_t centerY) {
	GFXcanvas16 &canvas = *faceBuffer;
	const int16_t palmY = centerY + 5;
	canvas.fillRoundRect(centerX - 31, palmY - 1, 27, 22, 9, C_SCLERA);
	canvas.fillRoundRect(centerX + 4, palmY - 1, 27, 22, 9, C_SCLERA);
	canvas.drawRoundRect(centerX - 31, palmY - 1, 27, 22, 9, C_PANEL_EDGE);
	canvas.drawRoundRect(centerX + 4, palmY - 1, 27, 22, 9, C_PANEL_EDGE);
	// Index fingers meet at the cleft; thumbs meet below, with outer fingers visible on both palms.
	drawQuadratic(centerX - 10, palmY + 2, centerX - 12, palmY - 18, centerX, palmY - 12, C_SCLERA, 4);
	drawQuadratic(centerX + 10, palmY + 2, centerX + 12, palmY - 18, centerX, palmY - 12, C_SCLERA, 4);
	for (int8_t finger = 0; finger < 3; ++finger) {
		const int16_t offsetY = palmY - 5 - finger * 4;
		drawQuadratic(centerX - 28, offsetY, centerX - 22, offsetY - 3, centerX - 17, offsetY, C_SCLERA, 3);
		drawQuadratic(centerX + 28, offsetY, centerX + 22, offsetY - 3, centerX + 17, offsetY, C_SCLERA, 3);
	}
	drawQuadratic(centerX - 10, palmY + 12, centerX - 6, palmY + 24, centerX, palmY + 24, C_SCLERA, 4);
	drawQuadratic(centerX + 10, palmY + 12, centerX + 6, palmY + 24, centerX, palmY + 24, C_SCLERA, 4);
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
