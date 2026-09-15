"use strict";
const scenes=[
{id:"16662",label:"Courtyard",title:"Courtyard · fabric & skin"},
{id:"14456",label:"City street",title:"City street · concrete & foliage"},
{id:"17186",label:"Rooftop",title:"Rooftop · architecture & daylight"},
{id:"18690",label:"Sunset",title:"Sunset · faces & warm light"},
{id:"19688",label:"Dining",title:"Rooftop dining · glass & materials"},
{id:"35048",label:"Airport",title:"Airport · hair & interior lighting"}
];
const comparison=document.getElementById("comparison"),split=document.getElementById("split");
const before=document.getElementById("before"),after=document.getElementById("after");
const status=document.getElementById("load-status");
let request=0;
function setSplit(value){split.value=value;comparison.style.setProperty("--split",value+"%");split.setAttribute("aria-valuetext",value+" percent original, "+(100-value)+" percent enhanced");}
split.addEventListener("input",()=>setSplit(Number(split.value)));
document.getElementById("show-before").addEventListener("click",()=>setSplit(100));
document.getElementById("show-after").addEventListener("click",()=>setSplit(0));
document.getElementById("reset").addEventListener("click",()=>setSplit(50));
const buttons=scenes.map((scene,index)=>{
 const button=document.createElement("button");button.className="scene";button.textContent=scene.label;
 button.setAttribute("aria-pressed",index===0?"true":"false");
 button.addEventListener("click",()=>selectScene(index));document.getElementById("scenes").append(button);return button;
});
function load(url){return new Promise((resolve,reject)=>{const img=new Image();img.onload=()=>resolve(img);img.onerror=reject;img.src=url;});}
async function selectScene(index){
 const token=++request,scene=scenes[index];comparison.classList.add("loading");comparison.setAttribute("aria-busy","true");status.textContent="Loading "+scene.label.toLowerCase()+" comparison…";
 try{
  const images=await Promise.all([load("images/"+scene.id+"-before.png"),load("images/"+scene.id+"-after.png")]);
  if(token!==request)return;
  before.src=images[0].src;after.src=images[1].src;
  before.alt="Original "+scene.label.toLowerCase()+" scene";after.alt="Enhanced "+scene.label.toLowerCase()+" scene";
  buttons.forEach((button,i)=>button.setAttribute("aria-pressed",i===index?"true":"false"));
  document.getElementById("title").textContent=scene.title;document.getElementById("counter").textContent=String(index+1).padStart(2,"0")+" / 06";
  setSplit(50);status.textContent="";
 }catch{if(token===request)status.textContent="This comparison could not load. Select the scene again to retry.";}
 finally{if(token===request){comparison.classList.remove("loading");comparison.removeAttribute("aria-busy");}}
}
selectScene(0);