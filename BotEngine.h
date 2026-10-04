#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>

class GFXcanvas16;

#include <Arduino.h>

enum class LarkenScreen : uint8_t { Companion, Clock, Status, Games, Setup, Developer };
enum class BotExpression : uint8_t { Calm, Happy, Love, Hungry, Thirsty, Sleepy, Angry, Eating, Drinking, Bored, Playful };

struct BotVisualState {
	LarkenScreen screen = LarkenScreen::Companion;
	BotExpression expression = BotExpression::Calm;
	const String *botName = nullptr;
	const String *ownerName = nullptr;
	const String *wifiSsid = nullptr;
	const String *wifiMessage = nullptr;
	uint8_t hunger = 0;
	uint8_t thirst = 0;
	uint8_t energy = 0;
	uint8_t gameIndex = 0;
	uint16_t gameScore = 0;
	uint32_t framebufferBytes = 0;
	uint32_t uptimeSeconds = 0;
	bool wifiConfigured = false;
	bool wifiConnecting = false;
	bool wifiConnected = false;
	bool setupApActive = false;
	bool timeSynced = false;
	bool touch1Active = false;
	bool touch2Active = false;
};

class BotEngine {
public:
	BotEngine() = default;
	~BotEngine();

	bool begin(Adafruit_SPITFT &display);
	void render(const BotVisualState &state, uint32_t now);
	uint16_t measuredFps() const { return fps; }
	uint32_t framebufferBytes() const;
	bool isBuffered() const { return faceBuffer != nullptr; }

private:
	Adafruit_SPITFT *display = nullptr;
	GFXcanvas16 *faceBuffer = nullptr;
	LarkenScreen activeScreen = LarkenScreen::Companion;
	bool screenInitialized = false;
	uint16_t fps = 0;
	uint16_t framesInWindow = 0;
	uint32_t fpsWindowStarted = 0;
	uint32_t lastFrameAt = 0;
	uint32_t lastInfoUpdate = 0;
	int16_t faceX = 0;
	int16_t faceY = 0;
	int16_t lastFood = -1;
	int16_t lastWater = -1;
	int16_t lastEnergy = -1;
	int16_t previousGameX = -1;
	int16_t previousGameY = -1;
	uint16_t lastGameScore = 0xFFFF;

	bool allocateFaceBuffer();
	void drawScreenBase(const BotVisualState &state);
	void drawCompanionBase(const BotVisualState &state);
	void drawClockBase();
	void drawStatusBase();
	void drawGamesBase(const BotVisualState &state);
	void drawSetupBase();
	void drawDeveloperBase();
	void drawCompanion(const BotVisualState &state, uint32_t now);
	void drawFaceFrame(BotExpression expression, uint32_t now);
	void drawEye(int16_t cx, int16_t cy, int16_t eyeWidth, int16_t eyeHeight, BotExpression expression, float phase, bool leftEye);
	void drawMouth(int16_t cx, int16_t cy, int16_t width, BotExpression expression, float phase);
	void drawHands(BotExpression expression, int16_t centerX, int16_t centerY, float phase);
	void drawClock(const BotVisualState &state, uint32_t now);
	void drawStatus(const BotVisualState &state, uint32_t now);
	void drawGame(const BotVisualState &state, uint32_t now);
	void drawSetup(const BotVisualState &state, uint32_t now);
	void drawDeveloper(const BotVisualState &state, uint32_t now);
	void drawNeedBar(int16_t y, uint8_t value, uint16_t color);
	void drawCentered(const String &text, int16_t y, uint8_t size, uint16_t color);
	void drawQuadratic(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, uint8_t thickness = 1);
};
