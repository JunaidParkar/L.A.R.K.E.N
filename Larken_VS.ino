#include <SPI.h>
#include <esp_sleep.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_ST7789.h>
#include "WifiEngine.h"
#include "BotEngine.h"
#include "PowerButton.h"

#define LARKEN_USE_ST7789 0

static const uint8_t PIN_TFT_SCK = 4;
static const uint8_t PIN_TFT_MOSI = 6;
static const uint8_t PIN_TFT_CS = 7;
static const uint8_t PIN_TFT_DC = 3;
static const uint8_t PIN_TFT_RST = 10;
static const uint8_t PIN_TOUCH_T1 = 0;
static const uint8_t PIN_TOUCH_T2 = 1;
static const uint8_t PIN_POWER_BUTTON = 5;

#if LARKEN_USE_ST7789
static const uint16_t TFT_NATIVE_WIDTH = 240;
static const uint16_t TFT_NATIVE_HEIGHT = 240;
Adafruit_ST7789 tft(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
#else
Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
#endif

WifiEngine wifiEngine;
BotEngine botEngine;
PowerButton powerButton;
LarkenScreen screenMode = LarkenScreen::Companion;
BotExpression actionExpression = BotExpression::Calm;

uint8_t hunger = 76;
uint8_t thirst = 72;
uint8_t energy = 88;
uint8_t gameIndex = 0;
uint16_t gameScore = 0;
uint32_t lastInteraction = 0;
uint32_t actionUntil = 0;
uint32_t lastNeedsUpdate = 0;
uint32_t t1PressStarted = 0;
uint32_t t2PressStarted = 0;
uint32_t pendingTapAt = 0;
uint32_t lastSerialHeartbeat = 0;
uint8_t pendingTaps = 0;
bool t1WasTouched = false;
bool t2WasTouched = false;
bool t1LongHandled = false;
bool t2LongHandled = false;

bool isTouchActive(uint8_t pin) {
	return digitalRead(pin) == HIGH;
}

void setAction(BotExpression expression, uint32_t durationMs) {
	actionExpression = expression;
	actionUntil = millis() + durationMs;
}

void interact() {
	lastInteraction = millis();
	if (screenMode == LarkenScreen::Games) {
		++gameScore;
		setAction(BotExpression::Playful, 1600);
		return;
	}

	static uint8_t nextCareAction = 0;
	if (nextCareAction == 0) {
		hunger = min<uint8_t>(100, hunger + 18);
		setAction(BotExpression::Eating, 2600);
	} else if (nextCareAction == 1) {
		thirst = min<uint8_t>(100, thirst + 22);
		setAction(BotExpression::Drinking, 2400);
	} else {
		setAction(BotExpression::Love, 2200);
	}
	nextCareAction = (nextCareAction + 1) % 3;
	energy = min<uint8_t>(100, energy + 2);
}

void toggleSetupMode() {
	wifiEngine.toggleSetupPortal();
	screenMode = wifiEngine.setupPortalActive() ? LarkenScreen::Setup : LarkenScreen::Companion;
}

void handleT1Gesture(uint8_t taps) {
	if (taps == 1) interact();
	else if (taps == 2) toggleSetupMode();
	else if (taps >= 3) {
		screenMode = screenMode == LarkenScreen::Developer ? LarkenScreen::Companion : LarkenScreen::Developer;
	}
}

void cycleMode() {
	if (screenMode == LarkenScreen::Setup || screenMode == LarkenScreen::Developer) {
		screenMode = LarkenScreen::Companion;
	} else {
		const uint8_t nextMode = (static_cast<uint8_t>(screenMode) + 1) % 4;
		screenMode = static_cast<LarkenScreen>(nextMode);
	}
}

void updateTouches(uint32_t now) {
	if (screenMode == LarkenScreen::PowerMenu || screenMode == LarkenScreen::ResetConfirm) return;

	const bool t1 = isTouchActive(PIN_TOUCH_T1);
	if (t1 && !t1WasTouched) {
		t1WasTouched = true;
		t1LongHandled = false;
		t1PressStarted = now;
	} else if (!t1 && t1WasTouched) {
		t1WasTouched = false;
		if (!t1LongHandled) {
			++pendingTaps;
			pendingTapAt = now;
		} else {
			pendingTaps = 0;
		}
	}
	if (t1 && !t1LongHandled && now - t1PressStarted > 900) {
		t1LongHandled = true;
		pendingTaps = 0;
		setAction(BotExpression::Angry, 3500);
	}
	if (pendingTaps && !t1WasTouched && now - pendingTapAt > 420) {
		handleT1Gesture(pendingTaps);
		pendingTaps = 0;
	}

	const bool t2 = isTouchActive(PIN_TOUCH_T2);
	if (t2 && !t2WasTouched) {
		t2WasTouched = true;
		t2LongHandled = false;
		t2PressStarted = now;
	} else if (!t2 && t2WasTouched) {
		t2WasTouched = false;
		if (!t2LongHandled) cycleMode();
	}
	if (t2 && !t2LongHandled && now - t2PressStarted > 1000) {
		t2LongHandled = true;
		toggleSetupMode();
	}
}

void enterDeepSleep() {
	const esp_err_t wakeResult = esp_deep_sleep_enable_gpio_wakeup(
		1ULL << PIN_POWER_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
	if (wakeResult != ESP_OK) {
		Serial.printf("POWER: GPIO wake setup failed (%d); staying awake\n", static_cast<int>(wakeResult));
		return;
	}

	Serial.println("POWER: entering deep sleep; press the power button to wake");
	botEngine.prepareForDeepSleep();
	wifiEngine.shutdownForSleep();
	Serial.flush();
	delay(20);
	esp_deep_sleep_start();
}

void handlePowerButton(uint32_t now) {
	const PowerButtonEvent event = powerButton.update(now);
	if (event == PowerButtonEvent::None) return;

	if (screenMode == LarkenScreen::PowerMenu) {
		if (event == PowerButtonEvent::SingleClick) enterDeepSleep();
		else if (event == PowerButtonEvent::DoubleClick) ESP.restart();
		else if (event == PowerButtonEvent::TripleClick) screenMode = LarkenScreen::ResetConfirm;
		return;
	}

	if (screenMode == LarkenScreen::ResetConfirm) {
		if (event == PowerButtonEvent::SingleClick) screenMode = LarkenScreen::Companion;
		else if (event == PowerButtonEvent::DoubleClick) {
			wifiEngine.resetUserSettings();
			screenMode = LarkenScreen::Setup;
		}
		return;
	}

	if (event == PowerButtonEvent::LongPress) screenMode = LarkenScreen::PowerMenu;
}

BotExpression currentExpression(uint32_t now) {
	if (t1WasTouched && now - t1PressStarted > 2600) return BotExpression::Angry;
	if (static_cast<int32_t>(actionUntil - now) > 0) return actionExpression;
	if (energy < 18) return BotExpression::Sleepy;
	if (thirst < 24) return BotExpression::Thirsty;
	if (hunger < 24) return BotExpression::Hungry;
	if (now - lastInteraction > 150000) return BotExpression::Bored;
	if (now - lastInteraction < 12000) return BotExpression::Happy;
	return BotExpression::Calm;
}

void updateNeeds(uint32_t now) {
	if (now - lastNeedsUpdate < 60000) return;
	const uint32_t elapsedMinutes = (now - lastNeedsUpdate) / 60000;
	lastNeedsUpdate += elapsedMinutes * 60000;
	hunger = hunger > elapsedMinutes ? hunger - elapsedMinutes : 0;
	thirst = thirst > elapsedMinutes * 2 ? thirst - elapsedMinutes * 2 : 0;
	if (energy > 0) --energy;
	if (energy < 18 && now - lastInteraction < 5000) energy = min<uint8_t>(100, energy + 4);
}

void setup() {
	Serial.begin(115200);
	const uint32_t serialWaitStarted = millis();
	while (!Serial && millis() - serialWaitStarted < 3000) delay(10);
	Serial.println("LARKEN BOOT: serial online, 115200 baud");
	Serial.println("LARKEN BOOT: starting Wi-Fi engine");
	powerButton.begin(PIN_POWER_BUTTON);
	pinMode(PIN_TOUCH_T1, INPUT);
	pinMode(PIN_TOUCH_T2, INPUT);
	wifiEngine.begin();
	if (wifiEngine.setupPortalActive()) screenMode = LarkenScreen::Setup;

	Serial.println("LARKEN BOOT: starting display");
	SPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
#if LARKEN_USE_ST7789
	tft.init(TFT_NATIVE_WIDTH, TFT_NATIVE_HEIGHT);
#else
	tft.begin();
#endif
	tft.setRotation(1);
	tft.setSPISpeed(40000000);
	if (!botEngine.begin(tft)) Serial.println("LARKEN WARNING: face framebuffer unavailable");

	const uint32_t now = millis();
	lastInteraction = now;
	lastNeedsUpdate = now;
	Serial.printf("LARKEN READY: free heap=%u\n", static_cast<unsigned int>(ESP.getFreeHeap()));
}

void loop() {
	const uint32_t now = millis();
	handlePowerButton(now);
	updateTouches(now);
	updateNeeds(now);
	wifiEngine.update(now);

	BotVisualState visualState;
	visualState.screen = screenMode;
	visualState.expression = currentExpression(now);
	visualState.botName = &wifiEngine.botName();
	visualState.ownerName = &wifiEngine.ownerName();
	visualState.wifiSsid = &wifiEngine.connectedSsid();
	visualState.wifiMessage = &wifiEngine.statusMessage();
	visualState.hunger = hunger;
	visualState.thirst = thirst;
	visualState.energy = energy;
	visualState.gameIndex = gameIndex;
	visualState.gameScore = gameScore;
	visualState.framebufferBytes = botEngine.framebufferBytes();
	visualState.uptimeSeconds = now / 1000;
	visualState.wifiConfigured = wifiEngine.hasCredentials();
	visualState.wifiConnecting = wifiEngine.isConnecting();
	visualState.wifiConnected = wifiEngine.isConnected();
	visualState.setupApActive = wifiEngine.setupPortalActive();
	visualState.timeSynced = wifiEngine.hasTime();
	visualState.touch1Active = t1WasTouched;
	visualState.touch2Active = t2WasTouched;
	botEngine.render(visualState, now);

	if (now - lastSerialHeartbeat >= 3000) {
		lastSerialHeartbeat = now;
		Serial.printf("LARKEN ALIVE: uptime=%lu ms, fps=%u, heap=%u, face-buffer=%lu, AP=%s\n",
			static_cast<unsigned long>(now), botEngine.measuredFps(),
			static_cast<unsigned int>(ESP.getFreeHeap()),
			static_cast<unsigned long>(botEngine.framebufferBytes()),
			wifiEngine.setupPortalActive() ? "active" : "off");
	}
}