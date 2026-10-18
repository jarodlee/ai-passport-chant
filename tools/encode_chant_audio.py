#!/usr/bin/env python3
"""Encode complete recordings as local-only 11.025 kHz IMA ADPCM (Python 3.9)."""
import audioop
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
assets=root/'assets/music'
tracks=[
    ('dabei','大悲咒','dabei.mp3','https://www.youtube.com/watch?v=HQR_jVqWE_Y (excerpt 0:00-08:30)'),
    ('guanyin','观音菩萨圣号','guanyin.mp3','https://www.cttbusa.org/recitation/gwanyinchant.mp3'),
    ('auspicious','消灾吉祥神咒','auspicious.mp3','https://www.cttbusa.org/recitation/auspicious_mantra.mp3'),
    ('xinjing','心经','xinjing.mp3','https://www.youtube.com/watch?v=j6YKyMu2Kfc'),
    ('xinjinglong','心经长版','xinjinglong.mp3','https://www.youtube.com/watch?v=ujr7nB-NTcM (excerpt 0:00-04:50)'),
]
records=[]
for name,title,original,url in tracks:
    with tempfile.TemporaryDirectory() as tmp:
        pcm_path=Path(tmp)/'audio.pcm'
        subprocess.run(['ffmpeg','-nostdin','-y','-v','error','-i',str(assets/original),
                        '-ac','1','-ar','11025','-af','alimiter=limit=0.8:level=0',
                        '-f','s16le',str(pcm_path)],check=True)
        pcm=pcm_path.read_bytes()
    if len(pcm)//2 % 2: pcm+=b'\0\0'
    encoded,_=audioop.lin2adpcm(pcm,2,None)
    (assets/(name+'.adpcm')).write_bytes(encoded)
    record={'name':name,'title':title,'source':url,'sample_rate':11025,'samples':len(pcm)//2,
        'duration_seconds':len(pcm)/2/11025,'codec':'IMA ADPCM; high nibble first; initial predictor/index 0',
        'original_sha256':hashlib.sha256((assets/original).read_bytes()).hexdigest(),
        'adpcm_sha256':hashlib.sha256(encoded).hexdigest(),'adpcm_bytes':len(encoded)}
    (assets/(name+'-metadata.json')).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    records.append(record)
(assets/'dabei_audio.h').write_text('#pragma once\n#define CHANT_SAMPLE_RATE 11025\n#define CHANT_TOTAL_SAMPLES '+str(records[0]['samples'])+'u\n')
code='#include "chant_playlist.h"\n'
for rec in records:
    n=rec['name']
    code+=f'extern const uint8_t {n}_start[] asm("_binary_{n}_adpcm_start");\n'
    code+=f'extern const uint8_t {n}_end[] asm("_binary_{n}_adpcm_end");\n'
code+='const chant_track_t chant_tracks[CHANT_TRACK_COUNT] = {\n'
for rec in records:
    n=rec['name']
    code+=f'    {{"{rec["title"]}", {n}_start, {n}_end, {rec["samples"]}u}},\n'
code+='};\n'
(root/'main/chant_playlist.c').write_text(code)
print('Complete tracks:', [(r['title'],r['duration_seconds'],r['adpcm_bytes']) for r in records])
