"use strict";
// PeopleFlow real SaaS frontend: all mutations and authorization are handled by the server.
const $=id=>document.getElementById(id);
const state={user:null,csrf:null,page:"overview",employees:[],sectionLoading:false};
const html=x=>String(x??"").replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#39;"})[c]);
const money=n=>new Intl.NumberFormat("en-IN",{style:"currency",currency:"INR",maximumFractionDigits:0}).format(Number(n||0));
const dateNow=()=>{const d=new Date();return new Date(d.getTime()-d.getTimezoneOffset()*60000).toISOString().slice(0,10);};
const initials=s=>String(s||"?").split(" ").map(x=>x[0]).slice(0,2).join("").toUpperCase();
const roleStaff=()=>["Admin","HR"].includes(state.user?.role);
const formatDay=d=>d?new Date(d+"T00:00:00").toLocaleDateString("en-IN",{day:"2-digit",month:"short",year:"numeric"}):"—";
const statusBadge=s=>`<span class="badge ${["Pending","On Leave","Remote"].includes(s)?"warn":["Inactive","Absent","Rejected"].includes(s)?"no":""}">${html(s)}</span>`;
const pages=[{id:"overview",title:"Overview",icon:"▦"},{id:"employees",title:"Employees",icon:"♙"},{id:"attendance",title:"Attendance",icon:"◷"},{id:"leaves",title:"Leave requests",icon:"▤"},{id:"payroll",title:"Payroll estimate",icon:"▥",staff:true},{id:"users",title:"User accounts",icon:"⚿",admin:true},{id:"audit",title:"Audit log",icon:"☷",admin:true},{id:"settings",title:"Settings",icon:"⚙"}];
function notify(message,bad=false){const t=$("toast");t.textContent=message;t.className="toast"+(bad?" bad":"");clearTimeout(notify.timer);notify.timer=setTimeout(()=>t.classList.add("hidden"),3500);}
async function request(path,{method="GET",data=null,raw=false}={}){
  const options={method,credentials:"same-origin",headers:{}};
  if(method!=="GET"){
    options.headers["Content-Type"]="application/x-www-form-urlencoded";
    if(state.csrf)options.headers["X-CSRF-Token"]=state.csrf;
    options.body=new URLSearchParams(data||{}).toString();
  }
  const res=await fetch(path,options);
  if(raw){if(!res.ok)throw new Error("Export failed");return res;}
  let result;try{result=await res.json()}catch{throw new Error("Invalid response from the PeopleFlow server");}
  if(!res.ok||!result.ok)throw new Error(result.error||"Request failed");
  return result.data;
}
async function safe(task){try{await task()}catch(e){notify(e.message||"Something went wrong",true);}}
function showLogin(){state.user=null;state.csrf=null;$("login").classList.remove("hidden");$("workspace").classList.add("hidden");}
function switchAuth(signup){
  $("loginForm").classList.toggle("hidden", signup);
  $("signupForm").classList.toggle("hidden", !signup);
  $("authTitle").textContent=signup?"Start your workspace.":"Welcome back.";
  $("authSubtitle").textContent=signup?"Create your organization's private HR workspace.":"Sign in to manage your team.";
  $("authSwitchText").textContent=signup?"Already have an account?":"New to PeopleFlow?";
  $("authSwitchButton").textContent=signup?"Sign in":"Create an organization";
  $("authSwitchButton").dataset.signup=String(signup);
  $("loginError").classList.add("hidden");
}
async function loadMe(){const info=await request("/api/me");state.user=info.user;state.csrf=info.csrf;$("login").classList.add("hidden");$("workspace").classList.remove("hidden");
  $("userName").textContent=state.user.name;$("userRole").textContent=state.user.role;$("avatar").textContent=initials(state.user.name);$("orgSidebar").textContent=state.user.organization;
  drawNav();await navigate(state.page);
}
function drawNav(){const nav=$("navigation");nav.innerHTML=pages.filter(p=>(!p.staff||roleStaff())&&(!p.admin||state.user.role==="Admin")).map(p=>
  `<button class="nav-link ${p.id===state.page?"active":""}" data-page="${p.id}"><span class="nav-icon">${p.icon}</span>${p.title}</button>`).join("");}
