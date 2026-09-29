const STORAGE="srd-maintenance-static-v1";
const ADMIN_IDS={101:"Chhayank",102:"Lutharshan",103:"Charan",104:"Sashank",105:"murali",106:"Aditya",107:"Sathwik",108:"Preetam",109:"kartheek"};
const START_PASSWORD="maintenance123";
const SRD_CODE="srd123";
const DUTIES={"2026-09-22":"Chhayank","2026-09-23":"Lutharshan","2026-09-24":"Charan","2026-09-25":"Sashank","2026-09-26":"murali","2026-09-27":"Aditya","2026-09-28":"Sathwik","2026-09-29":"Preetam","2026-09-30":"Chhayank","2026-10-01":"Lutharshan","2026-10-02":"Charan","2026-10-03":"Sashank","2026-10-04":"murali","2026-10-05":"Aditya","2026-10-06":"Sathwik","2026-10-07":"Preetam","2026-10-08":"Chhayank","2026-10-09":"Lutharshan","2026-10-10":"Charan","2026-10-11":"Sashank","2026-10-12":"murali","2026-10-13":"Aditya","2026-10-14":"Sathwik","2026-10-15":"Preetam"};
const STOCK_NAMES=["keyboard","Projector remote","headphone","XLR small wire","Monitor","Pro fx cable","Quest big speaker 1","Quest Amp","Line-in jack","ProFX30V3 type d cable","Mike WIRE(xlr)(white black )","Quest big speaker 2","CPU power chord","ProFX30V3 Mixer","HDMI Cable","headphone jack wire","MONITOR power chord","Ahuja AMP","speaker stand 1","VGA CABLE Monitor","Chair 2","mouse","Jack to Jack","MIKE Stand","CPU","SETUP BOX POWER CABLE","senior biometric","PROJECTOR","Mike 1","TV HDMI","Vga 2 HDMI","Mic","Extention To AVC","Line in to XLR","mic stand","Podium","Ahuja Podium mike holder","speaker stand 2","SETUP BOX","MiKE5","TV Screen","Light remote","quest small speaker","Mike WIRE(xlr)(white red)","Mike 2","senior biometric power cable","6 Socket Extention Box","skrew driver kit 1","Podium mike","2 Way Extention Box","HDMI Splitter (Black)","speaker wire 2","Mike 4","speaker wire 1","8 socket extension box","Mike 3","PROJECTOR (Old)","Mic WIRE(Xlr)(White Pink)","Light Wire","EP to PHONE (Blue)","TV POWER CABLE","Podium Mike Wire","Projector Power Cable","Hdmi 2 Hdmi Cable","Mic Stand Base","Soldering Machine 2","Soldering Machine 1","AVC Table","Jack to Xlr wire","Podium mic Stand","Old Headphones","Line-in Extendel","Old hostel podium mic","Line in Jack","1 Socket Extention","Jack Wire","Maintenance cupboard","XLR white","Projector Screen 1","Projector Screen 2","EP to PHONO (BLACK)","Measuring Tape","Scissors","pipes","ahuja"];

