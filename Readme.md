# Larken Project Notes

## Hardware Overview

Current development hardware:

- ESP32-C3 SuperMini
- ILI9341 2.4-inch SPI display module, 240x320, with resistive-touch and microSD header pins
- Two TTP223 touch modules
- Momentary power button
- BC547 NPN transistors and one STN3906 V3 PNP transistor
- Planned battery: 1S LiPo, nominally around 1000-1500 mAh
- Charger previously identified as a TP5100 board; exact board variant is not confirmed

The current firmware has an ILI9341 default and an ST7789 compile-time option. Confirm the future ST7789 breakout's actual supported VCC, backlight voltage/current, and dimensions before connecting it.

## Wi-Fi And Antenna

The ESP32-C3 SuperMini typically uses a PCB antenna integrated at one end of the module; it does not require an external antenna connector. Keep that end clear of breadboard contacts, metal, display wiring, batteries, and ground planes. The Wi-Fi network disappearing when the board is inserted into a breadboard was isolated to the physical setup; the radio test worked when the board was removed.

The firmware currently limits Wi-Fi transmit power to 8.5 dBm as a power/range experiment. This reduces range. Remove or raise the `WiFi.setTxPower` setting if more range is needed.

On first boot with no saved home network, the firmware starts the open setup AP `L.A.R.K.E.N Setup` in AP-only mode. The portal is at `http://192.168.4.1`; it stores bot name, owner name, timezone offset, and home Wi-Fi credentials. The Wi-Fi engine owns Preferences, the setup server, station reconnects, and NTP time.

Wi-Fi transmit bursts can expose weak power rails or breadboard contacts. Use short power leads and a stable supply. The ESP32-C3 GPIOs are 3.3 V only; never apply a 5 V display rail to a GPIO.

## Firmware Architecture

Open the root `Larken_VS.ino` in Arduino IDE. Keep these files together in the same sketch folder; the IDE compiles the `.cpp` tabs automatically:

- `WifiEngine.h/.cpp`: preferences, setup AP, web portal, station connection, and NTP
- `BotEngine.h/.cpp`: all display screens and face rendering
- `PowerButton.h/.cpp`: debounced momentary-button click/hold events
- `Larken_VS.ino`: touch handling, simulated needs, button actions, and engine orchestration

The BotEngine uses a bounded RGB565 face buffer in RAM and sends that region to the TFT. It avoids clearing the full display on every animation frame. It allocates smaller face-buffer sizes if heap is limited. The serial heartbeat reports frame rate, heap, and framebuffer size. Actual sustained 30 fps still needs to be measured on the assembled board.

Install these libraries through Arduino IDE Library Manager:

- Adafruit GFX Library
- Adafruit ILI9341
- Adafruit ST7735 and ST7789 Library

Select an ESP32-C3 board. Set `LARKEN_USE_ST7789` to `1` and set native panel dimensions when moving to an ST7789 module.

## Touch And Power Button Behavior

T1 controls interaction: single tap interacts, double tap toggles setup mode, triple tap toggles the developer HUD, and a long hold triggers an annoyed reaction. T2 cycles modes with a tap and toggles setup mode when held.

The power button is GPIO5, active-low, using `INPUT_PULLUP`. Hold it for about 3 seconds while running to open the power menu:

- Single click: ESP32 deep sleep; a later button press wakes it
- Double click: reboot without clearing user settings
- Triple click: open reset confirmation

On the reset-confirm screen, single click cancels and double click clears only the `larken` Preferences namespace, then starts setup again. It does not erase firmware or perform an ESP32 chip erase. Short button clicks on the regular companion screen do nothing.

A normally-open momentary button connects GPIO5 to GND when pressed. An external 10 kOhm pull-up from GPIO5 to 3.3 V can improve deep-sleep wake reliability; do not connect the button to 5 V.

## Sleep And Peripheral Power

Deep sleep stops the ESP32 CPU and radio and the firmware sends the TFT controller sleep commands. It does **not** physically disconnect the ESP32, TFT VCC, TFT LED/backlight, or TTP223 VCC. The current power-button implementation therefore does not yet provide an all-loads-off sleep state.

A momentary button can wake the ESP32 from deep sleep because the ESP32 remains powered. A truly disconnected ESP32 requires a hardware latch/power controller to turn itself back on.

### Recommended Peripheral-Rail Option

To preserve the current software power menu and GPIO5 wake behavior, use two logic-enabled high-side load switches:

1. A 5 V switch between the boost-converter 5 V output and both TFT VCC and LED pins, only after confirming that both pins are rated for the same switched 5 V rail on the chosen module.
2. A 3.3 V switch between ESP32 3V3 and both TTP223 VCC pins.

