'use strict';
// Browser UI for C++17 / SQLite HRMS with server-enforced role permissions.
const state = { token: '', user: null, view: 'dashboard', employees: [], attendance: [], leaves: [], users: [], overview: {}, search: '' };
const isManager = () => ['ADMIN','HR'].includes(state.user?.role);
const isAdmin = () => state.user?.role === 'ADMIN';
const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
const icon = name => `<svg class="icon"><use href="#i-${name}"/></svg>`;
const escapeHtml = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[c]));
const money = v => '₹' + Math.round(Number(v) || 0).toLocaleString('en-IN');
const initials = name => (String(name || '?').trim().split(/\s+/).slice(0,2).map(x => x.charAt(0).toUpperCase()).join('') || '?');
const colors = ['teal','blue','purple','orange','pink'];
const avatar = (name, id) => `<span class="avatar ${colors[(Number(id)||0)%colors.length]}">${escapeHtml(initials(name))}</span>`;
const localDate = () => { const d=new Date(); return `${d.getFullYear()}-${String(d.getMonth()+1).padStart(2,'0')}-${String(d.getDate()).padStart(2,'0')}`; };
const readableDate = date => { const d=new Date(`${date}T12:00:00`); return isNaN(d.getTime()) ? date : new Intl.DateTimeFormat('en-IN',{day:'numeric',month:'short',year:'numeric'}).format(d); };
const status = val => val === 'P' ? '<span class="pill amber">Pending</span>' : val === 'A' ? '<span class="pill green">Approved</span>' : '<span class="pill red">Rejected</span>';
const pageHeader = (eyebrow,title,subtitle,action='') => `<div class="page-heading"><div><div class="eyebrow-small">${eyebrow}</div><h1 class="page-title">${title}</h1><p class="page-description">${subtitle}</p></div>${action}</div>`;
const empty = (title, subtitle) => `<div class="empty-state"><div class="empty-symbol">✳</div><strong>${title}</strong>${subtitle}</div>`;
let toastTimer;
function toast(message, isError=false){ const el=$('#toast');el.textContent=message;el.classList.toggle('error',isError);el.hidden=false;clearTimeout(toastTimer);toastTimer=setTimeout(()=>el.hidden=true,3700); }
async function api(path, data){
  const opts={method: data === undefined ? 'GET' : 'POST', headers:{'X-Session-Token':state.token}};
  if(data !== undefined){ opts.headers['Content-Type']='application/x-www-form-urlencoded;charset=UTF-8';opts.body=new URLSearchParams(data).toString(); }
  let response;
  try{ response=await fetch('/api/'+path,opts); } catch { throw new Error('Cannot reach the C++ server. Keep the server window open.'); }
  let body;
  try{ body=await response.json(); } catch { throw new Error('Unexpected response from local server.'); }
  if(response.status===401 && path!=='login') { signOut(false); throw new Error('Session expired. Please sign in again.'); }
  if(!response.ok) throw new Error(body.error || 'Something went wrong.');
  return body;
}
async function loadAll(){
  const [employees,attendance,leaves,overview] = await Promise.all([
    api('employees'),api('attendance'),api('leaves'),api('overview')
  ]);
  const users=isAdmin()?await api('users'):[];
  Object.assign(state,{employees,attendance,leaves,overview,users});
  const pending=leaves.filter(l=>l.status==='P').length;
  $('#sidebar-pending').hidden=pending===0;
  $('#sidebar-pending').textContent=pending;
}
function signOut(callServer=true){
  if(callServer && state.token) api('logout',{}).catch(()=>{});
  state.token=''; state.user=null; $('#app-view').hidden=true; $('#login-view').hidden=false;
  $('#login-password').value='';$('#modal-root').innerHTML='';
}
$('#login-form').addEventListener('submit',async event=>{
  event.preventDefault();const button=$('.login-submit'),error=$('#login-error');error.hidden=true;button.disabled=true;
  try{
    const credentials=Object.fromEntries(new FormData(event.currentTarget));
    const data=await api('login',credentials);
    state.token=data.token;state.user=data.user;
    await loadAll();
    applyRole();
    $('#login-view').hidden=true;$('#app-view').hidden=false;
    state.view='dashboard';render();
  }catch(e){error.textContent=e.message;error.hidden=false;}
  finally{button.disabled=false;}
});
$('#logout-button').addEventListener('click',()=>signOut());
$('#change-password-button').addEventListener('click',()=>openPasswordModal());
function applyRole(){
  const u=state.user;
  $$('.admin-only').forEach(el=>el.hidden=!isAdmin());
  $('#workspace-role').textContent=(u.role==='ADMIN'?'Admin':u.role==='HR'?'HR':'Employee')+' workspace';
  $('#signed-name').textContent=u.name;
  $('#signed-role').textContent=u.role;
  for(const selector of ['#side-avatar','#top-avatar'])$(selector).textContent=initials(u.name);
}
$('#menu-toggle').addEventListener('click',()=>$('#sidebar').classList.toggle('open'));
$$('.nav-item').forEach(btn=>btn.addEventListener('click',()=>navigate(btn.dataset.view)));
function navigate(view){
  if(view==='accounts'&&!isAdmin())return;
  state.view=view;
  state.search='';
  $('#sidebar').classList.remove('open');
  render();
  window.scrollTo(0,0);
}
function render(){
  const names={dashboard:'Overview',employees:'Employees',attendance:'Attendance',leaves:'Leave requests',accounts:'User accounts'};
  $('#breadcrumb-title').textContent=names[state.view];
  $$('.nav-item').forEach(btn=>btn.classList.toggle('active',btn.dataset.view===state.view));
  const renders={dashboard:renderDashboard,employees:renderEmployees,attendance:renderAttendance,leaves:renderLeaves,accounts:renderAccounts};
  $('#main-content').innerHTML=renders[state.view]();
}
function statCard(label,value,foot,iconName,color){
  return `<div class="stat-card"><div class="stat-row"><div class="stat-label">${label}</div><span class="stat-icon ${color}">${icon(iconName)}</span></div><div><div class="stat-number">${value}</div><div class="stat-foot">${foot}</div></div></div>`;
}
function personRow(emp){
  return `<div class="person-row">${avatar(emp.name,emp.id)}<div><div class="person-name">${escapeHtml(emp.name)}</div><div class="person-meta">${escapeHtml(emp.email)}</div></div><div class="person-right">${escapeHtml(emp.department)}</div></div>`;
}
function renderDashboard(){
  const d=state.overview;
  const dep=d.departmentCounts||[];
  const max=Math.max(1,...dep.map(x=>x.count));
  const bars=dep.length?dep.map((x,i)=>`<div class="dept-row"><div class="dept-row-label"><span>${escapeHtml(x.name)}</span><span>${x.count} ${x.count===1?'employee':'employees'}</span></div><div class="bar-track"><div class="bar-fill ${['','blue','orange','purple','gray'][i%5]}" style="width:${x.count/max*100}%"></div></div></div>`).join(''):empty('No departments yet','Add an employee to see team distribution.');
  const recent=[...state.employees].sort((a,b)=>b.id-a.id).slice(0,5);
  const pending=state.leaves.filter(l=>l.status==='P');
  return `${pageHeader('YOUR WORKSPACE',`Good to see you, ${escapeHtml(state.user?.name||'there')} 👋`,'Here’s a snapshot of your people and what needs your attention.',isManager()?`<button class="button primary" data-action="add-employee">${icon('plus')} Add employee</button>`:'')}
    <section class="stats-grid">
      ${statCard('Total employees',d.employees||0,'<b>●</b> Active team members','users','')}
      ${statCard('Departments',d.departments||0,'Across your organization','grid','blue')}
      ${isManager()?statCard('Monthly base payroll',money(d.payroll||0),'Estimated from annual salaries','trend','purple'):statCard('My role',escapeHtml(state.user.role),'Personal workspace','users','purple')}
      ${statCard('Pending leaves',d.pending||0,'Requests awaiting review','leave','orange')}
    </section>
    <div class="dashboard-grid">
      <div class="panel"><div class="panel-header"><div><h3>Team by department</h3><p>How your people are distributed</p></div><span class="short-badge">Live data</span></div><div class="dept-list">${bars}</div>${dep.length?`<div class="info-strip"><strong>✦ Team insight</strong><span>Your team spans ${dep.length} ${dep.length===1?'department':'departments'}.</span></div>`:''}</div>
      <div class="panel"><div class="panel-header"><div><h3>Recently added</h3><p>Latest members of your team</p></div><button class="text-link" data-action="go-employees">View all →</button></div><div class="team-list">${recent.length?recent.map(personRow).join(''):empty('No employees yet','Add your first team member.')}</div></div>
      <div class="panel"><div class="panel-header"><div><h3>Leave requests</h3><p>Requests that need approval</p></div><button class="text-link" data-action="go-leaves">View all →</button></div>${pending.length?pending.slice(0,5).map(l=>`<div class="status-row"><div><div class="status-name">${escapeHtml(l.employee)}</div><div class="status-date">${readableDate(l.date)} • ${escapeHtml(l.reason)}</div></div>${status(l.status)}</div>`).join(''):'<div class="mini-empty">All caught up! No pending requests ✨</div>'}</div>
      <div class="panel"><div class="panel-header"><div><h3>Attendance overview</h3><p>All recorded days, across your team</p></div><button class="text-link" data-action="go-attendance">View all →</button></div><div class="status-row"><div class="status-name">Present records</div><span class="pill green">${d.present||0}</span></div><div class="status-row"><div class="status-name">Absent records</div><span class="pill red">${d.absent||0}</span></div><div class="status-row"><div class="status-name">Approved leaves</div><span class="pill blue">${d.approved||0}</span></div><div class="info-strip" style="margin-top:16px">Stats reflect all saved records, not an automatic live attendance feed.</div></div>
    </div>`;
}
function renderEmployees(){
  const list=state.employees.filter(e=>[e.name,e.department,e.email,String(e.id)].some(v=>v.toLowerCase().includes(state.search.toLowerCase())));
  return `${pageHeader('EMPLOYEE DIRECTORY','Your people',isManager()?'Manage employee information, departments, and annual salaries.':'View your employee profile and details.',isManager()?`<button class="button primary" data-action="add-employee">${icon('plus')} Add employee</button>`:'')}
    <section class="table-card"><div class="table-header"><div><h3>${isManager()?'All employees':'My employee profile'} <span class="muted">(${state.employees.length})</span></h3><p>${isManager()?'Search, edit, or remove employee records':'Your HR profile (read only)'}</p></div><div class="search-wrap">${icon('search')}<input id="employee-search" class="search-input" placeholder="Search name, email, department..." value="${escapeHtml(state.search)}" autocomplete="off"></div></div>
    <div class="table-scroll"><table><thead><tr><th>Employee</th><th>ID</th><th>Department</th><th>Annual salary</th><th>Actions</th></tr></thead><tbody id="employee-rows">${employeeRows(list)}</tbody></table></div></section>`;
}
function employeeRows(list){
  return list.length?list.map(e=>`<tr><td class="employee-name-cell">${avatar(e.name,e.id)}<div><div class="person-name">${escapeHtml(e.name)}</div><div class="person-meta">${escapeHtml(e.email)}</div></div></td><td class="id-mark">#${e.id}</td><td><span class="pill blue">${escapeHtml(e.department)}</span></td><td>${money(e.salary)}</td><td>${isManager()?`<div class="table-actions"><button class="icon-button" data-action="edit-employee" data-id="${e.id}" title="Edit employee">${icon('edit')}</button><button class="icon-button danger" data-action="delete-employee" data-id="${e.id}" title="Delete employee">${icon('trash')}</button></div>`:'<span class="muted">Read only</span>'}</td></tr>`).join(''):`<tr><td colspan="5">${empty('No matching employees','Try a different search or add someone new.')}</td></tr>`;
}
function employeeOptions(){return state.employees.map(e=>`<option value="${e.id}">${escapeHtml(e.name)} (#${e.id})</option>`).join('');}
function renderAttendance(){
  const sorted=[...state.attendance].sort((a,b)=>b.date.localeCompare(a.date)||a.employee.localeCompare(b.employee));
  return `${pageHeader('TIME & ATTENDANCE','Track attendance','Save daily presence and review existing records.')}
    ${isManager()?`<section class="panel form-panel"><h3>Mark attendance</h3><p>Recording the same employee and date again will update their status.</p><form id="attendance-form" class="inline-form"><div class="field"><label for="att-emp">Employee</label><select required id="att-emp" name="employeeId"><option value="">Select employee</option>${employeeOptions()}</select></div><div class="field"><label for="att-date">Date</label><input required type="date" id="att-date" name="date" value="${localDate()}"></div><div class="field"><label for="att-status">Status</label><select id="att-status" name="status"><option value="P">Present</option><option value="A">Absent</option></select></div><div class="field"><label>&nbsp;</label><button type="submit" class="button primary" style="height:43px;width:100%">Save attendance</button></div></form></section>`:''}
    <section class="table-card"><div class="table-header"><div><h3>Attendance records</h3><p>${sorted.length} saved records • Most recent first</p></div><span class="short-badge">Saved locally</span></div><div class="table-scroll"><table><thead><tr><th>Employee</th><th>Date</th><th>Status</th></tr></thead><tbody>${sorted.length?sorted.map(a=>`<tr><td class="employee-name-cell">${avatar(a.employee,a.employeeId)}<span>${escapeHtml(a.employee)}</span></td><td>${readableDate(a.date)}</td><td>${a.status==='P'?'<span class="pill green">Present</span>':'<span class="pill red">Absent</span>'}</td></tr>`).join(''):`<tr><td colspan="3">${empty('No attendance records','Mark attendance to get started.')}</td></tr>`}</tbody></table></div></section>`;
}
function renderLeaves(){
  const sorted=[...state.leaves].sort((a,b)=>b.id-a.id);
  return `${pageHeader('TIME OFF','Manage leave requests',isManager()?'Submit leave requests and approve or reject them from one place.':'Request leave and view the approval status.')}
    <section class="panel form-panel"><h3>New leave request</h3><p>For this portfolio demo, each request represents one day of leave.</p><form id="leave-form" class="inline-form leave-form">${isManager()?`<div class="field"><label for="leave-emp">Employee</label><select id="leave-emp" name="employeeId" required><option value="">Select employee</option>${employeeOptions()}</select></div>`:`<input type="hidden" name="employeeId" value="${state.user.employeeId}">`}<div class="field"><label for="leave-date">Leave date</label><input id="leave-date" type="date" name="date" required value="${localDate()}"></div><div class="field"><label for="leave-reason">Reason</label><input id="leave-reason" name="reason" required maxlength="200" placeholder="e.g. Personal day"></div><div class="field"><label>&nbsp;</label><button class="button primary" type="submit" style="height:43px;width:100%">Submit request</button></div></form></section>
    <section class="table-card"><div class="table-header"><div><h3>All leave requests</h3><p>Review and keep track of employee leave</p></div><span class="short-badge">${state.leaves.filter(x=>x.status==='P').length} waiting</span></div><div class="table-scroll"><table><thead><tr><th>Employee</th><th>Leave date</th><th>Reason</th><th>Status</th><th>Action</th></tr></thead><tbody>${sorted.length?sorted.map(l=>`<tr><td class="employee-name-cell">${avatar(l.employee,l.employeeId)}<span>${escapeHtml(l.employee)}</span></td><td>${readableDate(l.date)}</td><td>${escapeHtml(l.reason)}</td><td>${status(l.status)}</td><td>${isManager()&&l.status==='P'?`<div class="action-buttons"><button class="button small primary" data-action="review-leave" data-id="${l.id}" data-decision="approve">Approve</button><button class="button small danger" data-action="review-leave" data-id="${l.id}" data-decision="reject">Reject</button></div>`:'<span class="muted">Reviewed</span>'}</td></tr>`).join(''):`<tr><td colspan="5">${empty('No leave requests','Submit your first request above.')}</td></tr>`}</tbody></table></div></section>`;
}
function renderAccounts(){
  if(!isAdmin())return empty('Access denied','Only admins can manage accounts.');
  return `${pageHeader('ACCESS CONTROL','User accounts','Create HR and employee accounts with separate permissions.',`<button class="button primary" data-action="add-account">${icon('plus')} Create account</button>`)}
  <section class="table-card"><div class="table-header"><div><h3>All user accounts</h3><p>Passwords are hashed with PBKDF2-SHA256 in SQLite.</p></div><span class="short-badge">Admin access</span></div>
  <div class="table-scroll"><table><thead><tr><th>Account</th><th>Role</th><th>Linked employee</th><th>Action</th></tr></thead><tbody>${state.users.map(u=>`<tr><td><strong>${escapeHtml(u.name)}</strong><div class="person-meta">${escapeHtml(u.email)}</div></td><td><span class="pill blue">${escapeHtml(u.role)}</span></td><td>${u.employeeId||'—'}</td><td><button class="button small subtle" data-action="reset-password" data-id="${u.id}">Reset password</button></td></tr>`).join('')}</tbody></table></div></section>`;
}
function modalShell(title,content,submitText,formId){
  $('#modal-root').innerHTML=`<div class="modal-backdrop"><div class="modal" role="dialog" aria-modal="true"><div class="modal-heading"><h2>${escapeHtml(title)}</h2><button class="icon-button" data-action="close-modal" aria-label="Close">${icon('close')}</button></div><form id="${formId}"><div class="modal-body">${content}</div><div class="modal-footer"><button class="button subtle" type="button" data-action="close-modal">Cancel</button><button class="button primary" type="submit">${escapeHtml(submitText)}</button></div></form></div></div>`;
}
function openAccountModal(){
  if(!isAdmin())return;
  modalShell('Create user account',`<div class="field"><label>Display name</label><input name="name" maxlength="100" required placeholder="e.g. Priya Singh"></div>
    <div class="field"><label>Login email</label><input name="email" type="email" maxlength="120" required placeholder="priya@peopleflow.demo"></div>
    <div class="field"><label>Role</label><select name="role" id="new-role"><option value="HR">HR manager</option><option value="EMPLOYEE">Employee</option></select></div>
    <div class="field" id="linked-field" hidden><label>Linked employee</label><select name="employeeId"><option value="0">Select employee</option>${employeeOptions()}</select></div>
    <div class="field"><label>Temporary password (8+ characters)</label><input type="password" minlength="8" maxlength="128" name="password" required autocomplete="new-password"></div>`, 'Create account','account-form');
  $('#new-role').addEventListener('change',()=>$('#linked-field').hidden=$('#new-role').value!=='EMPLOYEE');
}
function openPasswordModal(){
  modalShell('Change your password',`<div class="field"><label>Current password</label><input type="password" name="oldPassword" required></div>
    <div class="field"><label>New password (8+ characters)</label><input type="password" name="newPassword" minlength="8" maxlength="128" required></div>`, 'Update password','password-form');
}
function openResetModal(id){
  if(!isAdmin())return;
  const user=state.users.find(u=>u.id===id);if(!user)return;
  modalShell('Reset password — '+user.name,`<input type="hidden" name="id" value="${user.id}"><div class="field"><label>New password (8+ characters)</label><input type="password" name="newPassword" minlength="8" maxlength="128" required></div>`, 'Reset password','reset-form');
}
function openEmployeeModal(employee=null){
  const edit=!!employee;
  $('#modal-root').innerHTML=`<div class="modal-backdrop"><div class="modal" role="dialog" aria-modal="true" aria-labelledby="modal-title"><div class="modal-heading"><h2 id="modal-title">${edit?'Edit employee':'Add new employee'}</h2><button class="icon-button" data-action="close-modal" aria-label="Close">${icon('close')}</button></div><form id="employee-form"><div class="modal-body"><div class="field"><label for="emp-name">Full name</label><input id="emp-name" name="name" required maxlength="100" placeholder="e.g. Rahul Sharma" value="${escapeHtml(employee?.name||'')}"></div><div class="field"><label for="emp-dep">Department</label><input id="emp-dep" name="department" required maxlength="80" placeholder="e.g. Engineering" value="${escapeHtml(employee?.department||'')}"></div><div class="field"><label for="emp-email">Work email</label><input id="emp-email" name="email" type="email" required maxlength="120" placeholder="e.g. rahul@company.com" value="${escapeHtml(employee?.email||'')}"></div><div class="field"><label for="emp-salary">Annual base salary (₹)</label><input id="emp-salary" name="salary" type="number" min="0" max="1000000000000" step="0.01" required placeholder="e.g. 600000" value="${employee?.salary??''}"></div></div><div class="modal-footer"><button type="button" class="button subtle" data-action="close-modal">Cancel</button><button type="submit" class="button primary">${edit?'Save changes':'Add employee'}</button></div></form></div></div>`;
  $('#employee-form').dataset.id=edit?employee.id:'';
  $('#emp-name').focus();
}
function closeModal(){$('#modal-root').innerHTML='';}
async function mutate(path,data,successMessage){
  await api(path,data);
  await loadAll();
  render();
  toast(successMessage);
}
$('#main-content').addEventListener('click',async event=>{
  const btn=event.target.closest('[data-action]');if(!btn)return;
  const act=btn.dataset.action;
  if(act==='add-employee'&&isManager())return openEmployeeModal();
  if(act==='add-account'&&isAdmin())return openAccountModal();
  if(act==='reset-password'&&isAdmin())return openResetModal(Number(btn.dataset.id));
  if(act==='go-employees')return navigate('employees');
  if(act==='go-leaves')return navigate('leaves');
  if(act==='go-attendance')return navigate('attendance');
  const id=Number(btn.dataset.id);
  const employee=state.employees.find(e=>e.id===id);
  if(act==='edit-employee'&&employee&&isManager())return openEmployeeModal(employee);
  if(act==='delete-employee'&&employee&&isManager()){
    if(!confirm(`Delete ${employee.name}? Their attendance and leave history will also be removed. This cannot be undone.`))return;
    try{await mutate('employees/delete',{id},'Employee deleted');}catch(e){toast(e.message,true);}
  }
  if(act==='review-leave'&&isManager()){
    const decision=btn.dataset.decision;
    try{await mutate('leaves/review',{id,action:decision},`Leave ${decision==='approve'?'approved':'rejected'}`);}catch(e){toast(e.message,true);}
  }
});
$('#main-content').addEventListener('input',event=>{
  if(event.target.id==='employee-search'){
    state.search=event.target.value;
    const list=state.employees.filter(e=>[e.name,e.department,e.email,String(e.id)].some(v=>v.toLowerCase().includes(state.search.toLowerCase())));
    $('#employee-rows').innerHTML=employeeRows(list);
  }
});
$('#main-content').addEventListener('submit',async event=>{
  if(!['attendance-form','leave-form'].includes(event.target.id))return;
  event.preventDefault();
  const isAttendance=event.target.id==='attendance-form';
  const data=Object.fromEntries(new FormData(event.target));
  const btn=$('button[type="submit"]',event.target);btn.disabled=true;
  try{await mutate(isAttendance?'attendance/add':'leaves/add',data,isAttendance?'Attendance saved':'Leave request submitted');}
  catch(e){toast(e.message,true);btn.disabled=false;}
});
$('#modal-root').addEventListener('click',event=>{
  if(event.target.classList.contains('modal-backdrop')||event.target.closest('[data-action="close-modal"]'))closeModal();
});
$('#modal-root').addEventListener('submit',async event=>{
  if(!['employee-form','account-form','password-form','reset-form'].includes(event.target.id))return;
  event.preventDefault();
  const formEl=event.target;
  const id=formEl.dataset.id;
  const data=Object.fromEntries(new FormData(formEl));
  if(id)data.id=id;
  if(formEl.id==='account-form'&&data.role==='EMPLOYEE'&&!(Number(data.employeeId)>0)){
    toast('Please link the employee account to an employee.',true);return;
  }
  const btn=$('button[type="submit"]',formEl);btn.disabled=true;
  try{
    if(formEl.id==='account-form')await mutate('users/add',data,'User account created');
    else if(formEl.id==='reset-form')await mutate('users/reset-password',data,'Password reset');
    else if(formEl.id==='password-form'){
      await api('users/change-password',data);
      closeModal();signOut(false);toast('Password changed. Sign in again.');return;
    }
    else await mutate(id?'employees/update':'employees/add',data,id?'Employee updated':'Employee added successfully');
    closeModal();
  }catch(e){toast(e.message,true);btn.disabled=false;}
});
document.addEventListener('keydown',event=>{if(event.key==='Escape')closeModal();});
