/* SPDX-License-Identifier: GPL-3.0-only */
'use strict';
const audio=document.querySelector('#audio'),status=document.querySelector('#status');
const canvas=document.querySelector('#stage'),ctx=canvas.getContext('2d');
const atlas=new Image();atlas.src='../site/media/dancer.png';
let events=[],sampleRate=44100,objectURL=null;
const section=['','INTRO','VERSE','CHORUS','BRIDGE','OUTRO'],mouth=['A','I','U','E','O','CLOSED'];
document.querySelector('#wave').onchange=event=>{
 const file=event.target.files[0];if(!file)return;
 if(objectURL)URL.revokeObjectURL(objectURL);
 objectURL=URL.createObjectURL(file);audio.src=objectURL;
};
document.querySelector('#state').onchange=async event=>{
 try{
  const file=event.target.files[0];if(!file)return;if(file.size>10*1024*1024)throw Error('State file exceeds 10 MiB.');
  const data=JSON.parse(await file.text());
  if(data.format!=='MIKU-performance-1'||data.rate!==44100||!Array.isArray(data.events)||data.events.length>100000)throw Error('Unsupported firmware-state file.');
  let previous=-1;
  for(const item of data.events){
   for(const key of ['sample','mouth','level','mora','section','row','step','playing','sprite','audio'])if(!Number.isSafeInteger(item[key])||item[key]<0)throw Error('Invalid state values.');
   if(item.sample<previous||item.sample>44100*3600||item.mouth>5||item.level>127||item.mora>69||item.section>5||item.row>63||item.step>15||item.playing>1||item.sprite>7||item.audio>127||typeof item.lyric!=='string'||item.lyric.length>8)throw Error('State value out of range.');
   previous=item.sample;
  }
  events=data.events;sampleRate=data.rate;status.textContent=`Loaded ${events.length} firmware records. Load the matching WAV and press play.`;
 }catch(error){events=[];status.textContent=error.message;}
};
function draw(){
 let e=null,lo=0,hi=events.length,target=Math.floor(audio.currentTime*sampleRate);
 while(lo<hi){const mid=(lo+hi)>>>1;if(events[mid].sample<=target)lo=mid+1;else hi=mid;}
 if(lo)e=events[lo-1];
 ctx.imageSmoothingEnabled=false;ctx.fillStyle='#0c1428';ctx.fillRect(0,0,240,240);
 if(e){
  ctx.fillStyle=`rgba(64,222,209,${e.audio/700})`;ctx.fillRect(0,0,240,214);
 }
 if(atlas.complete&&atlas.naturalWidth)ctx.drawImage(atlas,(e?e.sprite:0)*61,0,61,71,29,1,183,213);
 ctx.fillStyle='#f77bb4';ctx.font='12px monospace';ctx.textAlign='center';
 ctx.fillText(e?`${e.lyric.toUpperCase()||'A'} · ${mouth[e.mouth]}`:'ORIGINAL OUTFIT',120,230);
 if(e)status.textContent=`${e.playing?'PLAYING':'RELEASE'} · ${section[e.section]} · row ${e.row+1}, step ${e.step+1} · actual mouth ${mouth[e.mouth]} · sample ${e.sample}`;
 requestAnimationFrame(draw);
}
draw();
