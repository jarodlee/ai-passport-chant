'use strict';
const RETAIN_MS=150000;
function jsonResponse(res,status,value){
 const body=Buffer.from(JSON.stringify(value));
 res.writeHead(status,{'Content-Type':'application/json','Content-Length':body.length,'Cache-Control':'no-store'});
 res.end(body);
}
function uploadAction(job,{id,ip,replaceReady},now=Date.now()){
 if(!job||now-job.time>=RETAIN_MS)return 'new';
 if(id&&job.requestId===id&&job.ip===ip)return 'replay';
 if(job.ip===ip&&(job.state==='error'||job.delivered||(replaceReady&&job.state==='ready')))return 'new';
 return 'busy';
}
function markDelivered(job){job.delivered=true;job.time=Date.now();}
module.exports={RETAIN_MS,jsonResponse,uploadAction,markDelivered};
