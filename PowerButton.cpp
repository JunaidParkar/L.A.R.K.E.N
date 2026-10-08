#include "PowerButton.h"

void PowerButton::begin(uint8_t buttonPin) {
	pin = buttonPin;
	pinMode(pin, INPUT_PULLUP);
	rawDown = digitalRead(pin) == LOW;
	stableDown = rawDown;
	rawChangedAt = millis();
	pressedAt = rawChangedAt;
}

PowerButtonEvent PowerButton::update(uint32_t now) {
	const bool currentRawDown = digitalRead(pin) == LOW;
	if (currentRawDown != rawDown) {
		rawDown = currentRawDown;
		rawChangedAt = now;
	}

	if (rawDown != stableDown && now - rawChangedAt >= DEBOUNCE_MS) {
		stableDown = rawDown;
		if (stableDown) {
			pressedAt = now;
			longPressSent = false;
		} else if (!longPressSent) {
			if (clickCount < 3) ++clickCount;
			clickDeadline = now + CLICK_WINDOW_MS;
		}
	}

	if (stableDown && !longPressSent && now - pressedAt >= LONG_PRESS_MS) {
		longPressSent = true;
		clickCount = 0;
		return PowerButtonEvent::LongPress;
	}

	if (clickCount && static_cast<int32_t>(now - clickDeadline) >= 0) {
		const uint8_t completedClicks = clickCount;
		clickCount = 0;
		if (completedClicks == 1) return PowerButtonEvent::SingleClick;
		if (completedClicks == 2) return PowerButtonEvent::DoubleClick;
		return PowerButtonEvent::TripleClick;
	}

	return PowerButtonEvent::None;
}
