'use strict';
// Exercise the actual HTTP handler on ephemeral ports with synthetic ASR/TTS.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),http=require('node:http');
const {EventEmitter}=require('node:events');
const root=path.resolve(__dirname,'..'),key='test-pairing-key';
let worker,asr=0;
const fakeFs=Object.create(fs);
fakeFs.chmodSync=(file,...args)=>String(file)===path.join(root,'native/whisper-cli')?undefined:fs.chmodSync(file,...args);
fakeFs.readFileSync=(file,...args)=>String(file)===path.join(root,'pairing.json')?JSON.stringify({key}):fs.readFileSync(file,...args);
function execFile(file,args,options,callback){
 const child=new EventEmitter();child.kill=()=>{};
 setTimeout(()=>{
  try{
   if(file.endsWith('whisper-cli')){asr++;fs.writeFileSync(args[args.indexOf('-of')+1]+'.txt','你好');}
   else if(file==='/usr/bin/say')fs.writeFileSync(args[args.indexOf('-o')+1],worker.wav(Buffer.alloc(3200)));
   else if(file==='/usr/bin/python3')fs.writeFileSync(args[2],Buffer.alloc(4704));
   else throw Error('Unexpected test child');
   callback(null,'');
  }catch(e){callback(e,'');}
 },5);
 return child;
}
const moduleStub={exports:{}};
const sandbox={Buffer,console,setTimeout,clearTimeout,setInterval,clearInterval,__dirname:path.join(root,'node'),module:moduleStub,
 process:{stdout:{write:()=>{}}},require:name=>name==='node:fs'?fakeFs:name==='node:child_process'?{execFile}:name.startsWith('./')?require(path.join(root,'node',name)):require(name)};
const source=fs.readFileSync(path.join(root,'node/worker.cjs'),'utf8');
vm.runInNewContext(source.slice(0,source.indexOf('const timer=setInterval'))+'\nmodule.exports={start,stop,status,answer,wav};',sandbox);
worker=moduleStub.exports;
const pause=ms=>new Promise(r=>setTimeout(r,ms));
let port;
async function request(route,{method='GET',id,replace=false,data,auth=key}={}){
 const headers={Authorization:'Bearer '+auth};
 if(data){headers['Content-Length']=data.length;headers['Content-Type']='application/octet-stream';}
 if(id)headers['X-Passport-Request-ID']=id;
 if(replace)headers['X-Passport-Replace-Ready']='1';
 return new Promise((resolve,reject)=>{
  const q=http.request({host:'127.0.0.1',port,path:route,method,headers},r=>{
   const chunks=[];r.on('data',b=>chunks.push(b));r.on('end',()=>resolve({status:r.statusCode,headers:r.headers,body:Buffer.concat(chunks)}));
  });q.on('error',reject);q.end(data);
 });
}
async function waitFor(state){for(let n=0;n<100;n++){if(worker.status().state===state)return;await pause(5);}throw Error('State timeout: '+state);}
(async()=>{
 try{
  port=(await worker.start(0,0)).port;
  const upload=(id,replace=false)=>request('/v1/voice',{method:'POST',data:Buffer.alloc(2000),id,replace});
  assert.equal((await request('/v1/voice',{auth:'wrong'})).status,401);
  let r=await upload('0123456789abcdef');assert.equal(r.status,202);assert.equal(Number(r.headers['content-length']),r.body.length);
  const id=JSON.parse(r.body).jobId;
  r=await upload('0123456789abcdef');assert.equal(r.status,202);assert.equal(JSON.parse(r.body).jobId,id);
  assert.equal((await upload('1111111111111111',true)).status,409);
  await waitFor('thinking');assert.equal(asr,1);
  await worker.answer({jobId:id,ok:false,errorCode:'TEST_ERROR'});
  r=await upload('2222222222222222');assert.equal(r.status,202);
  const next=JSON.parse(r.body).jobId;assert.notEqual(next,id);
  await waitFor('thinking');await worker.answer({jobId:next,ok:true,text:'连接正常。'});await waitFor('ready');
  r=await request('/v1/jobs/'+next+'/audio',{method:'GET'});assert.equal(r.status,200);assert.equal(r.body.length,3200);
  // Delivery retains the reply for network retries while releasing admission.
  assert.equal((await request('/v1/jobs/'+next+'/audio')).status,200);
  assert.equal((await upload('2222222222222222')).status,202);
  assert.equal(asr,2);
  assert.equal((await upload('3333333333333333')).status,202);
  await waitFor('thinking');
  assert.equal((await upload('bad-id')).status,400);
  console.log('Bridge real HTTP: paired admission, replay without duplicate ASR, busy/error recovery, retained audio and JSON framing PASS');
 }finally{worker.stop();}
})().catch(e=>{console.error(e);process.exitCode=1;});