const head=(title,subtitle,actions="")=>`<section class="page-head"><div><div class="eyeline">PEOPLEFLOW WORKSPACE</div><h1>${html(title)}</h1><p>${html(subtitle)}</p></div><div class="head-actions">${actions}</div></section>`;
const table=(columns,rows,empty="Nothing here yet")=>`<div class="table-wrap"><table class="data-table"><thead><tr>${columns.map(c=>`<th>${html(c)}</th>`).join("")}</tr></thead><tbody>${rows.length?rows.join(""):`<tr><td class="empty" colspan="${columns.length}">${html(empty)}</td></tr>`}</tbody></table></div>`;
const card=(label,value,icon,foot)=>`<div class="kpi"><div class="mini">${html(label)}<span class="kpi-icon">${icon}</span></div><div class="number">${value}</div><div class="foot">${html(foot)}</div></div>`;
const userCell=name=>`<span class="mini-user"><span class="initials">${html(initials(name))}</span><strong>${html(name)}</strong></span>`;
async function navigate(page){const valid=pages.find(p=>p.id===page);if(!valid||(valid.staff&&!roleStaff())||(valid.admin&&state.user.role!=="Admin"))page="overview";
  state.page=page;drawNav();$("breadcrumb").textContent=pages.find(p=>p.id===page).title;
  $("content").innerHTML="<p class='muted'>Loading your workspace…</p>";
  $("menuToggle").closest(".workspace").querySelector(".sidebar").classList.remove("open");
  try{await ({overview,employees,attendance,leaves,payroll,users,audit,settings}[page])()}catch(e){
    $("content").innerHTML=`${head("Unable to load","Please try again.")}<div class="panel panel-pad">${html(e.message)}</div>`;
    if(String(e.message).includes("sign in"))showLogin();
  }
}
async function overview(){const d=await request("/api/dashboard");const staff=roleStaff();
  const cards=staff?[card("Total employees",d.employees,"♙","Across all departments"),card("Active team members",d.active,"◉","Currently marked active"),card("Pending leave requests",d.pending,"▤","Awaiting HR approval"),card("Monthly salary estimate",money(d.payroll),"₹","Base salary ÷ 12")]
    :[card("My profile",state.user.name.split(" ")[0],"♙","Personal employee workspace"),card("Attendance days",d.attendance_days,"◷","Recorded present or remote"),card("Pending leave requests",d.pending,"▤","Awaiting a decision"),card("My role",state.user.role,"◇","Access based on account role")];
  const departments=d.departments||[];const max=Math.max(1,...departments.map(x=>x.headcount));
  $("content").innerHTML=head(`Hello, ${state.user.name.split(" ")[0]} 👋`,"Here is what's happening in your workspace today.",`<button class="btn outline" data-page="employees">View team →</button>`)+
    `<div class="kpi-grid">${cards.join("")}</div><div class="split"><div class="panel panel-pad"><div class="panel-title"><div><h3>${staff?"Team by department":"Your workspace"}</h3><p>${staff?"How your team is distributed":"Manage your time, records, and requests"}</p></div></div>${staff?departments.map(x=>`<div class="dept-row"><strong>${html(x.department)}</strong><div class="progress"><span style="width:${Math.min(100,100*x.headcount/max)}%"></span></div><span>${x.headcount}</span></div>`).join(""):"<div class='note-box'><strong>Everything in one place.</strong>Check your employee record, track your attendance and request time off using the navigation.</div>"}</div><div class="panel panel-pad"><div class="panel-title"><div><h3>Quick actions</h3><p>Things you can do right now</p></div></div><div class="note-box"><strong>Take care of your people ✳</strong>Manage records and approvals with clear, role-based workflows.</div><div style="display:grid;gap:10px;margin-top:18px"><button data-page="leaves" class="btn outline">Manage leave requests →</button><button data-page="attendance" class="btn outline">Open attendance →</button></div></div></div>`;
}
async function employees(){state.employees=await request("/api/employees");
  $("content").innerHTML=head("People directory","Your team's important details, thoughtfully organized.",roleStaff()?`<button id="addEmployee" class="btn primary">＋ Add employee</button><a class="btn outline" href="/api/export/employees">↓ Export CSV</a>`:"")+
    `<div class="panel"><div class="toolbar"><div><strong>All team members</strong><span class="muted"> · ${state.employees.length} records</span></div><input id="employeeSearch" class="search-input" placeholder="Search employees, emails, departments…" type="search" aria-label="Search employees"></div><div id="employeesTable"></div></div>`;
  drawEmployees();$("employeeSearch").addEventListener("input",drawEmployees);
  if(roleStaff())$("addEmployee").addEventListener("click",()=>employeeModal());
}
function drawEmployees(){const search=($("employeeSearch")?.value||"").toLowerCase();const rows=state.employees.filter(e=>[e.name,e.email,e.department,e.job_title].some(v=>String(v).toLowerCase().includes(search))).map(e=>
  `<tr><td>${userCell(e.name)}</td><td>${html(e.email)}</td><td>${html(e.department)}</td><td>${html(e.job_title)}</td><td>${money(e.annual_salary)}</td><td>${statusBadge(e.status)}</td><td>${roleStaff()?`<div class="actions"><button class="icon-button" data-edit="${e.id}">Edit</button><button class="icon-button" data-delete="${e.id}">Deactivate</button></div>`:"—"}</td></tr>`);
  $("employeesTable").innerHTML=table(["Employee","Email","Department","Position","Annual salary","Status","Actions"],rows,"No employees match your search");
  $("employeesTable").querySelectorAll("[data-edit]").forEach(btn=>btn.addEventListener("click",()=>employeeModal(state.employees.find(e=>e.id==btn.dataset.edit))));
  $("employeesTable").querySelectorAll("[data-delete]").forEach(btn=>btn.addEventListener("click",()=>safe(async()=>{
    const e=state.employees.find(x=>x.id==btn.dataset.delete);
    if(!confirm(`Deactivate ${e?.name}? Their attendance and leave history will be retained.`))return;
    await request(`/api/employees/${btn.dataset.delete}`,{method:"DELETE"});notify("Employee deactivated");await navigate("employees");
  })));
}
function openModal(title,desc,fields,submit){const box=$("modalContent");box.innerHTML=`<h2>${html(title)}</h2><p>${html(desc)}</p><form id="modalForm"><div class="formgrid modal-grid">${fields}</div><div class="modal-foot"><button type="button" class="btn outline" id="cancelModal">Cancel</button><button class="btn primary" type="submit">Save changes</button></div></form>`;
  const dialog=$("modal");dialog.showModal();$("cancelModal").onclick=()=>dialog.close();$("modalForm").onsubmit=e=>{e.preventDefault();safe(async()=>{const button=$("modalForm").querySelector("[type=submit]");button.disabled=true;try{await submit(Object.fromEntries(new FormData(e.target).entries()));dialog.close();notify("Saved successfully");await navigate(state.page);}finally{button.disabled=false;}});};}
