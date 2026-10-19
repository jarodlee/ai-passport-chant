// Exercise the real sandbox main.js through both text and device entry points.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const path=require('node:path');
const route={agentKind:'pi',providerId:'deepseek',model:'deepseek-v4-flash',effort:'high',fastMode:false};
let cfg={taskId:'owned-voice-task',route},handler;
const sent=[],results=[],nodeCalls=[];
const context=vm.createContext({
 Intl,Date,crypto:{randomUUID:()=> 'test-random-id'},setTimeout,
 BroadcastChannel:class {postMessage(){}},
 fetch:async (_url,options)=>{
  if(options?.method==='PUT')cfg=JSON.parse(options.body);
  return {ok:true,json:async()=>structuredClone(cfg)};
 },
 cindy:{
  onHostMessage:fn=>{handler=fn;},
  send:async result=>{results.push(result);},
  node:{request:async request=>{nodeCalls.push(request);return {ok:true};}},
  tasks:{
   capabilities:async()=>({operations:['setModel'],sourceCallContext:true}),
   get:async()=>({revision:7,resolvedConfig:route,permissionMode:'ask'}),
   send:async request=>{sent.push(request);return {runId:'run-'+sent.length};},
   getRun:async()=>({status:'completed',outputMessageId:'final-reply'}),
   readMessages:async()=>({items:[{id:'final-reply',text:'Verified answer'}]}),
   create:async()=>{throw Error('Must reuse the existing voice task');},
   setModel:async()=>{throw Error('Must preserve the selected model');},
   requestWriteAccess:async()=>{throw Error('Web lookup must not escalate local write permissions');}
  }
 }
});
vm.runInContext(fs.readFileSync(path.join(__dirname,'../main.js'),'utf8'),context);
(async()=>{
 await handler({type:'tool-call',tool:'test_text',args:{text:'Explain a factual concept'},callId:'text-test'});
 assert.equal(results[0].ok,true);
 assert.equal(results[0].result.text,'Verified answer');
 await handler({type:'event',name:'node-notification',method:'question',params:{text:'What is the weather?',jobId:'device-job'}});
 assert.equal(sent.length,2);
 assert.equal(sent[1].requestKey,'voice-device-job');
 assert.equal(sent[0].taskId,'owned-voice-task');
 for(const request of sent){
  // Both routes must send the current search-first instruction, retain scope
  // and have today's date rather than relying on stale conversation context.
  assert.match(request.text,/\d{4}-\d{2}-\d{2}/);
  assert(request.text.includes('先联网查询'));
  assert(request.text.includes('先实际调用'));
  assert(request.text.includes('最多100个汉字'));
  assert(request.text.includes('不得读写本地文件'));
  assert(request.text.includes('不能绕过'));
 }
 assert(sent[0].text.endsWith('Explain a factual concept'));
 assert(sent[1].text.endsWith('What is the weather?'));
 const answer=nodeCalls.find(r=>r.method==='answer');
 assert.equal(answer.params.jobId,'device-job');
 assert.equal(answer.params.text,'Verified answer');
 assert.equal(cfg.route.model,'deepseek-v4-flash');
 console.log('Voice search-first policy on text/device routes, task reuse and unchanged permission scope: PASS');
})().catch(error=>{console.error(error);process.exitCode=1;});
