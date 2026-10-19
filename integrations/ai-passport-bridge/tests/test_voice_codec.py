"""Compare the bridge's wire audio with an independent IMA ADPCM oracle."""
import audioop
import json
from pathlib import Path
import random
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
rng = random.Random(1973)
with tempfile.TemporaryDirectory() as temp:
    pcm_file = Path(temp) / 'voice.pcm'
    for count in [1, 2, 511, 512, 16001, 16000 * 10]:
        values = [rng.randint(-32768, 32767) for _ in range(count)]
        pcm = struct.pack('<' + 'h' * count, *values)
        pcm_file.write_bytes(pcm)
        script = "const fs=require('fs'),c=require('./node/voice-codec.cjs');process.stdout.write(c.encodeReply(fs.readFileSync(process.argv[1])));"
        wire = subprocess.check_output(['node', '-e', script, str(pcm_file)], cwd=root)
        assert wire[:4] == b'PVA1'
        assert struct.unpack('<II', wire[4:12]) == (16000, count)
        assert len(wire) == 12 + (count + 1)//2
        # audioop only emits complete bytes for odd sample counts.
        expected, _ = audioop.lin2adpcm(pcm, 2, None)
        assert wire[12:12+len(expected)] == expected
        if count % 2:
            assert wire[-1] & 15 == 0
        decoded = subprocess.check_output(['node', '-e', "const c=require('./node/voice-codec.cjs');process.stdout.write(c.decode(Buffer.from(process.argv[1],'hex')));", wire[12:].hex()], cwd=root)
        oracle, _ = audioop.adpcm2lin(wire[12:], 2, None)
        assert decoded == oracle
    script = "const assert=require('assert'),c=require('./node/voice-codec.cjs');for(const n of [0,1,2880002])assert.throws(()=>c.encodeReply(Buffer.alloc(n)),/AUDIO_LIMIT/);"
    subprocess.check_call(['node', '-e', script], cwd=root)
print('Bridge reply protocol/encoder/decoder vs audioop: PASS')
