'use strict';
const fs=require('node:fs'),path=require('node:path'),os=require('node:os'),http=require('node:http'),dgram=require('node:dgram'),crypto=require('node:crypto'),rl=require('node:readline'),{execFile}=require('node:child_process');
const root=path.join(__dirname,'..'),key=JSON.parse(fs.readFileSync(path.join(root,'pairing.json'))).key;
let server,udp,job=null,lastResponse=null; const children=new Set();
const {jsonResponse,uploadAction,markDelivered}=require('./voice-jobs.cjs');
const emit=o=>process.stdout.write(JSON.stringify(o)+'\n');
const privateIP=ip=> /^(127\.|10\.|192\.168\.|169\.254\.)/.test(ip)||/^172\.(1[6-9]|2\d|3[01])\./.test(ip);
const normalize=ip=>String(ip||'').replace(/^::ffff:/,'');
const mac=text=>crypto.createHmac('sha256',key).update(text).digest('hex');
const exec=(file,args,timeout=30000)=>new Promise((resolve,reject)=>{const c=execFile(file,args,{timeout,maxBuffer:512*1024},(e,out)=>{children.delete(c);e?reject(new Error(e.killed?'PROCESS_TIMEOUT':e.message)):resolve(out)});children.add(c);});
function json(res,status,value){lastResponse={kind:res.passportKind||'request',status,error:value.error||''};jsonResponse(res,status,value);}
function wav(pcm){const b=Buffer.alloc(44+pcm.length);b.write('RIFF');b.writeUInt32LE(36+pcm.length,4);b.write('WAVEfmt ',8);b.writeUInt32LE(16,16);b.writeUInt16LE(1,20);b.writeUInt16LE(1,22);b.writeUInt32LE(16000,24);b.writeUInt32LE(32000,28);b.writeUInt16LE(2,32);b.writeUInt16LE(16,34);b.write('data',36);b.writeUInt32LE(pcm.length,40);pcm.copy(b,44);return b;}
function normalizeAudio(pcm){let peak=0;for(let i=0;i+1<pcm.length;i+=2){const v=Math.abs(pcm.readInt16LE(i));if(v>peak)peak=v;}if(!peak||peak>=31000)return pcm;const gain=31000/peak;for(let i=0;i+1<pcm.length;i+=2){const v=Math.round(pcm.readInt16LE(i)*gain);pcm.writeInt16LE(Math.max(-32768,Math.min(32767,v)),i);}return pcm;}
function rawWav(b){if(b.toString('ascii',0,4)!=='RIFF')throw Error('BAD_WAV');let pos=12,format=false;while(pos+8<=b.length){let id=b.toString('ascii',pos,pos+4),n=b.readUInt32LE(pos+4);if(pos+8+n>b.length)throw Error('BAD_WAV');if(id==='fmt '){format=b.readUInt16LE(pos+8)===1&&b.readUInt16LE(pos+10)===1&&b.readUInt32LE(pos+12)===16000&&b.readUInt16LE(pos+22)===16;}if(id==='data'&&format)return b.subarray(pos+8,pos+8+n);pos+=8+n+(n%2);}throw Error('BAD_WAV');}
const {decode,encodeReply}=require('./voice-codec.cjs');
function cleanup(){if(!job)return;for(const c of children)c.kill();if(job.dir)fs.rmSync(job.dir,{recursive:true,force:true});job=null;}
async function recognize(j,data){try{j.dir=fs.mkdtempSync(path.join(os.tmpdir(),'passport-'));fs.chmodSync(j.dir,0o700);const input=path.join(j.dir,'input.wav'),out=path.join(j.dir,'transcript');fs.writeFileSync(input,wav(decode(data)),{mode:0o600});const binary=path.join(root,'native/whisper-cli');fs.chmodSync(binary,0o755);await exec(binary,['-m',path.join(root,'native/ggml-tiny.bin'),'-f',input,'-l','zh','-otxt','-of',out,'-nt','-np','-t','4'],45000);if(job!==j)return;const text=fs.readFileSync(out+'.txt','utf8').trim().slice(0,2000);if(!text||/^\[.*\]$/.test(text))throw Error('NO_SPEECH');j.state='thinking';j.time=Date.now();emit({jsonrpc:'2.0',method:'question',params:{jobId:j.id,text}});}catch(e){if(job===j){j.state='error';j.error=String(e.message).slice(0,120);}}}
async function answer(p){const j=job;if(!j||p.jobId!==j.id)return{ignored:true};if(!p.ok){j.state='error';j.error=p.errorCode||'CINDY_ERROR';return{ok:false};}try{j.state='synthesizing';j.time=Date.now();const text=String(p.text||'').replace(/[*#`]/g,'').trim().slice(0,300);if(!text)throw Error('EMPTY_REPLY');const out=path.join(j.dir,'reply.wav');await exec('/usr/bin/say',['-v','Tingting','-r','185','--file-format=WAVE','--data-format=LEI16@16000','-o',out,'--',text],30000);if(job!==j)return{ignored:true};j.audio=normalizeAudio(rawWav(fs.readFileSync(out)));if(!j.audio.length||j.audio.length>16000*2*90)throw Error('AUDIO_LIMIT');j.audioAdpcm=encodeReply(j.audio);try{const textFile=path.join(j.dir,'reply.txt'),bitmap=path.join(j.dir,'reply.bin');fs.writeFileSync(textFile,text,{mode:0o600});await exec('/usr/bin/python3',[path.join(root,'node/render_text.py'),textFile,bitmap],20000);if(job!==j)return{ignored:true};j.reply=fs.readFileSync(bitmap);}catch(e){console.error('TEXT_RENDER '+e.message);}j.time=Date.now();j.state='ready';return{ok:true};}catch(e){if(job===j){j.state='error';j.error=String(e.message).slice(0,120);}return{ok:false};}}
async function handle(req,res){const ip=normalize(req.socket.remoteAddress);res.passportKind=req.url==='/v1/voice'?'upload':'request';if(!privateIP(ip)){json(res,403,{error:'LAN_ONLY'});return;}if(req.headers.authorization!=='Bearer '+key){json(res,401,{error:'UNPAIRED'});return;}
 if(req.method==='POST'&&req.url==='/v1/voice'){
  const requestId=req.headers['x-passport-request-id'];
  if(requestId!==undefined&&!/^[a-f0-9]{16}$/.test(requestId)){req.resume();json(res,400,{error:'BAD_REQUEST_ID'});return;}
  const length=Number(req.headers['content-length']);
  if(!Number.isInteger(length)||length<1600||length>64000){req.resume();json(res,413,{error:'AUDIO_LIMIT'});return;}
  const action=uploadAction(job,{id:requestId,ip,replaceReady:req.headers['x-passport-replace-ready']==='1'});
  if(action==='replay'){req.resume();json(res,202,{jobId:job.id});return;}
  if(action==='busy'){req.resume();json(res,409,{error:'BUSY'});return;}
  // Reserve the slot before reading: a second upload cannot start another ASR.
  cleanup();const j=job={id:crypto.randomBytes(8).toString('hex'),requestId,state:'uploading',time:Date.now(),ip};
  const parts=[];let size=0;req.setTimeout(15000,()=>req.destroy());
  req.on('aborted',()=>{if(job===j)cleanup();});
  req.on('data',b=>{size+=b.length;if(size>length)req.destroy();else parts.push(b);});
  req.on('end',()=>{
   if(job!==j){json(res,409,{error:'BUSY'});return;}
   if(size!==length){cleanup();json(res,400,{error:'SHORT_BODY'});return;}
   j.state='recognizing';json(res,202,{jobId:j.id});recognize(j,Buffer.concat(parts));
  });return;
 }
 const m=/^\/v1\/jobs\/([a-f0-9]{16})(\/audio|\/text|\/cancel)?$/.exec(req.url);if(m&&job&&m[1]===job.id&&ip===job.ip){if(m[2]==='/cancel'){cleanup();json(res,200,{ok:true});}else if(m[2]==='/text'){if(job.state!=='ready'||!job.reply){json(res,404,{error:'NO_TEXT'});return;}res.writeHead(200,{'Content-Type':'application/octet-stream','Content-Length':job.reply.length,'Cache-Control':'no-store'});res.end(job.reply);}else if(m[2]){if(job.state!=='ready'){json(res,409,{error:'NOT_READY'});return;}const compressed=req.headers.accept==='audio/x-passport-ima-adpcm';const audio=compressed?job.audioAdpcm:job.audio;res.writeHead(200,{'Content-Type':compressed?'audio/x-passport-ima-adpcm':'application/octet-stream','Content-Length':audio.length,'Cache-Control':'no-store','Vary':'Accept'});res.end(audio);res.on('finish',()=>{if(job&&job.id===m[1]){markDelivered(job);}});}else json(res,200,{state:job.state,bytes:job.audio?.length||0,error:job.error||''});return;}json(res,404,{error:'NOT_FOUND'});
}
async function start(httpPort=18765,udpPort=18766){if(server)return status();server=http.createServer((q,s)=>{handle(q,s).catch(()=>{if(!s.headersSent)json(s,500,{error:'INTERNAL'});else s.destroy();});});server.headersTimeout=10000;server.requestTimeout=15000;server.maxConnections=4;try{await new Promise((ok,no)=>{server.once('error',no);server.listen(httpPort,'0.0.0.0',ok)});udp=dgram.createSocket('udp4');udp.on('message',(b,r)=>{if(!privateIP(r.address)||b.length>200)return;const text=b.toString(),m=/^PASSPORT ([a-f0-9]{16}) ([a-f0-9]{64})$/.exec(text);if(!m||!crypto.timingSafeEqual(Buffer.from(m[2]),Buffer.from(mac(m[1]))))return;udp.send(Buffer.from('PASSPORT '+m[1]+' '+mac(m[1]+' reply')),r.port,r.address);});udp.on('error',e=>console.error(e.message));await new Promise((ok,no)=>{udp.once('error',no);udp.bind(udpPort,'0.0.0.0',ok)});return status();}catch(e){stop();throw e;}}
function stop(){cleanup();if(server){server.closeAllConnections();server.close();server=null;}if(udp){try{udp.close()}catch{}udp=null;}return status();}
function status(){return{running:!!server,port:server?.address()?.port||18765,addresses:Object.values(os.networkInterfaces()).flat().filter(a=>a.family==='IPv4'&&!a.internal&&privateIP(a.address)).map(a=>a.address),state:job?.state||'idle',lastResponse,note:'按住确定说话，松开发送；支持同一局域网或已配置的 HTTPS 入口。停止服务可调用 status(stop:true)。'};}
const timer=setInterval(()=>{if(job&&Date.now()-job.time>150000)cleanup();},10000);timer.unref();
rl.createInterface({input:process.stdin}).on('line',async line=>{let r;try{r=JSON.parse(line);let result;if(r.method==='start')result=await start();else if(r.method==='stop')result=stop();else if(r.method==='answer')result=await answer(r.params);else if(r.method==='status')result=status();else throw Error('UNKNOWN_METHOD');emit({jsonrpc:'2.0',id:r.id,result});}catch(e){emit({jsonrpc:'2.0',id:r?.id??null,error:{code:-32000,message:String(e.message)}});}}).on('close',()=>{stop();process.exit(0)});
module.exports={decode,rawWav,wav};
