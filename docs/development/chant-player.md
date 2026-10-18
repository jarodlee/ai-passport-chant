[简体中文](chant-player.zh_CN.md)

# Chant Player v2

A dedicated green-and-gold 240 × 320 application replaces the hardware-test UI.
It boots to its home screen while automatically playing the Great Compassion
Mantra offline at 40% volume. Five recordings are embedded: an 8:30 Great
Compassion Mantra section (510.000 s) from the owner-selected YouTube source,
Guanyin Bodhisattva recitation (37.329 s), Disaster Eradicating Auspicious
Spirit Mantra (76.643 s), the Heart Sutra (305.850 s) from a second
owner-selected YouTube source, and a 4:50 excerpt of a third YouTube Heart
Sutra recitation (290.000 s). Audio is 11.025 kHz mono IMA ADPCM; the larger
application partition holds all five tracks.
Original MP3 files remain local. Playback selections and music volume reset at reboot; the
Cindy reply volume is stored separately and survives reboot.

## Pages and controls

- Home: the top line shows the connected Wi-Fi SSID and the next line shows
  local date/time (CST, `--` before SNTP sync). UP/DOWN select Music, Anqing
  Weather, Wi-Fi, Cindy, or Voice volume; the selected entry is drawn in the
  large title font between ▶ and ◀.
- Music: UP/DOWN choose a track or single/playlist repeat; OK selects it.
- Player: OK pauses/resumes; UP/DOWN change music volume by 5%, bounded to 0–90%.
- Hold OK, or double-click UP, on any page: home. Leaving provisioning closes its hotspot.
  The dedicated power key is a hardware latch and is not readable by the firmware.
- Weather: OK requests an update. Anqing is fixed at 30.54 N, 117.06 E, with
  Open-Meteo attribution, temperature in Celsius, WMO condition and humidity.
  The page shows the observation time plus a fresh-update label (`just now` or
  `N minutes ago`), and
  staleness is judged from that observation timestamp in wall-clock time.
- Wi-Fi: the page shows the connected SSID and IP in at most seven lines.
  Choose phone setup, the saved-network picker, a nearby-network scan, or
  forget this application's saved networks. During provisioning, OK closes the
  hotspot. The picker lists up to six saved SSIDs, marks the active one, and
  connects on OK; the choice is persisted. The scan page lists up to eight
  nearby APs by signal strength with SSID, dBm, channel and `*` for encryption;
  OK rescans. SSIDs are shown as printable ASCII (other bytes become `?`)
  because the UI font is a fixed subset. Forget requires a second OK; moving
  selection cancels confirmation.
- Cindy: push-to-talk with spoken replies. The Mac renders each reply as a
  224×168 one-bit bitmap and the device draws it on an LVGL indexed canvas, so
  arbitrary Chinese text needs no full glyph set in flash. The bitmap belongs
  only to the Cindy page and is cleared when leaving it; UP during a reply
  cancels playback and returns home immediately. Fixed status lines cover
  recording, discovery, upload, thinking and playback.
- Voice volume: UP/DOWN change the Cindy reply volume by 10% between 20% and
  100%, stored in NVS namespace `chant_cfg`; OK plays a short test beep. The
  music volume is independent and still resets at reboot.

The battery stays top-right, with `--%` for unavailable readings. Backlight dims
after 60 seconds and returns on a button event; no automatic sleep interrupts
playback. Existing demo UI sources remain for reference and host tests but are
excluded from the application build.

## NFC hardware limitation

Home reminds the phone user to approach the NFC area. The board's NTAG213 is a
**passive phone-readable tag**, without an MCU-facing BSP API. Firmware cannot
scan another NFC tag, detect a phone scan, change the tag, or report successful
recognition. This build does not alter tag data or the `cardid` partition.

## Phone Wi-Fi setup

Open Wi-Fi → phone setup. Connect the phone to the displayed `Passport-XXXX`
open hotspot without a password, then manually open
`http://192.168.4.1`. There is no captive-portal/DNS interception. Supply a
2.4 GHz SSID (1–32 UTF-8 bytes) and either an empty password for an open network,
or an 8–63 printable ASCII WPA2 passphrase. WPA3-only networks are not supported.
The current implementation scans nearby networks on demand (see the Wi-Fi page)
but does not list an SSID on the provisioning web form.

The embedded web form serves only to AP clients. The request check accepts a
client on 192.168.4.0/24, a socket local address of 192.168.4.1, or a
`Host: 192.168.4.1` header, because some phone portal browsers hide the socket
address. The page and error pages declare UTF-8; Chinese text is additionally
HTML-entity encoded (with JS `\u` escapes) so it still renders when a portal
browser ignores the charset. The hotspot allows one client, closes 15 seconds after a successful newly provisioned connection,
or after ten minutes, and can be stopped explicitly. Wrong passwords are not
persisted. Submitted credentials are stored as one NVS blob only after a matching
SSID has an IP; reboots reconnect automatically. Short retries change to a
30-second interval after repeated failures. Failed provisioning retains the
previous successfully stored credentials for the next boot. A save failure is
shown; it does not erase the partition. Forget clears the saved-network keys in `chant_wifi` while retaining the setup
marker; other NVS data and card identity are preserved. The unused storage
partition was removed when the application partition was enlarged.

Weather requests use HTTPS with the ESP certificate bundle, an SNTP wall clock,
8-second I/O timeouts, a bounded 4 KB response and observation age checks.
Entering the weather page requests one fresh update immediately; successful requests also recur every 15 minutes, failures retry after a minute, and the page shows an update-in-progress state.
Data older than 30 minutes is explicitly stale. Refresh failures retain the
last successful values with an error message. No SSID or arbitrary downloaded
text is rendered by the fixed Chinese font subset. Phone/browser API checks
are separate from actual ESP networking acceptance.

