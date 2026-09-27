// Adafruit Monster Eyes -- textured eye from flashed assets, ESP32-2424S012C
//
// Third example for the DIYmalls 1.28" ESP32-C3 round display board:
//
//   OneEye_ESP32_2424S012C         storage off, round pupil, solid colour
//   OneEye_ESP32_2424S012C_Demon   procedural demon: slit pupil, fiery iris
//   OneEye_ESP32_2424S012C_Textured (this one) real M4_Eyes artwork
//
// This sketch sets NO appearance values of its own. Everything -- radii, colours,
// eyelids, textures -- comes from config.eye, so switching eyes means changing
// EYE_FOLDER and reflashing the asset image, not editing code.
//
// The ESP32-C3 has no USB-OTG, only USB-Serial-JTAG, so the CIRCUITPY
// drag-and-drop route in the library README does not exist here. Assets must be
// written into the FAT partition directly.
//
// BOARD SETTINGS
//   USB CDC On Boot:   Enabled
//   Partition Scheme:  "No OTA (2MB APP/2MB FATFS)"
//
// BUILDING AND FLASHING THE ASSET IMAGE (macOS; adjust for your host)
//
//   Partition geometry comes from the core's noota_ffat.csv -- read it rather
//   than trusting these numbers on a different core version:
//     ffat, data, fat, 0x210000, 0x1E0000     (1966080 bytes = 1920 KiB)
//
//   A 1920 KiB volume with 512-byte sectors has too few clusters for FAT16, so
//   it must be FAT12. SdFat reads FAT12 because the Adafruit fork ships
//   FAT12_SUPPORT 1; plain upstream SdFat defaults it to 0.
//
//     newfs_msdos -F 12 -S 512 -c 1 -C 1920k fat.img
//     hdiutil attach -imagekey diskimage-class=CRawDiskImage \
//                    -mountpoint /tmp/eyefat fat.img
//     cp -R <M4_Eyes>/eyes/hazel /tmp/eyefat/
//     hdiutil detach /tmp/eyefat
//     esptool --chip esp32c3 --port <port> write-flash 0x210000 fat.img
//
//   Texture paths inside config.eye are opened verbatim against the volume ROOT
//   ("hazel/iris.bmp"), so keep the folder name and put the folder at the root.
//
// Never use GPIO 18 or 19: they are the C3's USB D-/D+ lines.

#include <Adafruit_Monster_Eyes.h>

// Which eye folder on the FAT volume to load.
#define EYE_FOLDER "hazel"

#define TFT_SCK 6
#define TFT_MOSI 7
#define TFT_CS 10
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 3 // Backlight, active HIGH

#define TFT_W 240
#define TFT_H 240

Eyes_ESPLCD display(EYES_ESPLCD_GC9A01A, TFT_W, TFT_H, TFT_SCK, TFT_MOSI,
                    TFT_CS, TFT_DC, TFT_RST);
Adafruit_Monster_Eyes eyes(&display);

void setup() {
  Serial.begin(115200);
  delay(3000); // USB CDC drops writes until the host opens the port
  eyes.setVerbose(Serial);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  display.setSPISpeed(40000000);
  display.setOrientation(true /* invert */);

  eyes.setStorageEnabled(true);
  eyes.setConfigFile("/" EYE_FOLDER "/config.eye");

  // begin() unmounts the volume when it is done unless asked not to, which would
  // make storageMounted() below report false even on a successful load. Keeping
  // it mounted also leaves loadEye() usable, so a different eye folder can be
  // switched to at runtime without a reboot.
  eyes.keepStorageMounted(true);

  // Drive mode cannot work on this chip, and the default safe-mode pin is
  // GPIO 0 -- the touch controller's interrupt line here, not a free button.
  eyes.setDriveModeEnabled(false);

  if (!eyes.begin()) {
    Serial.print("Monster Eyes failed: ");
    Serial.println(eyes.errorString());
    while (1)
      delay(1000);
  }

  // The "[4] media" block above is the authoritative answer: each line either
  // names a bitmap or says "none specified". This is just the headline.
  if (eyes.storageMounted()) {
    Serial.println("assets: FAT volume mounted, " EYE_FOLDER
                   "/config.eye loaded -- textures listed above");
  } else {
    Serial.println("assets: NO FAT volume, so nothing was loaded and this is "
                   "the built-in default eye, not " EYE_FOLDER ".");
    Serial.println("  The partition exists but is probably unformatted -- see "
                   "the header comment in this sketch.");
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
