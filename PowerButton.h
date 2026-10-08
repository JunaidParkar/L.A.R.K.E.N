#pragma once

#include <Arduino.h>

enum class PowerButtonEvent : uint8_t { None, LongPress, SingleClick, DoubleClick, TripleClick };

class PowerButton {
public:
	void begin(uint8_t pin);
	PowerButtonEvent update(uint32_t now);

private:
	static const uint16_t DEBOUNCE_MS = 35;
	static const uint16_t CLICK_WINDOW_MS = 500;
	static const uint16_t LONG_PRESS_MS = 3000;

	uint8_t pin = 0;
	bool rawDown = false;
	bool stableDown = false;
	bool longPressSent = false;
	uint8_t clickCount = 0;
	uint32_t rawChangedAt = 0;
	uint32_t pressedAt = 0;
	uint32_t clickDeadline = 0;
};
