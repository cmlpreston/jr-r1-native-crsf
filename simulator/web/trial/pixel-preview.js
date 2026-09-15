'use strict';
// Rasterize the local design at the LCD resolution, without devicePixelRatio.
// The original DOM still handles touch, keyboard focus and accessibility.
(() => {
  const lcd=document.getElementById('lcd');
  const canvas=document.getElementById('pixel-screen');
  const toggle=document.getElementById('pixel-preview');
  const note=document.getElementById('pixel-note');
  const context=canvas.getContext('2d');
  const serializer=new XMLSerializer();
  const css=[...document.styleSheets].flatMap(sheet=>{
    try{return [...sheet.cssRules].map(rule=>rule.cssText);}catch{return [];}
  }).join('\n');
  let previous='',busy=false,failed=false;
  toggle.addEventListener('change',()=>{
    canvas.hidden=!toggle.checked;
    previous='';failed=false;
    note.textContent=toggle.checked
      ?'Native pixel preview. Browser fonts approximate the firmware; physical size and panel contrast vary.'
      :'Smooth browser preview: enlarged text is sharper than the actual LCD.';
  });
  function frame(){
    if(busy||failed||document.hidden||!toggle.checked)return;
    const portrait=lcd.dataset.orientation==='portrait';
    const width=portrait?320:480,height=portrait?480:320;
    const copy=lcd.cloneNode(true);
    copy.style.transform='none';
    copy.style.width=width+'px';copy.style.height=height+'px';
    // An SVG image has no focus state of its own: preserve the visible focus.
    const focused=[...lcd.querySelectorAll('button')].indexOf(document.activeElement);
    if(focused>=0)copy.querySelectorAll('button')[focused].classList.add('key-focus');
    const html=serializer.serializeToString(copy);
    if(html===previous)return;
    previous=html;busy=true;
    const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');
    svg.setAttribute('width',width);svg.setAttribute('height',height);
    const foreign=document.createElementNS('http://www.w3.org/2000/svg','foreignObject');
    foreign.setAttribute('width','100%');foreign.setAttribute('height','100%');
    const style=document.createElement('style');style.textContent=css;
    foreign.append(style,copy);svg.append(foreign);
    const picture=new Image();
    picture.onload=()=>{
      if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height;}
      context.clearRect(0,0,width,height);
      context.drawImage(picture,0,0,width,height);
      canvas.dataset.ready='true';busy=false;
    };
    picture.onerror=()=>{
      busy=false;failed=true;canvas.hidden=true;toggle.checked=false;
      note.textContent='LCD raster preview is unavailable in this browser. Showing the smooth layout preview.';
    };
    picture.src='data:image/svg+xml;charset=utf-8,'+encodeURIComponent(serializer.serializeToString(svg));
  }
  // Enough for a design review; this is not a simulation of LCD refresh rate.
  setInterval(frame,100);
  frame();
})();
