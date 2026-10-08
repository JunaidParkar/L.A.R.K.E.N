#include "BotEngine.h"

#include <math.h>
#include <new>
#include <time.h>

namespace {

constexpr uint16_t C_BLACK     = 0x0000;
constexpr uint16_t C_WHITE     = 0xFFFF;
constexpr uint16_t C_EYE       = 0xF7DE;
constexpr uint16_t C_EYE_EDGE  = 0x6B8F;
constexpr uint16_t C_PUPIL     = 0x0861;
constexpr uint16_t C_TEAL      = 0x05B7;
constexpr uint16_t C_CYAN      = 0x07FF;
constexpr uint16_t C_MINT      = 0x7FEA;
constexpr uint16_t C_PINK      = 0xF81F;
constexpr uint16_t C_YELLOW    = 0xFFE0;
constexpr uint16_t C_AMBER     = 0xFD20;
constexpr uint16_t C_RED       = 0xF800;
constexpr uint16_t C_ORANGE    = 0xFB20;
constexpr uint16_t C_MOUTH     = 0xB8A6;
constexpr uint16_t C_MUTED     = 0x7BEF;
constexpr uint16_t C_PANEL     = 0x2128;

const char *GAME_NAMES[] = {
    "STAR CATCH", "BUBBLE POP", "TINY MAZE", "MOON HOP", "COLOR DOTS"
};

template <typename T>
T clampv(T v, T lo, T hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float easeInOut(float t) {
    t = clampv(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

String valueOr(const String *value, const char *fallback = "") {
    return value ? *value : String(fallback);
}

String shortValue(const String &value, size_t maxLength) {
    if (value.length() <= maxLength) return value;
    if (maxLength < 2) return value.substring(0, maxLength);
    return value.substring(0, maxLength - 1) + "~";
}

} // namespace

BotEngine::~BotEngine() {
    delete faceBuffer;
}

bool BotEngine::begin(Adafruit_SPITFT &target) {
    display = &target;

    if (!allocateFaceBuffer()) {
        Serial.println("BOT ENGINE: framebuffer allocation failed");
        return false;
    }

    fpsWindowStarted = millis();

    Serial.printf(
        "BOT ENGINE: character buffer %ux%u = %lu bytes\n",
        faceBuffer->width(),
        faceBuffer->height(),
        static_cast<unsigned long>(framebufferBytes())
    );

    return true;
}

uint32_t BotEngine::framebufferBytes() const {
    if (!faceBuffer) return 0;
    return static_cast<uint32_t>(faceBuffer->width()) *
           static_cast<uint32_t>(faceBuffer->height()) *
           sizeof(uint16_t);
}

void BotEngine::prepareForDeepSleep() {
    if (!display) return;
    display->sendCommand(0x28);
    display->sendCommand(0x10);
}

bool BotEngine::allocateFaceBuffer() {
    if (!display) return false;

    const int16_t availableWidth = display->width() - 24;
    const int16_t availableHeight = display->height() - 96;

    if (availableWidth < 150 || availableHeight < 90) return false;

    // Prefer the largest buffer that fits. One RGB565 buffer only.
    const uint16_t width =
        static_cast<uint16_t>(clampv<int16_t>(availableWidth, 150, 270));
    const uint16_t height =
        static_cast<uint16_t>(clampv<int16_t>(availableHeight, 90, 132));

    GFXcanvas16 *candidate =
        new (std::nothrow) GFXcanvas16(width, height);

    if (!candidate || !candidate->getBuffer()) {
        delete candidate;
        return false;
    }

    faceBuffer = candidate;
    faceX = (display->width() - width) / 2;
    faceY = (display->height() - height) / 2 - 2;

    return true;
}

void BotEngine::render(const BotVisualState &state, uint32_t now) {
    if (!display) return;

    if (!screenInitialized || activeScreen != state.screen) {
        activeScreen = state.screen;
        screenInitialized = true;
        lastInfoUpdate = 0;
        lastFrameAt = 0;
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
        case LarkenScreen::Companion:    drawCompanionBase(state); break;
        case LarkenScreen::Clock:        drawClockBase(); break;
        case LarkenScreen::Status:       drawStatusBase(); break;
        case LarkenScreen::Games:        drawGamesBase(state); break;
        case LarkenScreen::Setup:        drawSetupBase(); break;
        case LarkenScreen::Developer:    drawDeveloperBase(); break;
        case LarkenScreen::PowerMenu:    drawPowerMenuBase(); break;
        case LarkenScreen::ResetConfirm: drawResetConfirmBase(); break;
    }
}

void BotEngine::drawCompanionBase(const BotVisualState &state) {
    (void)state;
}

void BotEngine::drawClockBase() {
    drawCentered("LARKEN CLOCK", 14, 2, C_MINT);
    display->drawFastHLine(18, 43, display->width() - 36, C_PANEL);
    drawCentered("T2 changes mode", display->height() - 22, 1, C_MUTED);
}

void BotEngine::drawStatusBase() {
    drawCentered("SYSTEM STATUS", 10, 2, C_MINT);

    const char *labels[] = {
        "Wi-Fi", "Network", "Setup AP", "Portal",
        "Time sync", "Food", "Water", "Energy"
    };

    for (uint8_t i = 0; i < 8; ++i) {
        display->setTextSize(1);
        display->setTextColor(C_MUTED, C_BLACK);
        display->setCursor(14, 42 + i * 19);
        display->print(labels[i]);
    }

    display->drawFastHLine(14, 201, display->width() - 28, C_PANEL);
    drawCentered("T1 interact  T2 next", display->height() - 18, 1, C_MUTED);
}

void BotEngine::drawGamesBase(const BotVisualState &state) {
    drawCentered("LARKEN ARCADE", 10, 2, C_MINT);

    const uint8_t index =
        state.gameIndex % (sizeof(GAME_NAMES) / sizeof(GAME_NAMES[0]));

    drawCentered(GAME_NAMES[index], 42, 2, C_CYAN);

    const int16_t centerX = display->width() / 2;
    const int16_t centerY = display->height() / 2;
    const int16_t radius =
        min<int16_t>(display->width() / 5, display->height() / 4);

    display->drawCircle(centerX, centerY, radius, C_PANEL);
    display->drawCircle(centerX, centerY, radius - 7, C_CYAN);

    drawCentered("T1 tap to play", display->height() - 42, 1, C_WHITE);
}

void BotEngine::drawSetupBase() {
    drawCentered("LARKEN SETUP", 14, 2, C_MINT);
    drawCentered("Connect to L.A.R.K.E.N Setup", 56, 1, C_WHITE);
    drawCentered("Open http://192.168.4.1", 82, 1, C_CYAN);
    display->drawFastHLine(20, 117, display->width() - 40, C_PANEL);
    drawCentered("T2 hold to exit", display->height() - 24, 1, C_MUTED);
}

void BotEngine::drawDeveloperBase() {
    drawCentered("ENGINEER HUD", 8, 2, C_PINK);
    display->drawFastHLine(16, 35, display->width() - 32, C_PANEL);

    const char *labels[] = {
        "Uptime", "Frame rate", "Heap", "Face buffer",
        "Touch T1 / T2", "Wi-Fi", "Access point", "Needs F/W/E"
    };

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

    display->drawRGBBitmap(
        faceX,
        faceY,
        faceBuffer->getBuffer(),
        faceBuffer->width(),
        faceBuffer->height()
    );

    lastFrameAt = now;
    ++framesInWindow;
}

void BotEngine::drawFaceFrame(BotExpression expression, uint32_t now) {
    GFXcanvas16 &c = *faceBuffer;

    c.fillScreen(C_BLACK);

    const float phase = now * 0.001f;

    /*
      Animation design:
      - The body never touches the canvas boundary.
      - Arms originate from the body, not the screen edge.
      - Eyes remain the visual anchor.
      - Motion is layered: idle breathing + gaze + expression action.
    */

    float action = 0.0f;

    if (expression == BotExpression::Happy ||
        expression == BotExpression::Playful) {
        action = (sinf(phase * 5.5f) + 1.0f) * 0.5f;
    } else if (expression == BotExpression::Angry) {
        action = (sinf(phase * 18.0f) + 1.0f) * 0.5f;
    } else if (expression == BotExpression::Eating) {
        action = (sinf(phase * 7.0f) + 1.0f) * 0.5f;
    } else if (expression == BotExpression::Drinking) {
        action = (sinf(phase * 3.2f) + 1.0f) * 0.5f;
    } else if (expression == BotExpression::Sleepy) {
        action = (sinf(phase * 1.2f) + 1.0f) * 0.5f;
    } else if (expression == BotExpression::Love) {
        action = (sinf(phase * 2.2f) + 1.0f) * 0.5f;
    }

    const float breathe = sinf(phase * 1.7f) * 1.4f;

    const int16_t cx = c.width() / 2;
    const int16_t cy = c.height() / 2 + static_cast<int16_t>(breathe);

    // Small character bounce. This makes the face and hands feel connected.
    int16_t bounce = 0;

    if (expression == BotExpression::Happy ||
        expression == BotExpression::Playful) {
        bounce = static_cast<int16_t>(sinf(phase * 5.5f) * 2.0f);
    } else if (expression == BotExpression::Angry) {
        bounce = static_cast<int16_t>(sinf(phase * 18.0f) * 1.0f);
    }

    const int16_t eyeW =
        clampv<int16_t>(c.width() * 25 / 100, 32, 64);
    const int16_t eyeH =
        clampv<int16_t>(eyeW * 118 / 100, 42, 76);

    const int16_t eyeGap =
        clampv<int16_t>(c.width() * 16 / 100, 24, 44);

    const int16_t eyeY =
        cy - c.height() * 5 / 100 + bounce;

    // Deliberate slight asymmetry = less "sticker" feeling.
    const int16_t leftX = cx - eyeGap + 1;
    const int16_t rightX = cx + eyeGap - 1;

    // Natural blink, ~4.6 seconds.
    const uint32_t blinkClock = now % 4600UL;
    float blink = 0.0f;

    if (blinkClock < 155UL) {
        const float t = blinkClock / 155.0f;
        blink = sinf(t * 3.1415926f);
    }

    drawEye(
        leftX, eyeY, eyeW, eyeH,
        expression, phase, true, blink, action
    );

    drawEye(
        rightX, eyeY + 1, eyeW, eyeH,
        expression, phase, false, blink, action
    );

    const int16_t mouthY =
        cy + c.height() * 25 / 100 + static_cast<int16_t>(breathe);

    drawMouth(cx, mouthY, expression, phase, action);

    drawArmsAndHands(expression, phase, action);
}

void BotEngine::drawEye(
    int16_t cx,
    int16_t cy,
    int16_t eyeW,
    int16_t eyeH,
    BotExpression expression,
    float phase,
    bool leftEye,
    float blinkAmount,
    float action
) {
    GFXcanvas16 &c = *faceBuffer;

    // Closed / sleepy eyes are intentionally simple and graphic.
    if (expression == BotExpression::Sleepy) {
        const int16_t y = cy + 10;
        drawQuadratic(
            cx - eyeW / 2,
            y,
            cx,
            y + 7,
            cx + eyeW / 2,
            y,
            C_EYE,
            5
        );

        // Tiny lower lash gives sleepy personality.
        c.drawFastHLine(cx - eyeW / 3, y + 5, eyeW * 2 / 3, C_EYE_EDGE);
        return;
    }

    float openness = 1.0f - blinkAmount * 0.95f;

    switch (expression) {
        case BotExpression::Love:
            openness *= 0.84f;
            break;

        case BotExpression::Happy:
        case BotExpression::Playful:
            openness *= 1.03f;
            break;

        case BotExpression::Bored:
            openness *= 0.70f;
            break;

        case BotExpression::Angry:
            openness *= 0.78f;
            break;

        case BotExpression::Thirsty:
            openness *= 0.86f;
            break;

        case BotExpression::Hungry:
            openness *= 0.94f;
            break;

        default:
            break;
    }

    const int16_t visibleH =
        clampv<int16_t>(
            static_cast<int16_t>(eyeH * openness),
            5,
            eyeH
        );

    const int16_t top = cy - visibleH / 2;
    const int16_t radius = min<int16_t>(eyeW / 3, visibleH / 2);

    // Previous-eye language restored: bright simple eye, dark pupil,
    // tiny highlight. No complex iris graphic.
    c.drawRoundRect(
        cx - eyeW / 2 - 1,
        top - 1,
        eyeW + 2,
        visibleH + 2,
        radius + 1,
        C_EYE_EDGE
    );

    c.fillRoundRect(
        cx - eyeW / 2,
        top,
        eyeW,
        visibleH,
        radius,
        C_EYE
    );

    if (visibleH >= eyeH / 3) {
        const float side =
            leftEye ? 0.0f : 0.18f;

        float gazeX =
            sinf(phase * 0.72f + side) * 2.7f +
            sinf(phase * 0.19f + side) * 1.2f;

        float gazeY =
            sinf(phase * 0.43f + side + 0.7f) * 1.8f;

        if (expression == BotExpression::Angry) {
            gazeY += 3.0f;
        }

        if (expression == BotExpression::Hungry ||
            expression == BotExpression::Eating) {
            gazeX += sinf(phase * 1.8f) * 2.0f;
        }

        const int16_t px = cx + static_cast<int16_t>(gazeX);
        const int16_t py = cy + static_cast<int16_t>(gazeY);
        const int16_t pupilR = clampv<int16_t>(visibleH / 5, 7, 12);

        c.fillCircle(px, py, pupilR + 2, C_CYAN);
        c.fillCircle(px, py, pupilR, C_PUPIL);
        c.fillCircle(
            px - pupilR / 3,
            py - pupilR / 3,
            max<int16_t>(2, pupilR / 4),
            C_WHITE
        );
    }

    // Eyelid / brow acting.
    if (expression == BotExpression::Angry) {
        const int16_t browY = top - 7;

        if (leftEye) {
            drawQuadratic(
                cx - eyeW / 2,
                browY - 2,
                cx,
                browY + 2,
                cx + eyeW / 2,
                browY + 9,
                C_RED,
                4
            );
        } else {
            drawQuadratic(
                cx - eyeW / 2,
                browY + 9,
                cx,
                browY + 2,
                cx + eyeW / 2,
                browY - 2,
                C_RED,
                4
            );
        }
    }

    // Love cheek dots.
    if (expression == BotExpression::Love) {
        c.fillCircle(
            cx - eyeW / 2 + 2,
            cy + eyeH / 2 - 2,
            3,
            C_PINK
        );
        c.fillCircle(
            cx + eyeW / 2 - 2,
            cy + eyeH / 2 - 2,
            3,
            C_PINK
        );
    }

    (void)action;
}

void BotEngine::drawMouth(
    int16_t cx,
    int16_t cy,
    BotExpression expression,
    float phase,
    float action
) {
    GFXcanvas16 &c = *faceBuffer;

    switch (expression) {
        case BotExpression::Calm:
            // Small relaxed mouth.
            drawQuadratic(
                cx - 8, cy,
                cx, cy + 2,
                cx + 8, cy,
                C_MOUTH, 2
            );
            break;

        case BotExpression::Happy:
        case BotExpression::Playful:
            drawQuadratic(
                cx - 9, cy,
                cx - 5, cy + 3,
                cx - 1, cy,
                C_PINK, 3
            );
            drawQuadratic(
                cx - 1, cy,
                cx + 3, cy + 3,
                cx + 7, cy,
                C_PINK, 3
            );
            break;

        case BotExpression::Love:
            drawQuadratic(
                cx - 8, cy,
                cx - 4, cy + 3,
                cx, cy,
                C_PINK, 2
            );
            drawQuadratic(
                cx, cy,
                cx + 4, cy + 3,
                cx + 8, cy,
                C_PINK, 3
            );
            break;

        case BotExpression::Hungry:
            c.fillRoundRect(cx - 12, cy - 8, 24, 17, 8, C_MOUTH);
            c.drawRoundRect(cx - 12, cy - 8, 24, 17, 8, C_AMBER);
            c.fillRoundRect(cx - 7, cy + 1, 14, 5, 2, C_PINK);
            break;

        case BotExpression::Eating: {
            const int16_t open =
                7 + static_cast<int16_t>((sinf(phase * 7.0f) + 1.0f) * 3.0f);

            c.fillRoundRect(cx - 10, cy - open / 2, 20, open, 7, C_MOUTH);

            // Chew mark.
            if (action > 0.5f) {
                c.fillCircle(cx + 7, cy - 1, 2, C_AMBER);
            }
            break;
        }

        case BotExpression::Thirsty:
            c.drawRoundRect(cx - 6, cy - 4, 12, 9, 4, C_CYAN);
            c.drawFastHLine(cx - 3, cy + 1, 6, C_CYAN);
            break;

        case BotExpression::Drinking:
            c.fillRoundRect(cx - 8, cy - 6, 16, 12, 5, C_MOUTH);
            c.drawFastHLine(cx - 5, cy - 1, 10, C_CYAN);
            break;

        case BotExpression::Angry:
            drawQuadratic(
                cx - 13, cy + 4,
                cx - 4, cy - 3,
                cx, cy + 1,
                C_RED, 3
            );
            drawQuadratic(
                cx, cy + 1,
                cx + 4, cy - 3,
                cx + 13, cy + 4,
                C_RED, 3
            );
            break;

        case BotExpression::Sleepy: {
            const int16_t yawn =
                5 + static_cast<int16_t>((sinf(phase * 1.5f) + 1.0f) * 4.0f);

            c.fillRoundRect(
                cx - 8,
                cy - yawn / 2,
                16,
                yawn,
                6,
                C_MOUTH
            );
            break;
        }

        case BotExpression::Bored:
            c.drawFastHLine(cx - 8, cy, 16, C_MUTED);
            break;
    }
}

void BotEngine::drawArmsAndHands(
    BotExpression expression,
    float phase,
    float action
) {
    GFXcanvas16 &c = *faceBuffer;
    const int16_t w = c.width();
    const int16_t h = c.height();
    const int16_t cx = w / 2;
    const int16_t cheekX = clampv<int16_t>(w * 32 / 100, 62, 86);
    const int16_t cheekY = h * 64 / 100 + static_cast<int16_t>(sinf(phase * 2.0f) * 1.5f);

    if (expression == BotExpression::Calm || expression == BotExpression::Bored) {
        drawOpenHand(cx - cheekX, cheekY, 1, C_EYE, 1.42f);
        drawOpenHand(cx + cheekX, cheekY, -1, C_EYE, 1.42f);
        return;
    }

    if (expression == BotExpression::Thirsty) {
        drawOpenHand(cx + w * 25 / 100, h * 63 / 100, -1, C_EYE, 1.3f);
        return;
    }

    if (expression == BotExpression::Happy ||
        expression == BotExpression::Playful) {
        drawOpenHand(cx - cheekX, cheekY, 1, C_EYE, 1.55f);
        drawOpenHand(cx + cheekX, cheekY, -1, C_EYE, 1.55f);

        if (expression == BotExpression::Playful) {
            drawSparkle(cx - cheekX - 14, cheekY - 17, 4, C_YELLOW);
            drawSparkle(cx + cheekX + 14, cheekY - 17, 3, C_PINK);
        }
        return;
    }

    if (expression == BotExpression::Love) {
        drawOpenHand(cx - 13, h * 66 / 100, 1, C_EYE, 1.35f);
        drawOpenHand(cx + 13, h * 66 / 100, -1, C_EYE, 1.35f);
        return;
    }

    if (expression == BotExpression::Hungry ||
        expression == BotExpression::Eating) {
        const int16_t handY = h * (expression == BotExpression::Hungry ? 66 : 70) / 100
            - static_cast<int16_t>(action * 4.0f);
        drawOpenHand(cx + w * 25 / 100, handY, -1, C_EYE, 1.35f);
        if (expression == BotExpression::Eating) {
            drawFood(cx + w * 13 / 100, handY - 13, C_AMBER);
        }
        return;
    }

    if (expression == BotExpression::Drinking) {
        const int16_t cupX = cx + 15;
        const int16_t cupY = h * 53 / 100;
        drawOpenHand(cupX + 21, cupY + 16, -1, C_EYE, 1.25f);
        drawCup(cupX, cupY);
        if (action > 0.45f) {
            c.fillCircle(cupX + 4, cupY - 9, 2, C_CYAN);
        }
        return;
    }

    if (expression == BotExpression::Sleepy) {
        const int16_t handY = h * 62 / 100 - static_cast<int16_t>(action * 3.0f);
        drawOpenHand(cx - cheekX, handY, 1, C_EYE, 1.45f);
        drawOpenHand(cx + cheekX, handY, -1, C_EYE, 1.45f);
        return;
    }

    if (expression == BotExpression::Angry) {
        const int16_t fistY = h * 70 / 100 + static_cast<int16_t>(sinf(phase * 18.0f) * 2.0f);
        drawFist(cx - cheekX, fistY, 1, C_EYE, 1.4f);
        drawFist(cx + cheekX, fistY, -1, C_EYE, 1.4f);
    }
}

void BotEngine::drawOpenHand(
    int16_t x,
    int16_t y,
    int8_t direction,
    uint16_t color,
    float scale
) {
    GFXcanvas16 &c = *faceBuffer;
    const int16_t palmW = max<int16_t>(24, static_cast<int16_t>(24 * scale));
    const int16_t palmH = max<int16_t>(16, static_cast<int16_t>(16 * scale));
    const int16_t fingerW = max<int16_t>(5, static_cast<int16_t>(5 * scale));
	const int16_t palmTop = y - palmH / 2;
	const int8_t fingerOffset[] = {-9, -3, 3, 9};
	const int8_t fingerLength[] = {9, 14, 12, 8};

	// Digits overlap the palm slightly so the hand reads as one connected shape.
	for (int8_t finger = 0; finger < 4; ++finger) {
		const int16_t fingerX = x + static_cast<int16_t>(fingerOffset[finger] * scale);
		const int16_t fingerH = max<int16_t>(7, static_cast<int16_t>(fingerLength[finger] * scale));
		const int16_t fingerY = palmTop - fingerH + 3;
        c.fillRoundRect(fingerX, fingerY, fingerW, fingerH + 4, fingerW / 2, color);
	}

	c.fillRoundRect(x - palmW / 2, palmTop, palmW, palmH, palmH / 2, color);
	c.drawRoundRect(x - palmW / 2, palmTop, palmW, palmH, palmH / 2, C_EYE_EDGE);

	const int16_t thumbX = x + direction * (palmW / 2 - 2);
	const int16_t thumbY = y + palmH / 5;
	c.fillRoundRect(thumbX - fingerW / 2, thumbY - 2, fingerW + 2, palmH / 2 + 4, fingerW / 2, color);
	c.drawRoundRect(thumbX - fingerW / 2, thumbY - 2, fingerW + 2, palmH / 2 + 4, fingerW / 2, C_EYE_EDGE);

	// Two short palm creases; keep the large silhouette doing most of the work.
	c.drawFastHLine(x - palmW / 4, y + palmH / 5, palmW / 3, C_EYE_EDGE);
	c.drawFastHLine(x - palmW / 5, y + palmH / 3, palmW / 4, C_EYE_EDGE);
}

void BotEngine::drawFist(
    int16_t x,
    int16_t y,
    int8_t direction,
    uint16_t color,
    float scale
) {
    GFXcanvas16 &c = *faceBuffer;

    const int16_t w = static_cast<int16_t>(20 * scale);
    const int16_t h = static_cast<int16_t>(18 * scale);

    c.fillRoundRect(
        x - w / 2,
        y - h / 2,
        w,
        h,
        7,
        color
    );

    // One thumb bump.
    c.fillCircle(
        x + direction * (w / 2 - 2),
        y + 2,
        max<int16_t>(3, static_cast<int16_t>(5 * scale)),
        color
    );

    c.drawFastHLine(
        x - w / 3,
        y - 2,
        w * 2 / 3,
        C_EYE_EDGE
    );

    c.drawFastHLine(
        x - w / 3,
        y + 3,
        w * 2 / 3,
        C_EYE_EDGE
    );
}

void BotEngine::drawPointHand(
    int16_t x,
    int16_t y,
    int8_t direction,
    uint16_t color
) {
    GFXcanvas16 &c = *faceBuffer;

    c.fillRoundRect(x - 8, y - 7, 16, 15, 6, color);

    // One directional finger = readable "reach" gesture.
    const int16_t tipX = x + direction * 14;

    c.fillRoundRect(
        min<int16_t>(x, tipX),
        y - 4,
        abs(tipX - x) + 1,
        8,
        4,
        color
    );

    c.fillCircle(tipX, y, 4, color);

    c.fillCircle(x + direction * 8, y + 6, 4, color);
}

void BotEngine::drawCup(int16_t x, int16_t y) {
    GFXcanvas16 &c = *faceBuffer;

    c.fillRoundRect(x, y, 18, 20, 4, C_CYAN);
    c.drawRoundRect(x, y, 18, 20, 4, C_WHITE);

    c.drawFastHLine(x + 3, y + 5, 12, C_WHITE);

    // Handle.
    c.drawRoundRect(x + 15, y + 5, 8, 10, 4, C_CYAN);

    // Water line.
    c.drawFastHLine(x + 3, y + 9, 12, C_TEAL);
}

void BotEngine::drawFood(int16_t x, int16_t y, uint16_t color) {
    GFXcanvas16 &c = *faceBuffer;

    c.fillCircle(x, y, 6, color);
    c.fillCircle(x + 7, y - 3, 5, C_ORANGE);
    c.fillCircle(x + 11, y + 4, 4, color);

    // Tiny stem.
    c.drawLine(x + 1, y - 6, x + 4, y - 10, C_TEAL);
}

void BotEngine::drawHeart(
    int16_t cx,
    int16_t cy,
    int16_t size,
    uint16_t color
) {
    GFXcanvas16 &c = *faceBuffer;

    const int16_t s = max<int16_t>(3, size);

    c.fillCircle(cx - s / 2, cy - s / 4, s / 2, color);
    c.fillCircle(cx + s / 2, cy - s / 4, s / 2, color);

    c.fillTriangle(
        cx - s,
        cy - 1,
        cx + s,
        cy - 1,
        cx,
        cy + s + 2,
        color
    );
}

void BotEngine::drawSparkle(
    int16_t cx,
    int16_t cy,
    int16_t size,
    uint16_t color
) {
    GFXcanvas16 &c = *faceBuffer;

    c.drawFastVLine(cx, cy - size, size * 2 + 1, color);
    c.drawFastHLine(cx - size, cy, size * 2 + 1, color);

    if (size >= 3) {
        c.drawPixel(cx - size + 1, cy - size + 1, color);
        c.drawPixel(cx + size - 1, cy + size - 1, color);
    }
}

void BotEngine::drawClock(const BotVisualState &state, uint32_t now) {
    if (now - lastInfoUpdate < 250) return;
    lastInfoUpdate = now;

    display->fillRect(
        12,
        display->height() / 2 - 37,
        display->width() - 24,
        78,
        C_BLACK
    );

    if (!state.timeSynced) {
        drawCentered(
            "TIME NOT SYNCED",
            display->height() / 2 - 8,
            2,
            C_AMBER
        );

        drawCentered(
            "Setup Larken to sync",
            display->height() / 2 + 20,
            1,
            C_WHITE
        );

        return;
    }

    struct tm timeInfo;

    if (!getLocalTime(&timeInfo, 5)) return;

    char clockText[9];
    char dateText[24];

    strftime(clockText, sizeof(clockText), "%H:%M:%S", &timeInfo);
    strftime(dateText, sizeof(dateText), "%a, %d %b %Y", &timeInfo);

    drawCentered(
        String(clockText),
        display->height() / 2 - 25,
        3,
        C_WHITE
    );

    drawCentered(
        String(dateText),
        display->height() / 2 + 16,
        1,
        C_CYAN
    );
}

void BotEngine::drawStatus(
    const BotVisualState &state,
    uint32_t now
) {
    if (now - lastInfoUpdate < 1000) return;
    lastInfoUpdate = now;

    const int16_t valuesY = 42;
    const int16_t rowHeight = 19;

    for (uint8_t row = 0; row < 8; ++row) {
        display->fillRect(
            display->width() / 2 - 4,
            valuesY + row * rowHeight - 1,
            display->width() / 2 + 2,
            15,
            C_BLACK
        );
    }

    const char *wifiState =
        state.wifiConnected
            ? "Connected"
            : (state.wifiConnecting
                ? "Connecting"
                : (state.wifiConfigured ? "Offline" : "Not set"));

    display->setTextSize(1);
    display->setTextColor(
        state.wifiConnected ? C_MINT : C_AMBER,
        C_BLACK
    );

    display->setCursor(display->width() / 2 - 2, valuesY);
    display->print(wifiState);

    display->setTextColor(C_WHITE, C_BLACK);

    display->setCursor(display->width() / 2 - 2, valuesY + rowHeight);
    display->print(
        shortValue(
            valueOr(state.wifiSsid, "-"),
            display->width() > 260 ? 22 : 15
        )
    );

    display->setCursor(
        display->width() / 2 - 2,
        valuesY + rowHeight * 2
    );
    display->print(state.setupApActive ? "Broadcasting" : "Off");

    display->setCursor(
        display->width() / 2 - 2,
        valuesY + rowHeight * 3
    );
    display->print(state.setupApActive ? "192.168.4.1" : "-");

    display->setCursor(
        display->width() / 2 - 2,
        valuesY + rowHeight * 4
    );
    display->print(state.timeSynced ? "Synced" : "Not synced");

    const uint8_t needs[] = {
        state.hunger,
        state.thirst,
        state.energy
    };

    for (uint8_t i = 0; i < 3; ++i) {
        display->setCursor(
            display->width() / 2 - 2,
            valuesY + rowHeight * (5 + i)
        );

        display->print(needs[i]);
        display->print('%');
    }
}

void BotEngine::drawGame(
    const BotVisualState &state,
    uint32_t now
) {
    if (now - lastFrameAt < 33) return;

    lastFrameAt = now;

    const int16_t centerX = display->width() / 2;
    const int16_t centerY = display->height() / 2;

    const int16_t radius =
        min<int16_t>(
            display->width() / 5,
            display->height() / 4
        ) - 4;

    if (previousGameX >= 0) {
        display->fillCircle(
            previousGameX,
            previousGameY,
            5,
            C_BLACK
        );
    }

    const float angle = now / 500.0f;

    const int16_t x =
        centerX + static_cast<int16_t>(cosf(angle) * radius);

    const int16_t y =
        centerY + static_cast<int16_t>(sinf(angle) * radius);

    display->fillCircle(x, y, 5, C_AMBER);

    previousGameX = x;
    previousGameY = y;

    if (lastGameScore != state.gameScore) {
        display->fillRect(
            0,
            display->height() - 25,
            display->width(),
            16,
            C_BLACK
        );

        drawCentered(
            "Score: " + String(state.gameScore),
            display->height() - 22,
            1,
            C_MINT
        );

        lastGameScore = state.gameScore;
    }

    ++framesInWindow;
}

void BotEngine::drawSetup(
    const BotVisualState &state,
    uint32_t now
) {
    if (now - lastInfoUpdate < 500) return;
    lastInfoUpdate = now;

    display->fillRect(
        18,
        125,
        display->width() - 36,
        42,
        C_BLACK
    );

    drawCentered(
        state.setupApActive
            ? "SETUP NETWORK READY"
            : "SETUP NETWORK FAILED",
        128,
        1,
        state.setupApActive ? C_MINT : C_RED
    );

    const String status =
        state.wifiConnected
            ? "Home Wi-Fi connected"
            : valueOr(state.wifiMessage, "Waiting for Wi-Fi");

    drawCentered(
        shortValue(status, display->width() > 260 ? 36 : 25),
        148,
        1,
        state.wifiConnected ? C_MINT : C_AMBER
    );
}

void BotEngine::drawDeveloper(
    const BotVisualState &state,
    uint32_t now
) {
    if (now - lastInfoUpdate < 500) return;
    lastInfoUpdate = now;

    const int16_t y = 45;

    for (uint8_t row = 0; row < 8; ++row) {
        display->fillRect(
            display->width() / 2 - 4,
            y + row * 19 - 1,
            display->width() / 2 + 2,
            15,
            C_BLACK
        );
    }

    const String values[] = {
        String(state.uptimeSeconds) + " s",
        String(fps) + " fps",
        String(ESP.getFreeHeap()) + " bytes",
        String(state.framebufferBytes) + " bytes",
        String(state.touch1Active ? "ON" : "off") +
            " / " +
            (state.touch2Active ? "ON" : "off"),
        state.wifiConnected
            ? "Connected"
            : (state.wifiConnecting ? "Connecting" : "Offline"),
        state.setupApActive ? "Active" : "Off",
        String(state.hunger) + " / " +
            String(state.thirst) + " / " +
            String(state.energy)
    };

    for (uint8_t row = 0; row < 8; ++row) {
        display->setTextSize(1);
        display->setTextColor(C_WHITE, C_BLACK);
        display->setCursor(
            display->width() / 2 - 2,
            y + row * 19
        );
        display->print(values[row]);
    }
}

void BotEngine::drawCentered(
    const String &text,
    int16_t y,
    uint8_t size,
    uint16_t color
) {
    display->setTextSize(size);
    display->setTextColor(color, C_BLACK);

    int16_t x1, y1;
    uint16_t width, height;

    display->getTextBounds(
        text,
        0,
        y,
        &x1,
        &y1,
        &width,
        &height
    );

    display->setCursor(
        (display->width() - width) / 2,
        y
    );

    display->print(text);
}

void BotEngine::drawQuadratic(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t color,
    uint8_t thickness
) {
    GFXcanvas16 &c = *faceBuffer;

    int16_t previousX = x0;
    int16_t previousY = y0;

    for (uint8_t step = 1; step <= 12; ++step) {
        const float t = step / 12.0f;
        const float inv = 1.0f - t;

        const int16_t x =
            static_cast<int16_t>(
                inv * inv * x0 +
                2.0f * inv * t * x1 +
                t * t * x2
            );

        const int16_t y =
            static_cast<int16_t>(
                inv * inv * y0 +
                2.0f * inv * t * y1 +
                t * t * y2
            );

        for (uint8_t o = 0; o < thickness; ++o) {
            c.drawLine(
                previousX,
                previousY + o,
                x,
                y + o,
                color
            );
        }

        previousX = x;
        previousY = y;
    }
}
