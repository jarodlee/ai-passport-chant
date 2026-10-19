<p align="right"><a href="chant-bridge-source.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Voice bridge source

The companion source is in [`integrations/ai-passport-bridge`](../../integrations/ai-passport-bridge/).
It contains the Cindy plugin manifest, sandbox controller, native Node worker,
panel, icon and protocol tests. Version 1.5 adds idempotent uploads, fixed-length
JSON and terminal-job recovery; the 1.6 source makes factual questions search
public information before answering. Greetings need no lookup. Failed lookups
must be disclosed rather than replaced by an invented search result. Version
1.6 was packaged and host-tested but has not been installed or accepted on the
device. The installed bridge remains 1.5; firmware connection reuse has passed
actual HTTPS device conversations.

## Prepare a private installation

This is source, not a ready-to-install package. Pairing credentials, packaged
plugins, local logs, native executables and the ASR model are excluded. Supply
a trusted macOS ARM64 whisper.cpp CLI as `native/whisper-cli` and its compatible
multilingual tiny model as `native/ggml-tiny.bin`, and retain the accompanying
license. Make the executable runnable before packing. The bridge also needs
macOS Tingting and Python 3 with Pillow for text rendering.

`pairing.example.json` contains a non-secret placeholder. Generate a new random
16-byte key encoded as 32 hexadecimal characters for private `pairing.json`,
and put the same key in the firmware's ignored `main/chant_bridge_local.h`.
Do not use the placeholder for an active installation, reuse another device's
key, commit private configuration or distribute a paired binary/package. The
local firmware header can also configure a private HTTPS origin. See
[the remote voice connection](chant-player.md#remote-voice-connection).

Use Cindy's Forge validation/packing flow on the prepared private source;
installation is a separate explicitly authorized action. Updating this Git
source does not install a plugin or flash a device.

The plugin uses its own visible Cindy task and preserves its configured model.
The tested task permission is the default `ask`. Setting the task to full access
or an unapproved higher permission can cause `Task permission exceeds plugin
dispatch policy`; restore default permissions through Cindy's task interface.
Public web lookups worked at the default permission. Host permission prompts
still apply; the search-first prompt does not bypass them or grant local writes.

## Run host checks

From the companion source directory:

```sh
node --check main.js
node tests/test_voice_prompt.cjs
node tests/test_voice_jobs.cjs
node tests/test_voice_http.cjs
python3 tests/test_voice_codec.py
```

The codec oracle needs Python 3.12 or earlier (`audioop`). Node checks require
Node 18 or later. Tests use synthetic audio and mocked Cindy/ASR/TTS interfaces;
they do not require private pairing data, the CLI/model, or a running Cindy.
They do not establish live search quality, audio quality or public-network
reliability. The firmware's complete gate is documented in
[build and test](engineering/build-and-test.md).
