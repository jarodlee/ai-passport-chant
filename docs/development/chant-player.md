[简体中文](chant-player.zh_CN.md)

# Chant Player v2

A dedicated green-and-gold 240 × 320 application replaces the hardware-test UI.
It boots to its home screen while automatically playing the Heart Sutra long
version offline at 80% volume. Five recordings are embedded: an 8:30 Great
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
  local date/time (CST, `--` before SNTP sync). UP/DOWN select Wi-Fi scan, QR code,
  Music, Anqing Weather, Wi-Fi, Voice volume, or Cindy; the selected entry is drawn in the
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

Home displays a Chinese reminder to bring the top of the phone near the rear
label. The owner confirmed that an iPhone notification appears and currently
opens `folotoy.cn`. The requested replacement is `https://www.lxj.ai`; writing
and retesting this destination with the phone are still pending. The board's
NTAG213 is a **passive phone-readable tag**, without an MCU-facing BSP API.
Firmware cannot scan another NFC tag, detect a phone scan, change the tag, or
report successful recognition. This build does not alter tag data or `cardid`.

If the tag is writable, use NFC Tools on the iPhone: Write → Add a record →
URL / URI, enter `https://www.lxj.ai`, confirm, then select Write. Hold the top of
the phone near the device's rear label until writing completes. Close the app
and scan again to check the notification's destination. See the
[official NFC Tools instructions](https://www.wakdev.com/en/knowledge-base/how-to-guides/how-to-write-a-link-url-on-an-nfc-chip.html).
If the app reports a read-only tag, check its lock status; USB firmware cannot
undo a permanent read-only lock on the tag.

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

Cindy is the last Home entry. During a spoken reply, pressing OK interrupts playback and retains the new press; recording starts after the bounded audio download task exits, without reopening the music codec in between. The HTTP body read timeout is 250 ms and transient EAGAIN results are retried. Old-job network cleanup is deferred until after the next recording, so it does not delay opening the microphone. The current route lookup and initial TLS setup can still take longer if interrupted before playback begins; the short body timeout applies after response headers. UP cancels both the current exchange and any queued recording. The Cindy page no longer says that both devices must share a network. Hold OK until recording appears, speak for up to eight seconds, release to send; UP returns after the transaction. Music pauses during the exchange and resumes at the same position. A short recording is rejected. Status text uses the fixed subset, while each reply is also drawn from a bitmap
rendered on the Mac and served at `/v1/jobs/<id>/text`. The Mac normalizes the
Tingting PCM peak before sending it, and the device applies the stored voice
volume when playing replies.

The paired, local-only `ai-passport-bridge` package uses bundled ARM64 whisper.cpp (tiny multilingual, CPU/Accelerate) for ASR, Cindy's documented ordinary Tasks API for a short answer in an isolated, visible conversation task, and macOS Tingting for 16 kHz speech. The user's quick-answer channel failed its live test, so the user explicitly selected the existing Cindy task channel. The plugin creates its own visible task with an isolated workspace and default ask permissions; it allows read-only web search/page lookup for requested current information, requires source/date context and an honest failure message when verification is unavailable, and prohibits local files, commands, messages and other sessions. Ordinary conversation does not require a search. This is a request scope, not an OS sandbox guarantee. The plugin has a resident native Node worker; its status tool starts/stops HTTP 18765 and authenticated UDP discovery 18766. It creates private, temporary audio files and deletes them after 150 seconds or stop. It requires an awake Mac running Cindy. LAN discovery requires a shared trusted LAN without AP/client isolation; the optional HTTPS origin supports separate networks. No cloud ASR/TTS credentials are needed; the answer prompt follows the configured task model. The task can be opened in Cindy to view the conversation.

A random per-build pairing key lives in ignored `main/chant_bridge_local.h` and the private plugin `pairing.json`; never commit or publish either, or share the paired binaries/package. UDP challenge/response uses HMAC-SHA256. HTTP requires the pairing key, private IPv4 peers, bounded request bodies and one active job. LAN HTTP is unencrypted and assumes a trusted network. The microphone is opened only after an explicit held button, records at most 64 KB IMA ADPCM, and is owned by the audio worker. New firmware requests `audio/x-passport-ima-adpcm` at the existing `/audio` endpoint. Bridge v1.4 returns a 12-byte PVA1 header (magic, LE32 16000 Hz, LE32 sample count) followed by mono IMA ADPCM (zero predictor/index, high nibble first), up to 90 seconds. This reduces speech transfer size to roughly one quarter of PCM. A network-only producer feeds a 24 KiB compressed queue; the audio owner prebuffers 16 KiB (about two seconds), then decodes into 512-sample PCM chunks. On an underrun it refills before resuming. Cancellation joins the producer before freeing its queue. Header, length and duration checks reject truncated or mismatched audio. Old firmware omitting this Accept header still receives PCM. A bridge update alone therefore enables search, while the playback improvement requires the matching firmware. The bridge restarts its 150-second ready-job retention after synthesis. Weather refresh is deferred during a transaction; NVS writes still require audio quiescence or fail closed.

The temporary provisioning hotspot is now open (no password), still limited to one client and ten minutes. This local firmware includes a private, ignored first-connect configuration; it saves the chosen network after successful connection and preserves a setup marker when forgetting a network, so forgetting does not silently restore the preset. Never publish this local firmware or its credential header.

## Offline QR display and home shortcuts

The home order is Wi-Fi scan, QR code, music library, Anqing weather, Wi-Fi
connection, voice volume, and Cindy conversation. The first item opens the existing
scan page and immediately requests a scan; OK rescans. The network submenu also
retains its scan shortcut. Moving up from the first home item reaches Cindy.

The scan header draws a Wi-Fi icon (three arcs and a dot) beside its scan title,
without an icon font or a bitmap. Its widget is hidden on other pages. Large-font generation
now derives headings and home labels from the application source, and host tests
compare those strings with the generated glyph map. This fixes the four missing
glyph boxes previously shown by the scan page and covers the saved-network title
as well. Changing the text encoding or hiding font placeholders is not used as a
workaround. Drawing the icon avoids linking a whole font for one symbol.

The QR page displays a local black-on-white I1 image without networking. Long
press OK or double-click UP to return home; ordinary keys do not start a voice
transaction or change playback. The title moves above the image and the reply
canvas/body are hidden. The bitmap preserves four quiet modules, uses integer
scaling, and fits within 224 square pixels between the title and footer.

Prepare a private image with `python3 tools/prepare_chant_qr.py /path/to/qr.jpg`
(OpenCV with QR encoder support is required). The tool decodes the source,
re-encodes its exact destination without the decorative logo, and verifies the
packed display bitmap decodes to the same value. It never prints that value.
The supplied image produces a 196-pixel square and 4,908 bytes of indexed data.
Its source/preview `assets/images/chant-qr-local*` and generated
`main/chant_qr_local.h` are Git-ignored. On 2026-10-20 the owner authorized the
supplied QR for public distribution. Public builds use `assets/images/chant-qr.h`
when no local override exists. Regenerate the published asset with
`python3 tools/prepare_chant_qr.py --public assets/images/chant-qr.jpg`.
Wi-Fi credentials, pairing keys and personalized firmware remain private.
The feature does not rewrite the NFC tag.

Host navigation and bitmap decoding do not validate LCD scanning. Device checks
must cover phone scanning, title/battery/footer clearance, returning to other
pages without a leftover image, Chinese glyphs, and scan-on-entry behavior.

## Recovering from voice upload failures

Bridge v1.5 and the matching firmware add recovery for intermittent HTTPS failures:

- Each recording has a random request ID and at most two upload attempts. The
  bridge replays the original job for the same source and ID without duplicate
  recognition. Authentication errors, rate limits and busy jobs are not retried.
- JSON responses declare Content-Length. The device reads fragmented bodies,
  tolerates temporary read timeouts within an overall deadline and size limit,
  and honors cancellation. Diagnostics record the stage, HTTP status and free
  heap, without credentials or speech. Separate screen messages cover pairing,
  denied access, busy jobs, service failures and timeouts.
- After recording, unused ADPCM buffer capacity is released when possible before
  TLS setup. Failed jobs join cancelled jobs in deferred cleanup after the next
  recording, so cleanup does not delay opening the microphone.
- New questions may replace same-source failed jobs; matching firmware explicitly
  permits replacing an unplayed ready reply. Recognizing/thinking jobs stay busy.
  Delivered audio remains available for 150 seconds for retries while admitting
  the next question. Phase transitions refresh the job retention deadline.

The firmware also reuses the connection within each transaction instead of
performing a new TLS handshake for every poll. Consecutive public-network device
conversations completed, and the user confirmed audible replies. These checks
cover the tested connection and installed bridge v1.5; they do not establish
reliability on every network or validate the uninstalled v1.6 search policy.
Prolonged weak-network operation and cancellation recovery require further
device validation.

## Multiple saved networks

The app stores up to six Wi-Fi profiles as one versioned, validated NVS blob, migrating the former successful single-network setting. It starts with the last successful profile; two failed attempts move to the next profile, cycling through the list and backing off when none connects. It does not interrupt a working connection to chase other networks. The private local setup merges the three user-provided location profiles once; SSID matches update credentials without duplicate entries. Newly entered phone credentials are added only after the matching network obtains an IP. A full list reports a save failure instead of silently evicting an entry. The existing forget action clears all saved networks while retaining the setup marker, so private defaults do not return on reboot. Credentials are never included in maintained docs, logs or commits.

## Remote voice connection

The device first attempts authenticated LAN discovery for up to three seconds.
If no bridge responds, it uses `CHANT_BRIDGE_REMOTE_URL` from the ignored
`main/chant_bridge_local.h`. Leave it undefined to keep LAN-only operation.
The value must be an HTTPS DNS origin such as `https://voice.example.com`,
with no path, port, query, credentials or fragment. The same pairing key
protects upload, job polling, reply text and speech. A transaction keeps its
chosen route; it never uploads the same recording again through another route.

Remote connections use the existing certificate bundle, verify hostname and
certificate validity, reject redirects, and require the SNTP clock to be ready.
Remote failures do not downgrade to public HTTP. One sequentially owned HTTP
client is reused for upload, polling, reply text and speech within a transaction.
Only fully consumed successful responses keep the connection; failures, truncated
bodies and server-requested closes discard the connection before retrying. Upload
entity/idempotency headers are removed before GET requests. The audio downloader
borrows the client only after polling/text finish and is joined before cleanup;
the whole session is released before the next microphone recording. Logs count
requests and actual connections without recording URLs, keys or speech. Cloudflare Tunnel may
terminate HTTPS and proxy only this voice service to `http://127.0.0.1:18765`;
the bridge sees a loopback peer and still enforces its pairing key and job
ownership. Do not expose the Cindy application API or a whole local network.

A fixed hostname, healthy tunnel, awake Mac and running Cindy bridge are all
required for remote use. The hostname and private pairing configuration stay
out of Git. The public repository build has no remote origin and uses a dummy
pairing key until locally configured. Build checks do not prove remote service
availability or microphone/playback behavior on hardware.

The sanitized companion source, private setup requirements and task-permission
recovery guidance are in [voice bridge source](chant-bridge-source.md).
