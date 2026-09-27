#!/usr/bin/env bash
#
# Build and optionally flash a FAT asset image for boards that cannot export a
# CIRCUITPY drive -- i.e. anything without USB-OTG, such as the ESP32-C3/C6/H2.
# On those chips the library README's drag-and-drop route does not exist, so the
# eye folder has to be written into the FAT partition directly.
#
# macOS only: it uses newfs_msdos and hdiutil. On Linux use mkfs.vfat and a
# loopback mount, or esp32_fatfsimage.
#
# Usage:
#   extras/make_eye_ffat_image.sh <M4_Eyes/eyes dir> <eye folder> [port]
#
# Example:
#   extras/make_eye_ffat_image.sh ~/Adafruit_Learning_System_Guides/M4_Eyes/eyes \
#       hazel /dev/cu.usbmodem101
#
# With no port it writes ./eye_ffat.img and stops, so you can flash it yourself.
#
# Texture paths inside config.eye are opened verbatim against the volume root
# ("hazel/iris.bmp"), so the folder keeps its name and sits at the root.

set -euo pipefail

EYES_DIR=${1:?usage: $0 <M4_Eyes/eyes dir> <eye folder> [port]}
EYE=${2:?usage: $0 <M4_Eyes/eyes dir> <eye folder> [port]}
PORT=${3:-}

# Geometry of the "No OTA (2MB APP/2MB FATFS)" scheme's ffat partition. CHECK
# THIS against your core's partition CSV rather than trusting it -- a different
# scheme or core version moves it:
#   esptool --chip esp32c3 --port <port> read-flash 0x8000 0xC00 pt.bin
#   gen_esp32part.py pt.bin
FFAT_OFFSET=${FFAT_OFFSET:-0x210000}
FFAT_BYTES=${FFAT_BYTES:-1966080} # 0x1E0000
SECTOR=512
IMG=${IMG:-eye_ffat.img}
MNT=$(mktemp -d)

SRC="$EYES_DIR/$EYE"
[ -d "$SRC" ] || { echo "no such eye folder: $SRC" >&2; exit 1; }
[ -f "$SRC/config.eye" ] || { echo "no config.eye in $SRC" >&2; exit 1; }

echo "eye        : $EYE ($(du -sh "$SRC" | cut -f1))"
echo "image      : $IMG  ($FFAT_BYTES bytes)"

# A 1920 KiB volume with 512-byte sectors has too few clusters for FAT16, so it
# must be FAT12. SdFat reads FAT12 only because the Adafruit fork ships
# FAT12_SUPPORT 1; plain upstream SdFat defaults that to 0.
rm -f "$IMG"
python3 -c "open('$IMG','wb').write(b'\x00' * $FFAT_BYTES)"

# newfs_msdos refuses a plain file ("Cannot get partition offset"), so attach the
# image as a raw device first and format that.
DEV=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount "$IMG" | head -1 | awk '{print $1}')
trap 'hdiutil detach "$DEV" >/dev/null 2>&1 || true; rm -rf "$MNT"' EXIT
newfs_msdos -F 12 -S $SECTOR -c 1 -v EYES "${DEV/disk/rdisk}"
hdiutil detach "$DEV" >/dev/null

DEV=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -mountpoint "$MNT" "$IMG" | head -1 | awk '{print $1}')
COPYFILE_DISABLE=1 cp -R "$SRC" "$MNT/"
# Keep macOS housekeeping off the volume; it wastes clusters and confuses nobody
# usefully.
rm -rf "$MNT/.fseventsd" "$MNT/.Spotlight-V100" "$MNT/.TemporaryItems" 2>/dev/null || true
find "$MNT" -name '._*' -delete 2>/dev/null || true

echo "contents   :"
find "$MNT" -type f | sed "s|$MNT|  |"
sync
hdiutil detach "$DEV" >/dev/null
trap 'rm -rf "$MNT"' EXIT

if [ -z "$PORT" ]; then
  echo
  echo "wrote $IMG -- flash it with:"
  echo "  esptool --chip esp32c3 --port <port> write-flash $FFAT_OFFSET $IMG"
  exit 0
fi

echo
echo "flashing to $PORT at $FFAT_OFFSET"
esptool --chip esp32c3 --port "$PORT" --before default-reset --after hard-reset \
  write-flash -z "$FFAT_OFFSET" "$IMG"

echo
echo "Set EYE_FOLDER to \"$EYE\" in the sketch, then watch the [4] media block"
echo "on the serial log: each line should name a bitmap, not 'none specified'."
