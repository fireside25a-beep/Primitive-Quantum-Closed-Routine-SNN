#!/usr/bin/env node
// Render actual receipt frames through the same independent browser viewer.
// Optional publishing tool: PLAYWRIGHT_MODULE and NM_CHROME select local tools.
'use strict';
const {spawn}=require('child_process');
const fs=require('fs');
const path=require('path');
const {once}=require('events');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const root=path.resolve(__dirname,'..');
const capture=path.resolve(process.argv[2] || path.join(root,'evidence/observer/network/capture.json'));
const out=path.resolve(process.argv[3] || path.join(root,'evidence/observer'));
if(!fs.existsSync(capture)) throw Error('capture missing');
fs.mkdirSync(out,{recursive:true});
const server=spawn('python3',[path.join(__dirname,'serve.py'),'--replay',capture,'--port','0'],{cwd:root,stdio:['ignore','pipe','pipe']});
const serverUrl=new Promise((resolve,reject)=>{
 let output='';const timeout=setTimeout(()=>reject(Error('server start timeout')),12000);
 server.stdout.on('data',chunk=>{output+=chunk.toString();const match=output.match(/Observer: (http:\/\/127\.0\.0\.1:\d+)/);if(match){clearTimeout(timeout);resolve(match[1]);}});
 server.once('error',reject);server.once('exit',code=>reject(Error(`server exit ${code}`)));
});
(async()=>{
 let browser,encoder;
 try{
  const url=await serverUrl;
  browser=await chromium.launch({headless:true,executablePath:process.env.NM_CHROME || undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
  const page=await browser.newPage({viewport:{width:1440,height:900},deviceScaleFactor:1});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(url,{waitUntil:'domcontentloaded'});
  await page.waitForFunction(()=>window.nmObserver?.snapshot().loaded,{timeout:20000});
  const state=await page.evaluate(()=>window.nmObserver.snapshot());
  if(state.nodes<1 || errors.length)throw Error(`viewer failure: ${JSON.stringify({state,errors})}`);
  await page.evaluate(()=>window.nmObserver.seek(62));
  await page.screenshot({path:path.join(out,'viewer.png')});
  const frames=[...Array(22)].map((_,i)=>Math.min(92,i*4+1));
  async function animate(name){
   const output=path.join(out,name);
   encoder=spawn('ffmpeg',['-hide_banner','-loglevel','error','-y','-f','image2pipe','-framerate','18','-vcodec','png','-i','pipe:0','-vf','fps=18,scale=960:-1:flags=lanczos','-loop','0',output],{stdio:['pipe','ignore','pipe']});
   let err='';encoder.stderr.on('data',x=>err+=x);
   for(const i of frames){await page.evaluate(j=>window.nmObserver.seek(j),i);await page.waitForTimeout(50);const png=await page.screenshot({type:'png'});if(!encoder.stdin.write(png))await once(encoder.stdin,'drain');}
   encoder.stdin.end();const [code]=await once(encoder,'exit');if(code!==0)throw Error(`ffmpeg exit ${code}: ${err}`);
   encoder=null;return output;
  }
  const full=await animate('network-live.gif');
  await page.evaluate(()=>window.nmObserver.reveal());
  const reveal=await animate('network-reveal.gif');
  console.log(JSON.stringify({url,state,frames:frames.length,screenshot:path.join(out,'viewer.png'),animations:[full,reveal],errors}));
 }finally{if(encoder && encoder.exitCode===null)encoder.kill();if(browser)await browser.close();server.kill();}
})().catch(e=>{console.error(e);process.exitCode=1;});
