[简体中文](https://github.com/jarodlee/ai-passport-chant/blob/v1.0.0-chant/docs/development/release/chant-release.zh_CN.md)

# v1.0.0-chant — AI Passport Chant Player

First public firmware release for FoloToy AI Passport (ESP32-C3, 8 MB Flash).

## What's new

- Five offline chants; the long Heart Sutra excerpt starts automatically at 80% music volume.
- Wi-Fi scan first, QR display second, Cindy conversation last; Wi-Fi icon and Chinese title fixes.
- Saved networks, temporary password-free phone provisioning, Anqing weather, Wi-Fi name and time on home.
- Push-to-talk Cindy, interrupt-and-ask, buffered speech playback, separate reply volume and remote upload recovery.
- Owner-approved offline QR included. NFC shows a phone-placement reminder; firmware does not change the passive tag.

## Build and configuration

Activate ESP-IDF 5.5.3, then run `./tools/validate.sh` for the full gate, or
`./tools/validate.sh --firmware` for build and merged-image verification only.
The downloadable image is built from a clean GitHub checkout. It contains no
owner Wi-Fi passwords, private pairing key or remote bridge origin. Configure
Wi-Fi after flashing. Cindy needs a separately prepared bridge and a matching
private firmware build; the public placeholder key is not an active pairing.
See the [bridge setup](https://github.com/jarodlee/ai-passport-chant/blob/v1.0.0-chant/docs/development/chant-bridge-source.md).
The bridge 1.6 search-first source is included but has not passed live-device acceptance.

## Flash and use

Download `FoloToy-AI-Passport-full.bin` and `SHA256SUMS` below. Verify with
`sha256sum -c SHA256SUMS` (Linux) or `shasum -a 256 -c SHA256SUMS` (macOS).
Connect the device by USB and open the [browser flasher](https://ai-passport.folotoy.cn/tools/web-flasher/)
in a compatible desktop browser. Write this **merged full image at `0x0`**.
A full-image write can reset saved settings; do not use it merely to update an
already personalized device while preserving its pairing. No full-chip erase is required.

Use UP/DOWN to select and OK to open. Hold OK or double-click UP to return home
(in Cindy, holding OK records speech). Wi-Fi → phone setup opens a temporary
`Passport-XXXX` hotspot; connect without a password and visit `http://192.168.4.1`
to configure a 2.4 GHz network. On Wi-Fi scan, OK rescans.

## Validation and limits

The release pipeline runs the complete host/static/build gate and verifies the
merged image before publishing. This clean public variant is not hardware-tested;
the related personalized build has passed startup and voice checks. Phone scanning
of the QR on the LCD and weak-network endurance remain unverified. Audio source
attribution and rights are recorded in the [asset guide](https://github.com/jarodlee/ai-passport-chant/blob/v1.0.0-chant/assets/README.md);
the code license does not grant rights to third-party recordings.