let state=load();
let currentRole=null,currentUser=null;
const $=id=>document.getElementById(id);
function today(){return new Date().toISOString().slice(0,10)}
function dayName(iso){return new Date(iso+"T12:00:00").toLocaleDateString("en-IN",{weekday:"long"})}
function load(){const raw=localStorage.getItem(STORAGE);if(raw){try{return JSON.parse(raw)}catch(e){}}return{complaints:[],requests:[],stock:Object.fromEntries(STOCK_NAMES.map(x=>[x,0])),adminPasswords:{}}}
function save(){localStorage.setItem(STORAGE,JSON.stringify(state))}
function esc(v){return String(v??"").replace(/[&<>"']/g,m=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[m]))}
function showToast(message,type="ok"){const el=document.createElement("div");el.className="toast "+type;el.textContent=message;$("toastArea").prepend(el);setTimeout(()=>el.remove(),4500)}
function showView(name){
  ["loginView","studentView","srdView","adminView"].forEach(x=>$(x).classList.add("hidden"));
  $(name).classList.remove("hidden");
}
function setUserBar(){
  const bar=$("userBar");
  if(!currentRole){bar.classList.add("hidden");bar.innerHTML="";return}
  bar.classList.remove("hidden");
  bar.innerHTML='<div><strong>'+esc(currentUser.name)+'</strong><div class="user-role">'+esc(currentRole.toUpperCase())+'</div></div><button class="ghost" id="logoutBtn">Log out</button>';
  $("logoutBtn").onclick=()=>{currentRole=null;currentUser=null;setUserBar();showView("loginView")}
}
function setDates(){["complaintDate","srdDate"].forEach(id=>$(id).value=today())}
function renderStockDatalist(){$("stockNames").innerHTML=Object.keys(state.stock).sort((a,b)=>a.localeCompare(b)).map(x=>'<option value="'+esc(x)+'">').join("")}
function login(role,ident,password){
  if(role==="student"){
    if(!/^\d+$/.test(ident)||!password){showToast("Enter a valid registration number and password.","error");return}
    currentRole="student";currentUser={name:"Student "+ident,regno:ident};showView("studentView");renderStudent()
  }else if(role==="srd"){
    if(!ident||password!==SRD_CODE){showToast("SRD name or code is incorrect.","error");return}
    currentRole="srd";currentUser={name:ident};showView("srdView");renderSRD()
  }else{
    const name=ADMIN_IDS[ident];const expected=state.adminPasswords[ident]||START_PASSWORD;
    if(!name||password!==expected){showToast("Admin ID or password is incorrect.","error");return}
    currentRole="admin";currentUser={name,id:ident};showView("adminView");renderAdmin()
  }
  setUserBar()
}
function renderStudent(){
  const mine=state.complaints.filter(c=>String(c.regno)===String(currentUser.regno)).sort((a,b)=>b.id-a.id);
  $("studentTotal").textContent=mine.length;$("studentPending").textContent=mine.filter(c=>c.status==="pending").length;$("studentDone").textContent=mine.filter(c=>c.status==="done").length;
  $("studentComplaints").innerHTML=mine.length?'<div class="table-wrap"><table><thead><tr><th>ID</th><th>Type</th><th>Where</th><th>Date</th><th>Assigned</th><th>Status</th></tr></thead><tbody>'+mine.map(c=>'<tr><td class="num">'+c.id+'</td><td>'+esc(c.type)+'</td><td>'+esc(c.room)+'</td><td>'+esc(c.dateDisplay)+'</td><td>'+esc(c.duty)+'</td><td><span class="pill '+c.status+'">'+(c.status==="done"?"Fixed":"Pending")+'</span></td></tr>').join("")+'</tbody></table></div>':'<div class="empty">You have not sent any complaints yet.</div>'
}
function renderSRD(){
  renderStockDatalist();
  $("srdRequests").innerHTML=state.requests.length?'<div class="table-wrap"><table><thead><tr><th>SRD</th><th>Item</th><th>Date</th><th>Day</th></tr></thead><tbody>'+state.requests.slice().reverse().map(r=>'<tr><td>'+esc(r.srd)+'</td><td>'+esc(r.item)+'</td><td>'+esc(r.dateDisplay)+'</td><td>'+esc(r.day)+'</td></tr>').join("")+'</tbody></table></div>':'<div class="empty">No requests yet.</div>'
}
function renderAdmin(){
  const pending=state.complaints.filter(c=>c.status==="pending").sort((a,b)=>a.id-b.id);
  const done=state.complaints.filter(c=>c.status==="done").sort((a,b)=>b.id-a.id).slice(0,20);
  $("adminPendingCount").textContent=pending.length;$("adminDoneCount").textContent=state.complaints.filter(c=>c.status==="done").length;$("adminStockCount").textContent=Object.keys(state.stock).length;$("adminRequestCount").textContent=state.requests.length;$("pendingBadge").textContent=pending.length+" pending";
  $("pendingList").innerHTML=pending.length?pending.map(c=>jobHTML(c)).join(""):'<div class="empty">Nothing is waiting. New complaints will appear here.</div>';
  $("stockTable").innerHTML='<div class="table-wrap"><table><thead><tr><th>Item</th><th>In stock</th></tr></thead><tbody>'+Object.entries(state.stock).sort((a,b)=>a[0].localeCompare(b[0])).map(([item,qty])=>'<tr><td>'+esc(item)+'</td><td class="num '+(qty===0?"danger":"")+'">'+qty+'</td></tr>').join("")+'</tbody></table></div>';
  $("adminRequests").innerHTML=state.requests.length?'<div class="table-wrap"><table><thead><tr><th>SRD</th><th>Item</th><th>Date</th><th>Day</th></tr></thead><tbody>'+state.requests.slice().reverse().map(r=>'<tr><td>'+esc(r.srd)+'</td><td>'+esc(r.item)+'</td><td>'+esc(r.dateDisplay)+'</td><td>'+esc(r.day)+'</td></tr>').join("")+'</tbody></table></div>':'<div class="empty">No SRD requests yet.</div>';
}
function jobHTML(c){
  const items=[0,1,2].map(i=>'<div class="item-grid"><input id="item-'+c.id+'-'+i+'" placeholder="Item used"><input id="qty-'+c.id+'-'+i+'" type="number" min="1" placeholder="Qty"></div>').join("");
  return '<article class="job '+(c.duty===currentUser.name?"mine":"")+'"><div class="job-head"><span class="num">#'+c.id+'</span><span class="job-title">'+esc(c.type)+'</span><span>·</span><span>'+esc(c.room)+'</span>'+(c.duty===currentUser.name?'<span class="pill done">Yours</span>':"")+'</div><p class="meta">'+esc(c.student)+' · '+esc(c.dateDisplay)+' ('+esc(c.day)+') · assigned to '+esc(c.duty)+(c.details?"<br>"+esc(c.details):"")+'</p><div class="details-box"><div class="item-grid-label"><strong>Items used from stock</strong></div>'+items+'<div class="complete-actions"><label>Cost<input id="cost-'+c.id+'" type="number" min="0" step="0.01" value="0"></label><button class="primary" onclick="completeRepair('+c.id+')">Mark as fixed</button></div></div></article>'
}
function completeRepair(id){
  const c=state.complaints.find(x=>x.id===id);if(!c)return;
  const used=[];
  for(let i=0;i<3;i++){
    const item=$("item-"+id+"-"+i).value.trim(),qty=Math.max(0,parseInt($("qty-"+id+"-"+i).value||"0",10));
    if(!item||!qty)continue;
    const key=Object.keys(state.stock).find(x=>x.toLowerCase()===item.toLowerCase());
    const name=key||item,have=state.stock[key??name]||0,take=Math.min(qty,have);
    if(take<qty)showToast("Only "+have+" of "+name+" is in stock. Recorded "+take+".","warn");
    if(take>0){state.stock[name]=have-take;used.push(name+" x"+take)}
  }
  c.status="done";c.cost=Math.max(0,Number($("cost-"+id).value)||0);c.itemsUsed=used.join("; ")||"—";c.doneOn=new Date().toLocaleDateString("en-IN");c.doneBy=currentUser.name;save();renderAdmin();showToast("Repair #"+id+" marked as fixed.")
}
$("loginForm").addEventListener("submit",e=>{e.preventDefault();const role=document.querySelector(".role-tab.active").dataset.role;login(role,$("ident").value.trim(),$("password").value)})
document.querySelectorAll(".role-tab").forEach(btn=>btn.addEventListener("click",()=>{document.querySelectorAll(".role-tab").forEach(x=>x.classList.remove("active"));btn.classList.add("active");const role=btn.dataset.role;const label=role==="student"?"Registration number":role==="srd"?"Your name":"Admin ID";$("identLabel").textContent=label;$("ident").placeholder=role==="student"?"Enter registration number":role==="srd"?"Enter your name":"e.g. 101";$("ident").inputMode=role==="srd"?"text":"numeric"}))
$("complaintForm").addEventListener("submit",e=>{e.preventDefault();const iso=$("complaintDate").value;const id=(state.complaints.reduce((m,c)=>Math.max(m,c.id),100))+1;const duty=DUTIES[iso]||"Unassigned";const c={id,student:currentUser.name,regno:currentUser.regno,type:$("complaintType").value,room:$("complaintRoom").value.trim(),details:$("complaintDetails").value.trim(),dateDisplay:new Date(iso+"T12:00:00").toLocaleDateString("en-IN"),day:dayName(iso),duty,status:"pending"};if(!c.room){showToast("Please enter where the problem is.","error");return}state.complaints.push(c);save();e.target.reset();setDates();renderStudent();showToast("Complaint #"+id+" saved. Assigned to "+duty+".")})
$("srdForm").addEventListener("submit",e=>{e.preventDefault();const iso=$("srdDate").value,item=$("srdItem").value.trim();const key=Object.keys(state.stock).find(x=>x.toLowerCase()===item.toLowerCase());const qty=key?state.stock[key]:0;if(!item){showToast("Enter an item.","error");return}if(qty<=0){showToast(item+" is not in stock.","error");return}state.requests.push({srd:currentUser.name,item:key||item,dateDisplay:new Date(iso+"T12:00:00").toLocaleDateString("en-IN"),day:dayName(iso)});save();e.target.reset();setDates();renderSRD();showToast("Request saved. "+item+" is available ("+qty+" in stock).")})
$("stockForm").addEventListener("submit",e=>{e.preventDefault();const item=$("stockItem").value.trim(),qty=parseInt($("stockQty").value||"0",10);if(!item||qty<0){showToast("Enter a valid item and quantity.","error");return}const key=Object.keys(state.stock).find(x=>x.toLowerCase()===item.toLowerCase());state.stock[key||item]=(state.stock[key||item]||0)+qty;save();e.target.reset();renderAdmin();renderStockDatalist();showToast("Added "+qty+" to "+(key||item)+".")})
$("passwordForm").addEventListener("submit",e=>{e.preventDefault();const id=currentUser.id,current=$("currentPassword").value,newPw=$("newPassword").value,again=$("againPassword").value,expected=state.adminPasswords[id]||START_PASSWORD;if(current!==expected){showToast("Current password is wrong.","error");return}if(newPw.length<8||newPw!==again){showToast("New passwords must match and contain at least 8 characters.","error");return}state.adminPasswords[id]=newPw;save();e.target.reset();showToast("Password changed successfully.")})
setDates();renderStockDatalist();showView("loginView");
