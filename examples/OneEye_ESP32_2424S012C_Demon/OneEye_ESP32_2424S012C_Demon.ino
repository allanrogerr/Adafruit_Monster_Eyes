// Adafruit Monster Eyes -- textured demon eye on an ESP32-2424S012C
//
// Same board as OneEye_ESP32_2424S012C (DIYmalls 1.28" ESP32-C3 round display,
// GC9A01A 240x240, CST816 touch). That example disables storage and renders a
// plain solid-colour eye. This one goes after the real thing.
//
// TWO WAYS THIS RUNS, and it tells you which over serial:
//
//   1. Textured. A FAT partition holding config.eye and the iris/sclera/eyelid
//      BMPs is mounted, and the eye looks like M4_Eyes' demon. Requires the
//      asset image to be flashed -- see BUILDING THE ASSET IMAGE below.
//
//   2. Procedural fallback. No FAT partition, or no config.eye on it. The
//      settings below still give a recognisable demon: a vertical SLIT pupil and
//      a fiery iris, using the library's own stock demon proportions. This is
//      what you get out of the box, and it is worth seeing first.
//
// The ESP32-C3 has no USB-OTG -- only USB-Serial-JTAG -- so there is no
// CIRCUITPY drive to drag files onto. The asset image has to be flashed.
//
// BOARD SETTINGS
//   USB CDC On Boot: Enabled
//   Partition Scheme: "No OTA (2MB APP/2MB FATFS)"   <-- the FATFS part matters
// With a partition scheme that has no FAT partition, storage simply fails to
// mount and you get case 2. That is not an error you need to chase.
//
// BUILDING THE ASSET IMAGE
//   1. Get an eye folder -- config.eye plus its BMPs -- from Adafruit's M4_Eyes
//      project. The "demon" folder is the one this example is tuned for.
//   2. Put config.eye and the BMPs at the ROOT of a staging directory, or set
//      setConfigFile() below to wherever you put them.
//   3. Build a FAT image the size of the partition (2 MB for noota_ffat) with
//      mkfatfs, then flash it at the partition's offset. Read the offset out of
//      the partition table rather than assuming it:
//        esptool --chip esp32c3 --port <port> read-flash 0x8000 0xC00 pt.bin
//        gen_esp32part.py pt.bin
//      then
//        esptool --chip esp32c3 --port <port> write-flash <ffat_offset> fat.img
//
// EXPECT A LONG FIRST FRAME. Measured on this board: the slit-pupil polar map
// takes about 21 seconds to build at size 240, against 2 seconds for the round
// pupil of the sibling example. The screen stays dark for that whole time and
// the board looks hung. It is not -- watch the serial log, and the "Tables built
// in ..." line is the one you are waiting for. Steady state is ~30 fps either
// way, so this is startup cost only. Shrink setIrisRadius() if you need to boot
// faster.
//
// Never use GPIO 18 or 19 on this board: they are the C3's USB D-/D+ lines and
// claiming them tears down the USB port.

#include <Adafruit_Monster_Eyes.h>

#define TFT_SCK 6
#define TFT_MOSI 7
#define TFT_CS 10
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 3 // Backlight, active HIGH

#define TFT_W 240
#define TFT_H 240

// RGB565, native endian; the backend swaps on the way out.
#define DEMON_IRIS 0xF8E0   // Fiery orange-red
#define DEMON_SCLERA 0x2800 // Dark, faintly blood-warm
#define DEMON_PUPIL 0x0000
#define DEMON_BACK 0x0000
#define DEMON_EYELID 0x0000

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

  // Look for assets. config.eye, if found, overrides everything set below.
  eyes.setStorageEnabled(true);
  eyes.setConfigFile("/config.eye");

  // Drive mode cannot work on this chip, and the default safe-mode pin is
  // GPIO 0 -- the touch controller's interrupt line here, not a free button.
  eyes.setDriveModeEnabled(false);

  // --- procedural demon, used when no config.eye is found ---------------
  //
  // A NEGATIVE slit pupil radius means "derive the stock demon proportion from
  // the display size" (0.4167 * 240 = 100 px). Zero would mean a round pupil,
  // which is what the other example gets. eyeRadius and irisRadius are left at
  // their auto values, which are also the stock demon proportions.
  eyes.setSlitPupilRadius(-1);

  eyes.setIrisColor(DEMON_IRIS);
  eyes.setScleraColor(DEMON_SCLERA);
  eyes.setPupilColor(DEMON_PUPIL);
  eyes.setBackColor(DEMON_BACK);
  eyes.setEyelidColor(DEMON_EYELID);

  // Wide dilation range makes the slit breathe, which reads as alive.
  eyes.setPupilRange(0.15f, 0.55f);

  // A slow iris rotation. Costs nothing on a solid colour but comes into its
  // own once a textured iris is loaded. Comment out for a static eye.
  eyes.setIrisSpin(6.0f);

  eyes.setTracking(true);
  eyes.setAutoBlink(true);
  eyes.setAutoGaze(true);

  if (!eyes.begin()) {
    Serial.print("Monster Eyes failed: ");
    Serial.println(eyes.errorString());
    while (1)
      delay(1000);
  }

  // Say plainly which of the two modes is on screen, so a solid-colour eye is
  // never mistaken for a broken texture loader.
  if (eyes.storageMounted()) {
    Serial.println("assets: FAT volume mounted -- textured if config.eye was "
                   "found (see the media list above)");
  } else {
    Serial.println("assets: no FAT volume -- procedural demon (slit pupil, "
                   "solid fiery iris)");
    Serial.println("  for textures, flash an asset image; see the header "
                   "comment in this sketch");
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
