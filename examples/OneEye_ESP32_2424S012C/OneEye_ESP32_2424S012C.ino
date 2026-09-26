// Adafruit Monster Eyes -- one eye on an ESP32-2424S012C round display board
//
// Sold as "DIYmalls 1.28 inch ESP32-C3 IPS Round Display ESP32-2424S012C_I"
// and similar. ESP32-C3, GC9A01A 240x240 round IPS, CST816 capacitive touch.
//
// Three things make this board different from the Feather/EYESPI wiring in
// OneEye_GC9A01A:
//
//   1. The backlight is a plain GPIO (3), active HIGH, and nothing in the
//      library touches it. Leave it low and the panel renders into the dark.
//   2. GPIO 18 and 19 are the ESP32-C3's USB D-/D+ lines. Do not use them for
//      anything -- claiming them as GPIO tears down the USB port, and the board
//      then only reappears in download mode by holding BOOT while plugging in.
//   3. The C3 has no USB-OTG, only USB-Serial-JTAG, so there is no CIRCUITPY
//      drive to copy eye assets onto. The eye runs from built-in defaults plus
//      whatever this sketch sets. See setStorageEnabled() below.
//
// Board settings: USB CDC On Boot: Enabled, and a partition scheme with room
// for the sketch -- "Huge APP" works, or "No OTA (2MB APP/2MB FATFS)" if you
// want a FAT partition to read assets from.

#include <Adafruit_Monster_Eyes.h>

// Display wiring, fixed on this board.
#define TFT_SCK 6
#define TFT_MOSI 7
#define TFT_CS 10
#define TFT_DC 2
#define TFT_RST -1 // Panel reset is tied to the board's
#define TFT_BL 3   // Backlight, active HIGH

#define TFT_W 240
#define TFT_H 240

// esp_lcd drives the controller directly, so there is no panel object.
Eyes_ESPLCD display(EYES_ESPLCD_GC9A01A, TFT_W, TFT_H, TFT_SCK, TFT_MOSI,
                    TFT_CS, TFT_DC, TFT_RST);
Adafruit_Monster_Eyes eyes(&display);

void setup() {
  Serial.begin(115200);
  delay(3000); // USB CDC drops writes until the host opens the port
  eyes.setVerbose(Serial);

  // Backlight first, so any error below is actually readable on the panel.
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // 40MHz is proven good on this panel. 80MHz is what the vendor demo asks for
  // and is worth trying, but start where it is known to work.
  display.setSPISpeed(40000000);
  display.setOrientation(true /* invert */);

  // No CIRCUITPY drive on this chip. Skipping the mount avoids a misleading
  // "no FAT filesystem" error on a board that could never have had one.
  // Flash a FATFS partition scheme and set this true if you add assets.
  eyes.setStorageEnabled(false);

  // Drive mode is already off by default where MONSTER_EYES_USB_MSC is 0, but
  // being explicit documents why: the default safe-mode pin is GPIO 0, which on
  // this board is the touch controller's interrupt line, not a free button.
  eyes.setDriveModeEnabled(false);

  if (!eyes.begin()) {
    Serial.print("Monster Eyes failed: ");
    Serial.println(eyes.errorString());
    while (1)
      delay(1000);
  }

  Serial.printf("free heap %lu, largest block %lu\n",
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMaxAllocHeap());
}

void loop() {
  eyes.animate();

  static uint32_t last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    Serial.printf("%.0f fps\n", eyes.frameRate());
  }
}
