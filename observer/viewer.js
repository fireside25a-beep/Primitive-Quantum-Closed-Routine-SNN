'use strict';
const $=id=>document.getElementById(id);
const canvas=$('network'),ctx=canvas.getContext('2d'),raster=$('raster'),rx=raster.getContext('2d');
const background=document.createElement('canvas'),bg=background.getContext('2d');
let topology=null,positions=new Map(),ticks=[],capture=null,index=0,playing=false,live=false,last=0,accum=0,cursor=0,loaded=false;
let width=1,height=1,dpr=1,zoom=1,panX=0,panY=0,drag=null,seen=new Set(),status='connecting',gaps=0;
const escapeText=v=>String(v); // DOM values below use textContent only.
function field(name,value){const d=document.createElement('div'),s=document.createElement('span'),b=document.createElement('b');s.textContent=name;b.textContent=escapeText(value);d.append(s,b);return d;}
function layout(){
 if(!topology)return;positions.clear();
 const ids=topology.neurons.map(n=>n.id).sort((a,b)=>a-b),outs=new Map(ids.map(id=>[id,[]])),incoming=new Map(ids.map(id=>[id,0]));
 for(const e of topology.edges){outs.get(e.src)?.push(e.dst);incoming.set(e.dst,(incoming.get(e.dst)||0)+1);}
 let roots=ids.filter(id=>incoming.get(id)===0);if(!roots.length&&ids.length)roots=[ids[0]];
 const depth=new Map(),queue=[...roots];roots.forEach(id=>depth.set(id,0));
 for(let p=0;p<queue.length;p++){const id=queue[p];for(const to of outs.get(id)||[])if(!depth.has(to)){depth.set(to,depth.get(id)+1);queue.push(to);}}
 for(const id of ids)if(!depth.has(id))depth.set(id,0);
 const maxDepth=Math.max(1,...depth.values()),layers=Array.from({length:maxDepth+1},()=>[]);
 ids.forEach(id=>layers[depth.get(id)].push(id));
 for(let level=0;level<layers.length;level++){
   const row=layers[level],span=width*(.14+.78*level/maxDepth);
   row.forEach((id,i)=>{const x=width/2+(row.length===1?0:(i/(row.length-1)-.5)*span),y=height*.9-level/maxDepth*height*.78;positions.set(id,{x,y});});
 }
 paintBackground();
}
function curve(c,a,b){c.moveTo(a.x,a.y);c.bezierCurveTo(a.x,a.y+(b.y-a.y)*.48,b.x,b.y-(b.y-a.y)*.32,b.x,b.y);}
function paintBackground(){
 if(!topology)return;background.width=canvas.width;background.height=canvas.height;bg.setTransform(dpr,0,0,dpr,0,0);bg.clearRect(0,0,width,height);
 bg.strokeStyle='rgba(167,198,214,.17)';bg.lineWidth=.65;bg.beginPath();
 for(const e of topology.edges){const a=positions.get(e.src),b=positions.get(e.dst);if(a&&b)curve(bg,a,b);}bg.stroke();
 for(const n of topology.neurons){const p=positions.get(n.id);bg.fillStyle='rgba(181,211,225,.55)';bg.beginPath();bg.arc(p.x,p.y,1.2,0,Math.PI*2);bg.fill();}
 if(topology.neurons.length<=32){bg.fillStyle='#647d8b';bg.font='10px monospace';for(const [id,p] of positions)bg.fillText(String(id),p.x+7,p.y+3);}
}
function resize(){const rect=canvas.getBoundingClientRect();width=rect.width;height=rect.height;dpr=Math.min(2,devicePixelRatio||1);canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);const rr=raster.getBoundingClientRect();raster.width=Math.round(rr.width*dpr);raster.height=Math.round(rr.height*dpr);layout();}
function setTick(i){index=Math.max(0,Math.min(ticks.length-1,i));const t=ticks[index];if(!t)return;
 if($('reveal').checked){seen.clear();for(let j=0;j<=index;j++)for(const id of ticks[j].spikes)seen.add(id);}
 else for(const id of t.spikes)seen.add(id);
 $('tick').textContent=t.tick.toLocaleString();$('spikes').textContent=t.spikes.length;$('seek').value=index;
 $('fired').textContent=t.spikes.length?t.spikes.join(' · '):'No recorded spikes this tick';
 const f=t.fields,details=[field('Mode',topology.mode),field('Time ns',f.time_ns??'not declared'),field('Selected routes',t.routes.length)];
 if(f.flux!==undefined)details.push(field('Field flux',f.flux));
 if(f.corrected!==undefined)details.push(field('Q corrections',f.corrected));
 if(f.syndrome!==undefined)details.push(field('Syndromes',f.syndrome||'none'));
 if(f.qmemory)details.push(field('Memory corrections',f.qmemory.split(':')[1]));
 details.push(field('Event',f.event?f.event.slice(0,16)+'…':'—'));$('details').replaceChildren(...details);
}
function install(data){capture=data;topology=data.topology;ticks=data.ticks;index=0;seen.clear();loaded=true;live=false;
 $('scenario').textContent=topology.name;$('size').textContent=`${topology.neurons.length} / ${topology.edges.length}`;
 $('seek').disabled=false;$('seek').max=Math.max(0,ticks.length-1);$('play').disabled=false;$('play').textContent='Play receipt';
 $('start').disabled=true;$('mode').textContent='RECEIPT PLAYBACK';
 const p=data.provenance;gaps=p.transport_gaps||0;$('gaps').textContent=gaps;
 $('provenance').textContent=`${p.independent_replay_passed?'Independent replay passed.':'Replay verification not recorded.'} Live ticks received: ${p.live_ticks_received}/${ticks.length}. Live frame gaps: ${gaps}. Final receipt: ${data.roots.receipt_root}. Playback uses the complete final receipt.`;
 status='receipt ready';$('status').textContent='RECEIPT READY';layout();setTick(0);
}
function render(now){
 const dt=Math.min(.1,Math.max(0,(now-last)/1000));last=now;
 if(playing&&ticks.length){accum+=dt*Number($('speed').value);if(accum>=1){const step=Math.floor(accum);accum-=step;if(index+step>=ticks.length){playing=false;$('play').textContent='Replay';setTick(ticks.length-1);}else setTick(index+step);}}
 ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,width,height);ctx.save();ctx.translate(width/2+panX,height/2+panY);ctx.scale(zoom,zoom);ctx.translate(-width/2,-height/2);
 if(!$('reveal').checked)ctx.drawImage(background,0,0,width,height);
 else if(topology){ctx.lineWidth=.65;ctx.strokeStyle='rgba(175,209,225,.2)';ctx.beginPath();for(const e of topology.edges)if(seen.has(e.src)&&seen.has(e.dst))curve(ctx,positions.get(e.src),positions.get(e.dst));ctx.stroke();for(const id of seen){const p=positions.get(id);if(p){ctx.fillStyle='#7694a4';ctx.fillRect(p.x-1,p.y-1,2,2);}}}
 const active=new Map();for(let j=Math.max(0,index-5);j<=index;j++)for(const id of ticks[j]?.spikes||[])active.set(id,Math.max(active.get(id)||0,1-(index-j)/6));
 const current=ticks[index];if(current){ctx.strokeStyle='rgba(237,189,120,.8)';ctx.lineWidth=1.4;ctx.beginPath();for(const [src,dst] of current.routes){const a=positions.get(src),b=positions.get(dst);if(a&&b)curve(ctx,a,b);}ctx.stroke();}
 for(const [id,strength] of active){const p=positions.get(id);if(!p)continue;const radius=12+9*strength;const g=ctx.createRadialGradient(p.x,p.y,0,p.x,p.y,radius);g.addColorStop(0,`rgba(220,249,255,${.65*strength})`);g.addColorStop(.2,`rgba(140,220,245,${.25*strength})`);g.addColorStop(1,'rgba(80,160,190,0)');ctx.fillStyle=g;ctx.fillRect(p.x-radius,p.y-radius,radius*2,radius*2);ctx.beginPath();ctx.arc(p.x,p.y,1.4+strength,0,Math.PI*2);ctx.fillStyle=`rgba(240,255,255,${strength})`;ctx.fill();}
 ctx.restore();drawRaster();requestAnimationFrame(render);
}
function drawRaster(){const w=raster.width/dpr,h=raster.height/dpr;rx.setTransform(dpr,0,0,dpr,0,0);rx.clearRect(0,0,w,h);if(!topology)return;rx.strokeStyle='#14232d';rx.lineWidth=.5;for(let y=0;y<=4;y++){rx.beginPath();rx.moveTo(0,y*h/4);rx.lineTo(w,y*h/4);rx.stroke();}
 const start=Math.max(0,index-180),span=180,maxId=Math.max(1,...topology.neurons.map(n=>n.id));rx.fillStyle='#c0e9f4';for(let j=start;j<=index;j++){const t=ticks[j];if(!t)continue;const x=(j-start)/span*w;for(const id of t.spikes)rx.fillRect(x,4+id/maxId*(h-8),1.5,1.6);}rx.fillStyle='#d8b477';rx.fillRect((index-start)/span*w,0,1,h);
}
async function poll(){try{const r=await fetch('/api/poll?after='+cursor);const data=await r.json();cursor=data.cursor||cursor;
 if(!loaded){status=data.status;$('status').textContent=status.toUpperCase();gaps=(data.transport_gaps||0)+(data.client_skipped||0);$('gaps').textContent=gaps;
 if(data.ticks?.length){live=true;$('mode').textContent='LIVE / PROVISIONAL';ticks.push(...data.ticks);if(ticks.length>4096)ticks.splice(0,ticks.length-4096);setTick(ticks.length-1);}
 if(data.status==='failed'){$('provenance').textContent=data.error;}
 if(['verified','complete','replay'].includes(data.status)){const result=await fetch('/api/result');if(result.ok)install(await result.json());}
 }}catch(e){$('status').textContent='CONNECTION LOST';}setTimeout(poll,100);}