## Architecture and checks

The BSP is unchanged. A single audio worker owns the codec, chunk decoder and
playback model, feeding 512-sample PCM buffers. Buttons enqueue events only.
The UI takes the BSP LVGL lock, renders only changed labels, and reads copies
of network/audio state. A network worker owns Wi-Fi, settings and HTTPS; HTTP
handlers validate and enqueue provisioning requests. Saving or forgetting
credentials asks the audio owner to sleep the codec first and then resume at
the same position. Resuming calls `bsp_audio_wake()` before resetting the
format; without the wake, the codec stays marked asleep and reopening fails,
which surfaced as an audio-unavailable message after switching a saved network. Bluetooth is disabled to reserve internal RAM for Wi-Fi/TLS.

`./tools/validate.sh` runs repository checks, portable navigation, repeat,
volume/recovery, Wi-Fi input limits, WMO/value/expiry tests, codec oracle checks,
all three asset hashes, Chinese glyph coverage, and an isolated ESP-IDF 5.5.3
build with merged-image validation. Font generation additionally checks bitmap
pixels against Pillow/FreeType. Build identity and actual device results must
be recorded in the local delivery record; a successful build is not hardware
acceptance. Unperformed checks: all page rendering, keys, 11.025 kHz audio pitch and
quality, full-track/playlist wrap, pause/resume, backlight, battery, phone setup,
wrong-password recovery, saved-network reboot/forget, HTTPS/SNTP weather,
concurrent network/audio heap and timing. Speech conversation remains deferred.

See [asset provenance and regeneration](../../assets/README.md). Public audio
availability is not redistribution permission; this remains a local-only build.

## Preserve the measured device layout

| Partition | Offset | Size |
| --- | --- | --- |
| nvs | 0x9000 | 0x6000 |
| phy_init | 0xF000 | 0x1000 |
| factory | 0x10000 | 0x7E0000 |
| cardid | 0x7F0000 | 0x4000 |

Use the verified archive's bootloader at `0x0`, partition table at `0x8000`, and
application at `0x10000` for compatible segmented flashing after user approval.
No whole-chip erase. The merged image is for a deliberate complete refresh and
can reset NVS/PHY bytes. A successful build does not authorize flashing.

The 2026-10-18 repartition kept `nvs` and `phy_init` at their offsets, enlarged
`factory` over the former `storage` region, and moved `cardid` to `0x7F0000`.
Before flashing, `nvs`, `cardid`, `storage` and the old partition table were read
back to local backups; `cardid` was restored byte-for-byte at its new offset and
verified by hash. Wi-Fi profiles and the voice volume survived in `nvs`.

## Push-to-talk Cindy bridge (v3)

Open Cindy from Home. Hold OK until recording appears, speak for up to eight seconds, release to send; UP returns after the transaction. Music pauses during the exchange and resumes at the same position. A short recording is rejected. Status text uses the fixed subset, while each reply is also drawn from a bitmap
rendered on the Mac and served at `/v1/jobs/<id>/text`. The Mac normalizes the
Tingting PCM peak before sending it, and the device applies the stored voice
volume when playing replies.

The paired, local-only `ai-passport-bridge` package uses bundled ARM64 whisper.cpp (tiny multilingual, CPU/Accelerate) for ASR, Cindy's documented ordinary Tasks API for a short answer in an isolated, visible conversation task, and macOS Tingting for 16 kHz speech. The user's quick-answer channel failed its live test, so the user explicitly selected the existing Cindy task channel. The plugin creates its own visible task with an isolated workspace and default ask permissions; it prompts the model to answer without tools or files and never reads other sessions. This is a request scope, not an OS sandbox guarantee. The plugin has a resident native Node worker; its status tool starts/stops HTTP 18765 and authenticated UDP discovery 18766. It creates private, temporary audio files and deletes them after 150 seconds or stop. It requires an awake Mac running Cindy on the same trusted LAN, without AP/client isolation. No cloud ASR/TTS credentials are needed; the answer prompt follows the configured task model. The task can be opened in Cindy to view the conversation.

A random per-build pairing key lives in ignored `main/chant_bridge_local.h` and the private plugin `pairing.json`; never commit or publish either, or share the paired binaries/package. UDP challenge/response uses HMAC-SHA256. HTTP requires the pairing key, private IPv4 peers, bounded request bodies and one active job. LAN HTTP is unencrypted and assumes a trusted network. The microphone is opened only after an explicit held button, records at most 64 KB IMA ADPCM, and is owned by the audio worker. Responses stream bounded PCM chunks directly to the same audio owner. Weather refresh is deferred during a transaction; NVS writes still require audio quiescence or fail closed.

The temporary provisioning hotspot is now open (no password), still limited to one client and ten minutes. This local firmware includes a private, ignored first-connect configuration; it saves the chosen network after successful connection and preserves a setup marker when forgetting a network, so forgetting does not silently restore the preset. Never publish this local firmware or its credential header.

## Multiple saved networks

The app stores up to six Wi-Fi profiles as one versioned, validated NVS blob, migrating the former successful single-network setting. It starts with the last successful profile; two failed attempts move to the next profile, cycling through the list and backing off when none connects. It does not interrupt a working connection to chase other networks. The private local setup merges the three user-provided location profiles once; SSID matches update credentials without duplicate entries. Newly entered phone credentials are added only after the matching network obtains an IP. A full list reports a save failure instead of silently evicting an entry. The existing forget action clears all saved networks while retaining the setup marker, so private defaults do not return on reboot. Credentials are never included in maintained docs, logs or commits.
