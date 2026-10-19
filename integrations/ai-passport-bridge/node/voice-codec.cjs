'use strict';
const steps=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767],changes=[-1,-1,-1,-1,2,4,6,8];
function decode(data){let pcm=Buffer.alloc(data.length*4),predictor=0,index=0;for(let i=0;i<data.length*2;i++){let n=i%2?data[i>>1]&15:data[i>>1]>>4,s=steps[index],d=(s>>3)+(n&4?s:0)+(n&2?s>>1:0)+(n&1?s>>2:0);predictor=Math.max(-32768,Math.min(32767,predictor+(n&8?-d:d)));index=Math.max(0,Math.min(88,index+changes[n&7]));pcm.writeInt16LE(predictor,i*2);}return pcm;}
function encodeReply(pcm){
 if(!Buffer.isBuffer(pcm)||!pcm.length||pcm.length%2||pcm.length>16000*2*90)throw Error('AUDIO_LIMIT');
 const samples=pcm.length/2,out=Buffer.alloc(12+Math.ceil(samples/2));
 out.write('PVA1');out.writeUInt32LE(16000,4);out.writeUInt32LE(samples,8);
 let predictor=0,index=0;
 for(let i=0;i<samples;i++){
  let step=steps[index],diff=pcm.readInt16LE(i*2)-predictor,n=diff<0?8:0;
  diff=Math.abs(diff);let delta=step>>3;
  if(diff>=step){n|=4;diff-=step;delta+=step;}step>>=1;
  if(diff>=step){n|=2;diff-=step;delta+=step;}step>>=1;
  if(diff>=step){n|=1;delta+=step;}
  predictor=Math.max(-32768,Math.min(32767,predictor+(n&8?-delta:delta)));
  index=Math.max(0,Math.min(88,index+changes[n&7]));
  if(i%2)out[12+(i>>1)]|=n;else out[12+(i>>1)]=n<<4;
 }
 return out;
}
module.exports={decode,encodeReply};
