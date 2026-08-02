// ==========================================================================
// entry.js — RainCppAI 登录/注册页
// ==========================================================================

const $ = s => document.querySelector(s);
const $$ = s => document.querySelectorAll(s);

function initTheme() {
    const t = localStorage.getItem('rain-theme') || 'light';
    document.documentElement.setAttribute('data-theme', t);
    $('#themeToggle').textContent = t === 'dark' ? '\u{1F319}' : '\u2600\uFE0F';
}

$('#themeToggle').onclick = () => {
    const next = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', next);
    localStorage.setItem('rain-theme', next);
    $('#themeToggle').textContent = next === 'dark' ? '\u{1F319}' : '\u2600\uFE0F';
};

initTheme();

// Tab switching: login shows panel, register redirects to /register
$$('.tab').forEach(tab => tab.onclick = () => {
    if (tab.dataset.tab === 'register') {
        window.location.href = '/register';
        return;
    }
    $$('.tab').forEach(t => t.classList.remove('active'));
    $$('.form-panel').forEach(p => p.classList.remove('active'));
    tab.classList.add('active');
    $(`#panel-${tab.dataset.tab}`).classList.add('active');
});

function showToast(msg, dur = 2000) {
    const t = $('#toast');
    t.textContent = msg;
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), dur);
}

// If already logged in, redirect to chat
if (sessionStorage.getItem('userId')) window.location.href = '/chat';

$('#login-form').onsubmit = e => {
    e.preventDefault();
    fetch('/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username: $('#login-username').value, password: $('#login-password').value })
    }).then(r => {
        if (r.status === 403) { showToast('账号已在其他地方登录'); throw new Error('dup'); }
        if (r.status === 401) { showToast('用户名或密码错误'); throw new Error('auth'); }
        if (r.status === 200) return r.json();
        throw new Error('fail');
    }).then(d => {
        if (d?.userId) {
            sessionStorage.setItem('userId', d.userId);
            if (d.role) sessionStorage.setItem('role', d.role);
            if (d.username) sessionStorage.setItem('username', d.username);
            if (d.email) sessionStorage.setItem('email', d.email);
            showToast('登录成功');
            const dest = (d.role === 'admin') ? '/admin/dashboard' : '/chat';
            setTimeout(() => window.location.href = dest, 500);
        }
    }).catch(e => { if (!['dup', 'auth'].includes(e.message)) showToast('操作失败'); });
};
