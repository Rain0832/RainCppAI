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

// Profile modal
$('#profile-btn').onclick = e => {
    e.stopPropagation();
    $('#profileModal').classList.add('show');
    updateKeyStatus();
};

$('#modalClose').onclick = () => $('#profileModal').classList.remove('show');

$('#profileModal').onclick = e => {
    if (e.target === $('#profileModal')) $('#profileModal').classList.remove('show');
};

// Modal tabs
$$('.modal-tab').forEach(tab => tab.onclick = () => {
    $$('.modal-tab').forEach(t => t.classList.remove('active'));
    $$('.modal-panel').forEach(p => p.classList.remove('active'));
    tab.classList.add('active');
    $(`#panel-${tab.dataset.panel}`).classList.add('active');
});

// API Key management
const KM = {
    dashscope: 'rain-key-dashscope',
    'rag-id': 'rain-key-rag-id',
    doubao: 'rain-key-doubao',
    'doubao-ep': 'rain-key-doubao-ep',
    'baidu-id': 'rain-key-baidu-id',
    'baidu-secret': 'rain-key-baidu-secret'
};

function saveKey(n) {
    const v = $(`#key-${n}`).value.trim();
    if (!v) { showToast('请输入有效的 Key'); return; }
    localStorage.setItem(KM[n], v);
    $(`#key-${n}`).value = '';
    showToast('已保存');
    updateKeyStatus();
}

function updateKeyStatus() {
    Object.keys(KM).forEach(n => {
        const el = $(`#st-${n}`),
            set = !!localStorage.getItem(KM[n]);
        el.textContent = set ? '已配置' : '未配置';
        el.className = 'key-status ' + (set ? 'set' : 'unset');
    });
}

// Expose saveKey to onclick
window.saveKey = saveKey;