// ==========================================================================
// entry.js — RainCppAI 登录/注册页
// ==========================================================================

const $ = s => document.querySelector(s);
const $$ = s => document.querySelectorAll(s);

function initTheme() {
    const t = localStorage.getItem('rain-theme') || 'light';
    document.documentElement.setAttribute('data-theme', t);
    $('#themeToggle').textContent = t === 'dark' ? '🌙' : '☀️';
}

$('#themeToggle').onclick = () => {
    const next = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', next);
    localStorage.setItem('rain-theme', next);
    $('#themeToggle').textContent = next === 'dark' ? '🌙' : '☀️';
};

initTheme();

$$('.tab').forEach(tab => tab.onclick = () => {
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

// 如果已登录，直接跳转到聊天页
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
            showToast('登录成功');
            setTimeout(() => window.location.href = '/chat', 500);
        }
    }).catch(e => { if (!['dup', 'auth'].includes(e.message)) showToast('操作失败'); });
};

$('#register-form').onsubmit = e => {
    e.preventDefault();
    fetch('/register', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            username: $('#register-username').value,
            password: $('#register-password').value
        })
    }).then(r => {
        if (r.status === 200) {
            showToast('注册成功，请登录');
            $('#login-username').value = $('#register-username').value;
            $('#login-password').value = $('#register-password').value;
            setTimeout(() => $$('.tab')[0].click(), 800);
        } else if (r.status === 409) {
            showToast('用户名已存在');
        }
    }).catch(() => showToast('注册失败'));
};