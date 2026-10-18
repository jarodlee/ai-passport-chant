"""Independent audioop oracle for the microphone encoder, including chunk state."""
import ctypes, subprocess, tempfile, random, struct, audioop
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 lib=Path(d)/'codec.so'
 subprocess.run(['cc','-shared','-fPIC','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'main'),str(root/'main/chant_adpcm.c'),'-o',str(lib)],check=True)
 c=ctypes.CDLL(str(lib)); f=c.chant_encode_sample; f.argtypes=[ctypes.c_int,ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int)]
 rng=random.Random(1973)
 for values in ([0]*1024,[-32768,32767]*512,[rng.randint(-32768,32767) for _ in range(16000)]):
  predictor=ctypes.c_int(0);index=ctypes.c_int(0);output=bytearray()
  for a,b in zip(values[::2],values[1::2]):output.append((f(a,ctypes.byref(predictor),ctypes.byref(index))<<4)|f(b,ctypes.byref(predictor),ctypes.byref(index)))
  expected,state=audioop.lin2adpcm(struct.pack('<'+'h'*len(values),*values),2,None)
  assert bytes(output)==expected
  assert (predictor.value,index.value)==state
print('Microphone ADPCM encoder against audioop: PASS')
