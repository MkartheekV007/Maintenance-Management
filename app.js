const SUPABASE_URL = "https://flmzdktzswjjofpsclbc.supabase.co";
const SUPABASE_KEY = "sb_publishable_ApoNq1biHliViu9G9t1LzQ_8KooRxt6";
const { createClient } = window.supabase;
const db = createClient(SUPABASE_URL, SUPABASE_KEY);

const DUTIES = {
  "2026-09-22":"Chhayank","2026-09-23":"Lutharshan","2026-09-24":"Charan",
  "2026-09-25":"Sashank","2026-09-26":"murali","2026-09-27":"Aditya",
  "2026-09-28":"Sathwik","2026-09-29":"Preetam","2026-09-30":"Chhayank",
  "2026-10-01":"Lutharshan","2026-10-02":"Charan","2026-10-03":"Sashank",
  "2026-10-04":"murali","2026-10-05":"Aditya","2026-10-06":"Sathwik",
  "2026-10-07":"Preetam","2026-10-08":"Chhayank","2026-10-09":"Lutharshan",
  "2026-10-10":"Charan","2026-10-11":"Sashank","2026-10-12":"murali",
  "2026-10-13":"Aditya","2026-10-14":"Sathwik","2026-10-15":"Preetam"
};

let currentRole = null;
let currentUser = null;
const $ = id => document.getElementById(id);

