/* SPDX-License-Identifier: GPL-3.0-only */
'use strict';
const audio=document.querySelector('#audio'),status=document.querySelector('#status');
const canvas=document.querySelector('#stage'),ctx=canvas.getContext('2d');
const atlas=new Image(),portrait=new Image(),effectAtlas=new Image();
atlas.src='media/original-stage.png';portrait.src='media/original-face.png';effectAtlas.src='media/effects.png';
const faceContext=document.querySelector('#face').getContext('2d');
const outfit=document.querySelector('#outfit'),background=document.querySelector('#background'),frame=document.querySelector('#frame'),effect=document.querySelector('#effect'),intensity=document.querySelector('#intensity');
fetch('media/wardrobe.json').then(r=>{if(!r.ok)throw Error('Cannot load wardrobe');return r.json();}).then(items=>{
 outfit.replaceChildren(...items.map(item=>{const o=document.createElement('option');o.value=item.id;o.textContent=item.name;return o;}));
 outfit.onchange=()=>{const item=items.find(x=>x.id===outfit.value);if(!item)return;atlas.src='media/'+item.stage;portrait.src='media/'+item.faces;};
}).catch(error=>{status.textContent=error.message;});
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
   if(item.sample<previous||item.sample>44100*3600||item.mouth>5||item.level>127||item.mora>72||item.section>5||item.row>63||item.step>15||item.playing>1||item.sprite>7||item.audio>127||typeof item.lyric!=='string'||item.lyric.length>8)throw Error('State value out of range.');
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
 const bg=background.selectedIndex,fr=frame.selectedIndex,fx=effect.selectedIndex,level=e?e.audio:0;
 ctx.strokeStyle='#103040';ctx.lineWidth=1;
 if(bg===1){for(let x=0;x<240;x+=24){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,214);ctx.stroke();}for(let y=142;y<214;y+=18){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(240,y);ctx.stroke();}}
 if(bg===2){ctx.fillStyle='#101830';ctx.fillRect(0,142,240,72);ctx.fillStyle='#28d0c8';ctx.fillRect(0,142,240,2);}
 if(bg===3){
 ctx.fillStyle='#1c102c';ctx.fillRect(0,0,240,214);
 for(let x=0;x<30;x+=6){ctx.fillStyle=(x/6)%2?'#481038':'#280828';ctx.fillRect(x,0,6,164);ctx.fillRect(234-x,0,6,164);}
 ctx.fillStyle='#183f58';for(const x of [45,194]){ctx.beginPath();ctx.moveTo(x,28);ctx.lineTo(x-32,158);ctx.lineTo(x+32,158);ctx.fill();}
 for(let y=164;y<214;y+=16)for(let x=0;x<240;x+=24){ctx.fillStyle=((x/24+(y-164)/16)%2)?'#182038':'#282840';ctx.fillRect(x,y,24,16);}
 ctx.fillStyle='#687088';ctx.fillRect(0,16,240,2);ctx.fillRect(0,23,240,1);ctx.fillStyle='#28d0c8';ctx.fillRect(0,164,240,2);
 }
 if(fr){ctx.strokeStyle='#28d0c8';if(fr===1)ctx.strokeRect(4,4,231,205);else{ctx.fillStyle='#28d0c8';ctx.fillRect(3,8,2,level*198/127);ctx.fillRect(235,8,2,level*198/127);}}
 const age=(audio.currentTime*1000)%960,phase=Math.floor(age/240),density=Math.min(3,1+Math.floor(Number(intensity.value)/48));
 if(fx&&Number(intensity.value)>0&&e){
  if(fx===5){ctx.strokeStyle='#28d0c8';for(let x=-80;x<320;x+=80){ctx.beginPath();ctx.moveTo(x+age/24,0);ctx.lineTo(x+age/24-60,180);ctx.stroke();}}
  else if(effectAtlas.complete&&effectAtlas.naturalWidth){for(let n=0;n<density;n++){
   const left=[8,196,104][n];let top=[28,62,174][n];if(fx===3)top=(top+Math.floor(age/12))%182;if(fx===4)top=178-Math.floor(age/32);
   ctx.drawImage(effectAtlas,((fx-1)*4+phase)*32,0,32,32,left,top,32,32);
  }}
 }
 faceContext.imageSmoothingEnabled=false;faceContext.fillStyle='#0c1428';faceContext.fillRect(0,0,90,90);
 if(portrait.complete&&portrait.naturalWidth)faceContext.drawImage(portrait,(e&&e.level?e.mouth<5?e.mouth+2:7:0)*90,0,90,90,0,0,90,90);
 if(atlas.complete&&atlas.naturalWidth)ctx.drawImage(atlas,(e?e.sprite:0)*61,0,61,71,29,1,183,213);
 ctx.fillStyle='#f77bb4';ctx.font='12px monospace';ctx.textAlign='center';
 ctx.fillText(e?`${e.lyric.toUpperCase()||'A'} · ${mouth[e.mouth]}`:outfit.options[outfit.selectedIndex].textContent,120,230);
 if(e)status.textContent=`${e.playing?'PLAYING':'RELEASE'} · ${section[e.section]} · row ${e.row+1}, step ${e.step+1} · actual mouth ${mouth[e.mouth]} · sample ${e.sample}`;
 requestAnimationFrame(draw);
}
draw();