const formField=(label,name,value="",type="text",options=null)=>`<label>${html(label)}${options?`<select name="${name}" required>${options.map(v=>`<option ${v===value?"selected":""}>${html(v)}</option>`).join("")}</select>`:`<input name="${name}" type="${type}" value="${html(value)}" ${type==="number"?'min="0" step="1"':''} required>`}</label>`;
function employeeModal(e=null){openModal(e?"Edit employee":"Add employee","Changes will be saved to your organization database.",[
  formField("Full name","name",e?.name),formField("Work email","email",e?.email,"email"),
  formField("Department","department",e?.department),formField("Job title","job_title",e?.job_title),
  formField("Annual base salary (₹)","annual_salary",e?.annual_salary||0,"number"),formField("Joined on","joined_on",e?.joined_on||dateNow(),"date"),
  formField("Employment status","status",e?.status||"Active","text",["Active","On Leave","Inactive"])].join(""),
  data=>request(e?`/api/employees/${e.id}`:"/api/employees",{method:e?"PUT":"POST",data}));}
async function attendance(){const [entries,emps]=await Promise.all([request("/api/attendance"),roleStaff()?request("/api/employees"):Promise.resolve([])]);
  $("content").innerHTML=head("Attendance","Keep a clear daily record of who's here and who's away.",roleStaff()?`<a class="btn outline" href="/api/export/attendance">↓ Export CSV</a>`:"")+
    `<div class="panel">${roleStaff()?`<form id="attendanceForm" class="form-panel"><div class="formgrid"><label>Employee<select name="employee_id" required>${emps.map(e=>`<option value="${e.id}">${html(e.name)} · ${html(e.department)}</option>`).join("")}</select></label>${formField("Date","day",dateNow(),"date")}${formField("Status","status","Present","text",["Present","Absent","Remote"])}<div class="form-actions"><button class="btn primary" type="submit">Mark attendance</button></div></div><div class="statusline">Marking the same employee and date again updates that record.</div></form>`:""}<div class="toolbar"><strong>Attendance log</strong><span class="muted">Latest ${entries.length} records</span></div>${table(["Employee","Date","Status"],entries.map(e=>`<tr><td>${userCell(e.employee)}</td><td>${formatDay(e.day)}</td><td>${statusBadge(e.status)}</td></tr>`),"No attendance records yet")}</div>`;
  if(roleStaff())$("attendanceForm").addEventListener("submit",e=>{e.preventDefault();safe(async()=>{await request("/api/attendance",{method:"POST",data:Object.fromEntries(new FormData(e.target))});notify("Attendance recorded");await navigate("attendance");});});
}
async function leaves(){const [entries,emps]=await Promise.all([request("/api/leaves"),roleStaff()?request("/api/employees"):Promise.resolve([])]);
  const pending=entries.filter(x=>x.status==="Pending").length;
  $("content").innerHTML=head("Leave management","A simple way to request time away and approve applications.")+
    `<div class="kpi-grid">${card("Total requests",entries.length,"▤","Visible to your role")}${card("Awaiting review",pending,"◷","Pending leave decisions")}${card("Approved",entries.filter(x=>x.status==="Approved").length,"✓","Reviewed and accepted")}${card("Rejected",entries.filter(x=>x.status==="Rejected").length,"○","Reviewed and declined")}</div>`+
    `<div class="panel"><div class="panel-title panel-pad"><div><h3>Request time off</h3><p>One request can cover multiple days.</p></div></div><form class="form-panel" id="leaveForm"><div class="formgrid">${roleStaff()?`<label>Employee<select name="employee_id" required>${emps.map(e=>`<option value="${e.id}">${html(e.name)}</option>`).join("")}</select></label>`:""}${formField("Start date","start_date",dateNow(),"date")}${formField("End date","end_date",dateNow(),"date")}<label class="wide">Reason<textarea name="reason" rows="2" maxlength="500" required placeholder="e.g. Personal appointment"></textarea></label><div class="form-actions"><button class="btn primary">Submit request</button></div></div></form></div>`+
    `<div class="panel"><div class="toolbar"><strong>Leave requests</strong><span class="muted">${entries.length} requests</span></div>${table(["Employee","From","To","Reason","Status","Action"],entries.map(e=>`<tr><td>${userCell(e.employee)}</td><td>${formatDay(e.start_date)}</td><td>${formatDay(e.end_date)}</td><td>${html(e.reason)}</td><td>${statusBadge(e.status)}</td><td>${roleStaff()&&e.status==="Pending"?`<div class="actions"><button class="icon-button" data-review="${e.id}" data-status="Approved">Approve</button><button class="icon-button" data-review="${e.id}" data-status="Rejected">Reject</button></div>`:"—"}</td></tr>`),"No leave requests yet")}</div>`;
  $("leaveForm").addEventListener("submit",e=>{e.preventDefault();safe(async()=>{await request("/api/leaves",{method:"POST",data:Object.fromEntries(new FormData(e.target))});notify("Leave request submitted");await navigate("leaves");});});
  $("content").querySelectorAll("[data-review]").forEach(btn=>btn.addEventListener("click",()=>safe(async()=>{await request(`/api/leaves/${btn.dataset.review}`,{method:"PUT",data:{status:btn.dataset.status}});notify(`Leave ${btn.dataset.status.toLowerCase()}`);await navigate("leaves");})));
}
async function payroll(){const entries=await request("/api/payroll");const total=entries.reduce((n,e)=>n+e.monthly_estimate,0);
  $("content").innerHTML=head("Payroll overview","Estimated base salary costs (illustrative, not payroll processing).",`<a class="btn outline" href="/api/export/employees">↓ Export employees</a>`)+
    `<div class="kpi-grid">${card("Active payroll employees",entries.length,"♙","Excludes inactive records")}${card("Estimated monthly salary",money(total),"₹","Annual base salary / 12")}${card("Estimated annual salary",money(entries.reduce((n,e)=>n+e.annual_salary,0)),"▥","Sum of annual base salaries")}${card("Important", "Estimate", "ⓘ", "Not statutory payroll")}</div><div class="panel"><div class="toolbar"><strong>Monthly salary estimates</strong><span class="muted">No taxes or statutory deductions calculated</span></div>${table(["Employee","Department","Annual base salary","Monthly estimate"],entries.map(e=>`<tr><td>${userCell(e.name)}</td><td>${html(e.department)}</td><td>${money(e.annual_salary)}</td><td><strong>${money(e.monthly_estimate)}</strong></td></tr>`))}</div><p class="small-note" style="margin-top:18px">PeopleFlow does not calculate tax, provident fund, insurance, paid leave encashment, overtime, or payslips.</p>`;
}
async function users(){const [users,emps]=await Promise.all([request("/api/users"),request("/api/employees")]);
  $("content").innerHTML=head("Access management","Create secure team accounts within your organization.",`<button id="addUser" class="btn primary">＋ Add account</button>`)+
    `<div class="panel"><div class="toolbar"><strong>Registered accounts</strong><span class="muted">Only Admin can access this page</span></div>${table(["Name","Email","Role","Linked employee"],users.map(u=>`<tr><td>${userCell(u.name)}</td><td>${html(u.email)}</td><td>${statusBadge(u.role)}</td><td>${html(emps.find(e=>e.id===u.employee_id)?.name||"—")}</td></tr>`))}</div><p class="small-note" style="margin-top:18px">Employee accounts must be linked to an employee record. Share credentials through a secure channel and ask each member to change their password after first sign-in.</p>`;
  $("addUser").addEventListener("click",()=>{const options=`<label>Linked employee (required for Employee role)<select name="employee_id"><option value="">None</option>${emps.map(e=>`<option value="${e.id}">${html(e.name)}</option>`).join("")}</select></label>`;
    openModal("Add user account","Admin can create a login with server-enforced permissions",[formField("Full name","name"),formField("Email","email","","email"),formField("Role","role","Employee","text",["Employee","HR","Admin"]),options,formField("Initial password (12+ characters)","password","","password")].join(""),data=>request("/api/users",{method:"POST",data}));});
}
async function audit(){const entries=await request("/api/audit");$("content").innerHTML=head("Audit log","Recent changes recorded by the backend (latest 200).")+
  `<div class="panel"><div class="toolbar"><strong>Activity history</strong><span class="muted">Admin only</span></div>${table(["Date / time","Actor","Action","Record","ID"],entries.map(e=>`<tr><td>${html(e.created_at)}</td><td>${userCell(e.actor)}</td><td><strong>${html(e.action.replaceAll("_"," "))}</strong></td><td>${html(e.entity)}</td><td>${html(e.entity_id)}</td></tr>`),"No recorded changes yet")}</div>`;}