function today(){ return new Date().toISOString().slice(0,10); }
function dayName(iso){ return new Date(iso+"T12:00:00").toLocaleDateString("en-IN",{weekday:"long"}); }
function esc(v){ return String(v ?? "").replace(/[&<>"']/g,m=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[m])); }

function showToast(message,type="ok"){
  const el=document.createElement("div");
  el.className="toast "+type;
  el.textContent=message;
  $("toastArea").prepend(el);
  setTimeout(()=>el.remove(),4500);
}

function showView(name){
  ["loginView","studentView","srdView","adminView"].forEach(x=>$(x).classList.add("hidden"));
  $(name).classList.remove("hidden");
}

function setUserBar(){
  const bar=$("userBar");
  if(!currentRole){
    bar.classList.add("hidden");
    bar.innerHTML="";
    return;
  }
  bar.classList.remove("hidden");
  bar.innerHTML='<div><strong>'+esc(currentUser.full_name)+'</strong><div class="user-role">'+esc(currentRole.toUpperCase())+'</div></div><button class="ghost" id="logoutBtn">Log out</button>';
  $("logoutBtn").onclick=async()=>{
    await db.auth.signOut();
    currentRole=null;
    currentUser=null;
    setUserBar();
    showView("loginView");
  };
}

function setDates(){
  ["complaintDate","srdDate"].forEach(id=>$(id).value=today());
}

async function getProfile(userId){
  const {data,error}=await db.from("profiles").select("*").eq("id",userId).single();
  if(error) throw error;
  return data;
}

async function loadSession(){
  const {data:{session}}=await db.auth.getSession();
  if(!session){
    setDates();
    showView("loginView");
    return;
  }
  try{
    currentUser=await getProfile(session.user.id);
    currentRole=currentUser.role;
    await enterPortal();
  }catch(error){
    await db.auth.signOut();
    showToast(error.message || "Could not load your profile.","error");
    showView("loginView");
  }
}

async function enterPortal(){
  setUserBar();
  setDates();
  if(currentRole==="student"){
    showView("studentView");
    await renderStudent();
  }else if(currentRole==="srd"){
    showView("srdView");
    await renderSRD();
  }else if(currentRole==="admin"){
    showView("adminView");
    await renderAdmin();
  }else{
    await db.auth.signOut();
    showToast("Your account does not have a valid role.","error");
    showView("loginView");
  }
}

async function login(email,password,selectedRole){
  const {data,error}=await db.auth.signInWithPassword({email,password});
  if(error){
    showToast(error.message || "Sign in failed.","error");
    return;
  }
  try{
    currentUser=await getProfile(data.user.id);
    currentRole=currentUser.role;
    if(currentRole!==selectedRole){
      await db.auth.signOut();
      currentUser=null;
      currentRole=null;
      showToast("This account is registered as "+currentRoleLabel(selectedRole)+"? Check the selected role.","error");
      return;
    }
    await enterPortal();
  }catch(error){
    await db.auth.signOut();
    showToast(error.message || "Could not load your profile.","error");
  }
}

function currentRoleLabel(role){
  return role==="student"?"Student":role==="srd"?"SRD":"Admin";
}

async function renderStockDatalist(){
  const {data,error}=await db.from("stock").select("item_name").order("item_name");
  if(error){ showToast(error.message,"error"); return; }
  $("stockNames").innerHTML=(data||[]).map(x=>'<option value="'+esc(x.item_name)+'">').join("");
}

async function renderStudent(){
  const {data,error}=await db.from("complaints")
    .select("*")
    .eq("student_id",currentUser.id)
    .order("created_at",{ascending:false});
  if(error){showToast(error.message,"error");return;}
  const mine=data||[];
  $("studentTotal").textContent=mine.length;
  $("studentPending").textContent=mine.filter(c=>c.status==="PENDING"||c.status==="IN PROGRESS").length;
  $("studentDone").textContent=mine.filter(c=>c.status==="FIXED").length;
  $("studentComplaints").innerHTML=mine.length
    ? '<div class="table-wrap"><table><thead><tr><th>ID</th><th>Type</th><th>Where</th><th>Date</th><th>Assigned</th><th>Status</th></tr></thead><tbody>'
      +mine.map(c=>'<tr><td class="num">#'+c.complaint_no+'</td><td>'+esc(c.complaint_type)+'</td><td>'+esc(c.location)+'</td><td>'+esc(c.complaint_date)+'</td><td>'+esc(c.assigned_to||"—")+'</td><td><span class="pill '+statusClass(c.status)+'">'+esc(statusLabel(c.status))+'</span></td></tr>').join("")
      +'</tbody></table></div>'
    : '<div class="empty">You have not sent any complaints yet.</div>';
}

function statusClass(status){
  if(status==="FIXED") return "done";
  if(status==="REJECTED") return "error";
  return "pending";
}

function statusLabel(status){
  return status==="IN PROGRESS"?"In progress":status==="FIXED"?"Fixed":status==="REJECTED"?"Rejected":"Pending";
}

async function renderSRD(){
  await renderStockDatalist();
  const {data,error}=await db.from("srd_requests")
    .select("*")
    .eq("requested_by",currentUser.id)
    .order("created_at",{ascending:false});
  if(error){showToast(error.message,"error");return;}
  $("srdRequests").innerHTML=(data||[]).length
    ? '<div class="table-wrap"><table><thead><tr><th>Item</th><th>Qty</th><th>Date</th><th>Status</th></tr></thead><tbody>'
      +(data||[]).map(r=>'<tr><td>'+esc(r.item_name)+'</td><td class="num">'+r.quantity+'</td><td>'+esc(new Date(r.created_at).toLocaleDateString("en-IN"))+'</td><td><span class="pill '+statusClass(r.status==="APPROVED"||r.status==="ISSUED"?"FIXED":r.status)+'">'+esc(r.status)+'</span></td></tr>').join("")
      +'</tbody></table></div>'
    : '<div class="empty">No requests yet.</div>';
}

async function renderAdmin(){
  const [complaintsRes,stockRes,requestsRes]=await Promise.all([
    db.from("complaints").select("*").order("created_at",{ascending:false}),
    db.from("stock").select("*").order("item_name"),
    db.from("srd_requests").select("*").order("created_at",{ascending:false})
  ]);
  if(complaintsRes.error){showToast(complaintsRes.error.message,"error");return;}
  if(stockRes.error){showToast(stockRes.error.message,"error");return;}
  if(requestsRes.error){showToast(requestsRes.error.message,"error");return;}

  const complaints=complaintsRes.data||[];
  const stock=stockRes.data||[];
  const requests=requestsRes.data||[];
  const pending=complaints.filter(c=>c.status==="PENDING"||c.status==="IN PROGRESS");
  const fixed=complaints.filter(c=>c.status==="FIXED");

  $("adminPendingCount").textContent=pending.length;
  $("adminDoneCount").textContent=fixed.length;
  $("adminStockCount").textContent=stock.length;
  $("adminRequestCount").textContent=requests.length;
  $("pendingBadge").textContent=pending.length+" pending";

  $("pendingList").innerHTML=pending.length
    ? pending.map(jobHTML).join("")
    : '<div class="empty">Nothing is waiting. New complaints will appear here.</div>';

  $("stockTable").innerHTML='<div class="table-wrap"><table><thead><tr><th>Item</th><th>In stock</th></tr></thead><tbody>'
    +stock.map(s=>'<tr><td>'+esc(s.item_name)+'</td><td class="num '+(s.quantity===0?"danger":"")+'">'+s.quantity+'</td></tr>').join("")
    +'</tbody></table></div>';

  $("adminRequests").innerHTML=requests.length
    ? '<div class="table-wrap"><table><thead><tr><th>SRD</th><th>Item</th><th>Qty</th><th>Date</th><th>Status</th></tr></thead><tbody>'
      +requests.map(r=>'<tr><td>'+esc(r.requested_by)+'</td><td>'+esc(r.item_name)+'</td><td>'+r.quantity+'</td><td>'+esc(new Date(r.created_at).toLocaleDateString("en-IN"))+'</td><td>'+esc(r.status)+'</td></tr>').join("")
      +'</tbody></table></div>'
    : '<div class="empty">No SRD requests yet.</div>';
}

function jobHTML(c){
  const items=[0,1,2].map(i=>'<div class="item-grid"><input id="item-'+c.id+'-'+i+'" placeholder="Item used"><input id="qty-'+c.id+'-'+i+'" type="number" min="1" placeholder="Qty"></div>').join("");
  return '<article class="job"><div class="job-head"><span class="num">#'+c.complaint_no+'</span><span class="job-title">'+esc(c.complaint_type)+'</span><span>·</span><span>'+esc(c.location)+'</span></div><p class="meta">'+esc(c.details)+'<br>'+esc(c.complaint_date)+' · assigned to '+esc(c.assigned_to||"Unassigned")+'</p><div class="details-box"><div class="item-grid-label"><strong>Items used from stock</strong></div>'+items+'<div class="complete-actions"><label>Cost<input id="cost-'+c.id+'" type="number" min="0" step="0.01" value="0"></label><button class="primary" onclick="completeRepair(\''+c.id+'\')">Mark as fixed</button></div></div></article>';
}

async function completeRepair(id){
  const {data:c,error}=await db.from("complaints").select("*").eq("id",id).single();
  if(error||!c){showToast(error?.message||"Complaint not found.","error");return;}

  const used=[];
  for(let i=0;i<3;i++){
    const input=$("item-"+id+"-"+i);
    const qtyInput=$("qty-"+id+"-"+i);
    if(!input||!qtyInput) continue;
    const item=input.value.trim();
    const qty=Math.max(0,parseInt(qtyInput.value||"0",10));
    if(!item||!qty) continue;

    const {data:stockRows,error:stockError}=await db.from("stock").select("*").ilike("item_name",item).limit(1);
    if(stockError){showToast(stockError.message,"error");return;}
    const stock=stockRows?.[0];
    if(!stock){showToast("Stock item not found: "+item,"error");continue;}
    if(stock.quantity<qty){
      showToast("Only "+stock.quantity+" of "+stock.item_name+" is in stock.","warn");
      continue;
    }
    const {error:updateStockError}=await db.from("stock").update({quantity:stock.quantity-qty,updated_at:new Date().toISOString()}).eq("id",stock.id);
    if(updateStockError){showToast(updateStockError.message,"error");return;}
    used.push({item_name:stock.item_name,quantity:qty});
  }

  const cost=Math.max(0,Number($("cost-"+id).value)||0);
  const {error:updateError}=await db.from("complaints").update({
    status:"FIXED",
    completed_at:new Date().toISOString(),
    completed_by:currentUser.id,
    repair_items:used,
    repair_cost:cost
  }).eq("id",id);

  if(updateError){showToast(updateError.message,"error");return;}
  await renderAdmin();
  showToast("Repair #"+c.complaint_no+" marked as fixed.");
}

$("loginForm").addEventListener("submit",async e=>{
  e.preventDefault();
  const email=$("ident").value.trim();
  const password=$("password").value;
  const role=document.querySelector(".role-tab.active").dataset.role;
  if(!email||!password){showToast("Enter your email and password.","error");return;}
  await login(email,password,role);
});

document.querySelectorAll(".role-tab").forEach(btn=>btn.addEventListener("click",()=>{
  document.querySelectorAll(".role-tab").forEach(x=>x.classList.remove("active"));
  btn.classList.add("active");
  $("identLabel").textContent="Email";
  $("ident").placeholder="Enter your email";
  $("ident").inputMode="email";
}));

$("complaintForm").addEventListener("submit",async e=>{
  e.preventDefault();
  const iso=$("complaintDate").value;
  const location=$("complaintRoom").value.trim();
  const details=$("complaintDetails").value.trim();
  if(!location){showToast("Please enter where the problem is.","error");return;}

  const {error}=await db.from("complaints").insert({
    student_id:currentUser.id,
    complaint_date:iso,
    complaint_type:$("complaintType").value,
    location,
    details,
    assigned_to:DUTIES[iso]||null,
    status:"PENDING"
  });
  if(error){showToast(error.message,"error");return;}
  e.target.reset();
  setDates();
  await renderStudent();
  showToast("Complaint submitted successfully.");
});

$("srdForm").addEventListener("submit",async e=>{
  e.preventDefault();
  const item=$("srdItem").value.trim();
  if(!item){showToast("Enter an item.","error");return;}

  const {data:stock,error:stockError}=await db.from("stock").select("*").ilike("item_name",item).limit(1);
  if(stockError){showToast(stockError.message,"error");return;}
  if(!stock?.length){showToast("That item is not in the stock list.","error");return;}
  if(stock[0].quantity<=0){showToast(item+" is not in stock.","error");return;}

  const {error}=await db.from("srd_requests").insert({
    requested_by:currentUser.id,
    item_name:stock[0].item_name,
    quantity:1,
    purpose:"Maintenance item request"
  });
  if(error){showToast(error.message,"error");return;}
  e.target.reset();
  setDates();
  await renderSRD();
  showToast("Request submitted.");
});

$("stockForm").addEventListener("submit",async e=>{
  e.preventDefault();
  const item=$("stockItem").value.trim();
  const qty=parseInt($("stockQty").value||"0",10);
  if(!item||qty<0){showToast("Enter a valid item and quantity.","error");return;}

  const {data:existing,error:findError}=await db.from("stock").select("*").ilike("item_name",item).limit(1);
  if(findError){showToast(findError.message,"error");return;}

  let error;
  if(existing?.length){
    error=(await db.from("stock").update({
      quantity:existing[0].quantity+qty,
      updated_at:new Date().toISOString()
    }).eq("id",existing[0].id)).error;
  }else{
    error=(await db.from("stock").insert({item_name:item,quantity:qty})).error;
  }
  if(error){showToast(error.message,"error");return;}
  e.target.reset();
  await renderAdmin();
  await renderStockDatalist();
  showToast("Stock updated.");
});

$("passwordForm").addEventListener("submit",async e=>{
  e.preventDefault();
  const newPw=$("newPassword").value;
  const again=$("againPassword").value;
  if(newPw.length<8||newPw!==again){
    showToast("New passwords must match and contain at least 8 characters.","error");
    return;
  }
  const {error}=await db.auth.updateUser({password:newPw});
  if(error){showToast(error.message,"error");return;}
  e.target.reset();
  showToast("Password changed successfully.");
});

db.auth.onAuthStateChange(async(event,session)=>{
  if(event==="SIGNED_OUT"){
    currentRole=null;
    currentUser=null;
    setUserBar();
    showView("loginView");
  }
});

setDates();
showView("loginView");
loadSession();