$('start').onclick=async()=>{$('start').disabled=true;const r=await fetch('/api/start',{method:'POST'});if(!r.ok){$('status').textContent='START REFUSED';$('start').disabled=false;}};
$('play').onclick=()=>{if(index>=ticks.length-1){seen.clear();setTick(0);}playing=!playing;$('play').textContent=playing?'Pause':'Play receipt';};
$('seek').oninput=()=>{playing=false;$('play').textContent='Play receipt';seen.clear();const i=Number($('seek').value);for(let k=0;k<=i;k++)for(const id of ticks[k].spikes)seen.add(id);setTick(i);};
$('fit').onclick=()=>{zoom=1;panX=panY=0;};canvas.onwheel=e=>{e.preventDefault();zoom=Math.max(.3,Math.min(8,zoom*Math.exp(-e.deltaY*.001)));};
canvas.onpointerdown=e=>{drag={x:e.clientX,y:e.clientY};canvas.setPointerCapture(e.pointerId);};canvas.onpointermove=e=>{if(drag){panX+=e.clientX-drag.x;panY+=e.clientY-drag.y;drag={x:e.clientX,y:e.clientY};}};canvas.onpointerup=()=>drag=null;
window.addEventListener('resize',resize);
// Test/export entry point renders only supplied capture ticks, never synthetic data.
window.nmObserver={load:install,seek:i=>{playing=false;setTick(i);},snapshot:()=>({index,tick:ticks[index]?.tick,nodes:topology?.neurons.length,spikes:ticks[index]?.spikes,status,loaded}),reveal:()=>{$('reveal').checked=true;}};
(async()=>{topology=await(await fetch('/api/topology')).json();$('scenario').textContent=topology.name;$('size').textContent=`${topology.neurons.length} / ${topology.edges.length}`;resize();poll();requestAnimationFrame(render);})().catch(e=>{$('status').textContent='LOAD FAILED';$('provenance').textContent=e.message;});
