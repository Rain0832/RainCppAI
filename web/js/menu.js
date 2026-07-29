// ==========================================================================
// menu.js — RainCppAI 功能菜单页
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

// Admin entry: show only for admin role (SP 5.7)
if (sessionStorage.getItem('role') === 'admin') {
    const btn = document.getElementById('admin-btn');
    if (btn) btn.style.display = '';
}

// Admin button handler
const adminBtn = document.getElementById('admin-btn');
if (adminBtn) {
    adminBtn.onclick = function(e) {
        e.stopPropagation();
        window.location.href = '/admin/dashboard';
    };
}

function showToast(msg) {
    const t = $('#toast');
    t.textContent = msg;
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 2000);
}

// Auth guard — 未登录跳转
if (!sessionStorage.getItem('userId')) window.location.href = '/entry';

// Logout
$('#logout').onclick = e => {
    e.stopPropagation();
    const userId = sessionStorage.getItem('userId');
    fetch('/user/logout', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ userId, type: 'manual', gameType: 0 })
    }).then(r => {
        sessionStorage.clear();
        window.location.href = '/entry';
    }).catch(() => showToast('退出失败'));
};

// Profile button — removed API Key settings (SP 5.8, server-managed)
$('#profile-btn').onclick = e => {
    e.stopPropagation();
    showToast('模型服务由系统统一管理');
};