async function settings(){$("content").innerHTML=head("Account settings","Keep your organization account secure.")+
  `<div class="panel panel-pad settings-card"><div class="panel-title"><div><h3>Update password</h3><p>Your other sessions will be signed out.</p></div></div><form id="passwordForm"><div class="formgrid">${formField("Current password","old_password","","password")}${formField("New password (12+ characters)","new_password","","password")}<div class="form-actions"><button class="btn primary">Update password</button></div></div></form></div><div class="panel panel-pad settings-card"><h3>Data and privacy</h3><p class="small-note">PeopleFlow saves your organization data on the application server, not in your browser. Only authorized accounts may view team information. Payroll shows base-salary estimates, not statutory payroll processing. Your deployment administrator is responsible for backups, privacy compliance and account security.</p></div>`;
  $("passwordForm").addEventListener("submit",e=>{e.preventDefault();safe(async()=>{await request("/api/password",{method:"POST",data:Object.fromEntries(new FormData(e.target))});notify("Password updated");e.target.reset();});});}
window.addEventListener("DOMContentLoaded",async()=>{
  $("loginForm").addEventListener("submit",e=>{e.preventDefault();safe(async()=>{
    const btn=$("loginForm").querySelector("[type=submit]");btn.disabled=true;$("loginError").classList.add("hidden");
    try{await request("/api/login",{method:"POST",data:Object.fromEntries(new FormData(e.target))});await loadMe();}
    catch(err){$("loginError").textContent=err.message;$("loginError").classList.remove("hidden");}
    finally{btn.disabled=false;}
  });});
  $("authSwitchButton").addEventListener("click",e=>switchAuth(e.currentTarget.dataset.signup!=="true"));
  $("signupForm").addEventListener("submit",e=>{e.preventDefault();safe(async()=>{
    const btn=$("signupForm").querySelector("[type=submit]");btn.disabled=true;
    try {await request("/api/register",{method:"POST",data:Object.fromEntries(new FormData(e.target))});await loadMe();notify("Your workspace has been created");}
    catch(err){$("loginError").textContent=err.message;$("loginError").classList.remove("hidden");}
    finally{btn.disabled=false;}
  });});
  $("navigation").addEventListener("click",e=>{const b=e.target.closest("[data-page]");if(b)navigate(b.dataset.page);});
  $("content").addEventListener("click",e=>{const b=e.target.closest("[data-page]");if(b)navigate(b.dataset.page);});
  $("menuToggle").addEventListener("click",()=>document.querySelector(".sidebar").classList.toggle("open"));
  $("logout").addEventListener("click",()=>safe(async()=>{await request("/api/logout",{method:"POST"});showLogin();}));
  try{await loadMe()}catch{showLogin()}
});