Keep the ESP32 and GPIO5 button on the unswitched rail so the MCU can wake and control the switches. Add a pulldown on each enable input so peripheral rails default off during reset/deep sleep. Before cutting display power, send the display sleep command, stop SPI, and place TFT/touch signal GPIOs in a non-backfeeding state. On wake, keep enables off until pin states are configured, then turn on the rails and initialize the peripherals.

A purchasable example is the [Pololu Mini MOSFET Slide Switch LV, product 2810](https://www.pololu.com/product/2810). It is specified for 1.8-16 V input, has an external ON control input that turns on above approximately 1 V, and is rated for about 3 A continuous at 55 C. For GPIO control, leave its onboard slide switch OFF. Add a 100 kOhm pulldown from ON to GND. One module is needed per rail. Verify the converter and each rail's maximum current; a switch does not increase the boost converter's capacity.

### Whole-Battery Switch Option

The [Pololu Mini Pushbutton Power Switch LV, product 2808](https://www.pololu.com/product/2808) is specified for 2.2-16 V operation and latching momentary-button control. Its hardware pushbutton behavior is different from the firmware's 3-second menu gesture, so choosing it requires redesigning the user interaction. Its charger must connect directly to the protected battery/charger path rather than being interrupted by the switched load output.

For a custom PCB, TI lists [TPS22918](https://www.ti.com/product/TPS22918), a 1-5.5 V, 2 A load switch, and [TPS22919](https://www.ti.com/product/TPS22919), a 1.6-5.5 V, 1.5 A protected load switch. These are small surface-mount ICs, not ready-to-wire modules; follow the datasheet layout and capacitor requirements.

The BC547 and STN3906 V3 currently on hand have not been wired as load switches. Do not use a GPIO as a supply pin or connect a transistor directly as an unverified battery switch. The exact TFT rail currents and the STN3906 package must be verified before designing around those discrete parts.

## Battery And Charging Safety

No battery ADC, automatic cutoff, or charger control has been implemented. Do not use firmware or a transistor to interrupt a LiPo charger at 4.1 V. Charge termination must come from a correctly configured CC/CV charger for the exact cell chemistry, series count, target voltage, and allowed charge current. A BMS is secondary protection, not a precision charge-voltage controller.

For a 1S LiPo rated for 4.20 V, 4.20 V is the standard full-charge target. A 4.10 V target can trade capacity/runtime for cycle life, but requires a charger specified or configured for 4.10 V. Do not assume a generic TP5100 breakout supports 4.10 V; verify its exact schematic and datasheet, 1S/2S setting, and charge-current configuration. Do not change feedback/current-setting components by guesswork.

For a compact enclosure, use a known cell with documented charge-temperature limits and a charger with suitable cell-temperature monitoring (NTC). Keep the Larken system load off while charging, as planned, and provide appropriate thermal conditions. Do not charge an unidentified pouch cell in a sealed enclosure or unattended. Standardize the cell specification and supplier for production even if capacity varies.

## STN3906 V3 Datasheet Note

The KODENSHI/AUK [STN3906 datasheet](https://www.alldatasheet.com/datasheet-pdf/pdf/568302/KODENSHI/STN3906.html) identifies the listed through-hole STN3906 as a TO-92 PNP transistor with pin numbers 1=Emitter, 2=Base, 3=Collector. Absolute maximum ratings include VCEO=-40 V, IC=-100 mA, and PC=625 mW at Ta=25 C. These are stress limits, not recommended continuous operating targets; derate for enclosure temperature. The datasheet lists the marking as `STN3906` and does not explain a trailing `V3` code. Verify the physical part/package before relying on that pinout.

Design any display switch for the display manufacturer's maximum VCC current plus maximum LED/backlight current, with margin, and verify base drive and transistor dissipation. Do not use 100 mA absolute maximum as a load target. A future ST7789 breakout may have different current requirements.

## Serial And Radio Diagnostics

Use Serial Monitor at 115200 baud. Select the correct ESP32-C3 COM port. For native USB boards, enable **Tools > USB CDC On Boot > Enabled**, upload, and reopen the port that appears after reset. Some SuperMini revisions use a USB-to-UART bridge; choose the port and CDC setting that match the board.

The isolated `wifi_radio_test/wifi_radio_test.ino` sketch advertises `ESP32-C3-SuperMini-AP` with password `yourpassword123` and prints status every two seconds. It is a separate Arduino sketch and does not initialize the TFT or touch modules. The main firmware advertises `L.A.R.K.E.N Setup` on first boot if no credentials are saved.
