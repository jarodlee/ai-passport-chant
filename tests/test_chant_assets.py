"""Cross-check codec streaming against an independent CPython audioop oracle."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    src = Path(temp)/'codec.c'
    src.write_text('''#include "chant_adpcm.h"
#include <stdio.h>
int main(void) {
 unsigned char data[8192]; short pcm[511];
 for (int i=0;i<8192;i++) data[i]=(unsigned char)i;
 chant_decoder_t state={0};
 for (size_t pos=0;pos<16384;) {
  size_t n=16384-pos; if(n>511)n=511;
  chant_decode(&state,data,pos,n,pcm);
  for(size_t i=0;i<n;i++) { unsigned short v=(unsigned short)pcm[i]; putchar(v&255);putchar(v>>8); }
  pos+=n;
 }
 return 0;
}''')
    exe=Path(temp)/'codec'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
                    '-I'+str(root/'main'),str(src),str(root/'main/chant_adpcm.c'),'-o',str(exe)],check=True)
    decoded=subprocess.check_output([str(exe)])
    oracle=json.loads((root/'tests/chant-codec-oracle.json').read_text())
    assert hashlib.sha256(decoded).hexdigest() == oracle['decoded_sha256']
    assert len(decoded)==32768
source=''.join(p.read_text() for p in (root/'main').glob('chant_*.c'))
required=set(re.findall(r'[\u3400-\u9fff]',source))
inventory=set((root/'assets/fonts/chant-glyphs.txt').read_text())
assert required <= inventory, required-inventory
for size in (18,28):
    font=(root/f'assets/fonts/chant_font_{size}.c').read_text()
    offsets=re.search(r'unicode_list\[\] = \{([^}]+)\}',font).group(1)
    glyphs={chr(int(n)+32) for n in offsets.split(',')}
    expected = required if size==18 else set((root/'assets/fonts/chant-title-glyphs.txt').read_text().strip())
    if size==28:
        actual_titles=''.join(re.findall(r'\btitle\s*=\s*"([^"]*)"',source))
        actual_titles+=''.join(re.findall(r'\{"([^"]+)",PAGE_',source))
        expected |= set(actual_titles)
    assert expected <= glyphs
    assert set(chr(i) for i in range(32,127)) <= glyphs
    assert '\u9f98' not in glyphs
    assert '.bpp=4' in font and '.bitmap_format=0' in font
for name in ('dabei', 'guanyin', 'auspicious', 'xinjing', 'xinjinglong'):
    metadata=json.loads((root/f'assets/music/{name}-metadata.json').read_text())
    asset=(root/f'assets/music/{name}.adpcm').read_bytes()
    assert len(asset)==(metadata['samples']+1)//2
    assert hashlib.sha256(asset).hexdigest()==metadata['adpcm_sha256']
    assert metadata['sample_rate']==11025
    playlist=(root/'main/chant_playlist.c').read_text()
    assert str(metadata['samples'])+'u' in playlist
print('Chant decoder oracle, complete playlist integrity and Chinese font coverage: PASS')
