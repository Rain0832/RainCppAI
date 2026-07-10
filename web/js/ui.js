// ==========================================================================
// ui.js — RainCppAI 前端 UI 层
// 负责 DOM 渲染、事件绑定、主题切换、消息管理
// ==========================================================================

import {
    getModelId, getModelName, getApiKey, getRagId, getEndpointId,
    showToast, summarizeTitle, playTTS, logout, saveApiKey, updateKeyStatus,
    fetchHistory, fetchSessions, fetchModels, sendWithSSE, fetchApiKeysFromDb, deleteSession
} from './api.js';

// ---- 全局状态（模块作用域） ----

let currentSessionId = null;
let tempSession = false;
let lastUserQuestion = '';
const sessions = {};

// ---- 工具函数 ----

function $(s) { return document.querySelector(s); }

// ---- 暴露到 window 供 onclick 属性使用 ----

window.__playTTS = playTTS;
window.__deleteMsg = deleteMsg;
window.__regenerate = regenerate;
window.__saveKey = saveApiKey;

// ---- 主题 ----

function initTheme() {
    const t = localStorage.getItem('rain-theme') || 'light';
    document.documentElement.setAttribute('data-theme', t);
    $('#themeToggle').textContent = t === 'dark' ? '🌙' : '☀️';
}

// ---- 会话列表渲染 ----

function renderSessions() {
    const el = $('#sessionList');
    el.innerHTML = '';
    for (let id in sessions) {
        const d = document.createElement('div');
        d.className = 'session-item' + (id === currentSessionId ? ' active' : '');

        const nameSpan = document.createElement('span');
        nameSpan.className = 'session-name';
        nameSpan.textContent = sessions[id].name || `会话 ${id.slice(0, 8)}`;
        nameSpan.onclick = () => switchSession(id);
        d.appendChild(nameSpan);

        const delBtn = document.createElement('button');
        delBtn.className = 'session-del-btn';
        const icon = document.createElement('img');
        icon.src = '/assets/images/red_delete.svg';
        icon.alt = '';
        icon.width = 16;
        icon.height = 16;
        delBtn.appendChild(icon);
        delBtn.title = '删除会话';
        delBtn.onclick = (e) => {
            e.stopPropagation();
            handleDeleteSession(id);
        };
        d.appendChild(delBtn);

        el.appendChild(d);
    }
}

async function handleDeleteSession(id) {
    if (!confirm('确定要删除此会话吗？')) return;
    const ok = await deleteSession(id);
    if (ok) {
        delete sessions[id];
        if (id === currentSessionId) {
            currentSessionId = null;
            clearChat();
            $('#chatForm').style.display = 'none';
        }
        renderSessions();
        showToast('会话已删除');
    } else {
        showToast('删除失败');
    }
}

function hideWelcome() {
    const h = $('#welcomeHint');
    if (h) h.remove();
    document.querySelectorAll('.welcome-hint').forEach(e => e.remove());
}

// ---- 消息渲染 ----

function appendMsg(role, content, modelName, idx) {
    hideWelcome();
    const d = document.createElement('div');
    d.className = 'msg ' + role;
    if (typeof idx === 'number') d.dataset.idx = idx;
    const safe = DOMPurify.sanitize(marked.parse(content));
    let html = '';
    if (role === 'assistant' && modelName) html += `<div class="model-tag">${modelName}</div>`;
    html += `<div class="msg-content"><span>${safe}</span></div>`;
    let actions = '<div class="msg-actions">';
    if (role === 'assistant') {
        const esc = content.replace(/`/g, '\\`').replace(/\$/g, '\\$');
        actions += `<button class="action-btn" onclick="window.__playTTS(\`${esc}\`)">🔊 朗读</button>`;
    }
    actions += `<button class="del-btn" onclick="window.__deleteMsg(this)">✕ 删除</button>`;
    actions += '</div>';
    html += actions;
    d.innerHTML = html;
    $('#chatArea').appendChild(d);
    $('#chatArea').scrollTop = $('#chatArea').scrollHeight;
    return d;
}

function showThinking() {
    hideWelcome();
    const d = document.createElement('div');
    d.className = 'msg assistant';
    d.id = 'thinkingMsg';
    d.innerHTML = `<div class="model-tag">${getModelName()}</div><div class="thinking-dots"><span>·</span><span>·</span><span>·</span> 正在思考</div>`;
    $('#chatArea').appendChild(d);
    $('#chatArea').scrollTop = $('#chatArea').scrollHeight;
}

// ---- 删除消息 ----

function deleteMsg(btn) {
    const msgEl = btn.closest('.msg');
    if (!msgEl) return;
    const allMsgs = [...document.querySelectorAll('#chatArea .msg')];
    const domIdx = allMsgs.indexOf(msgEl);
    if (currentSessionId && sessions[currentSessionId]?.messages && domIdx >= 0 && domIdx < sessions[currentSessionId].messages.length) {
        sessions[currentSessionId].messages.splice(domIdx, 1);
    }
    msgEl.remove();
    showToast('已删除');
}

// ---- 重新生成（SSE 版本） ----

async function regenerate() {
    if (!currentSessionId || tempSession || !lastUserQuestion) {
        showToast('无法重新生成');
        return;
    }
    const msgs = sessions[currentSessionId]?.messages;
    if (!msgs || msgs.length < 2) return;

    const allMsgs = [...document.querySelectorAll('#chatArea .msg')];
    const lastAiDom = allMsgs[allMsgs.length - 1];
    if (lastAiDom) lastAiDom.remove();
    msgs.pop();

    const mt = $('#modelType').value;
    const mn = getModelName();
    const pv = getSelectedProvider();
    const ragId = getRagId();
    const endpointId = getEndpointId();

    $('#sendBtn').disabled = true;
    try {
        await sendWithSSE(lastUserQuestion, mt, pv, mn, currentSessionId, ragId, endpointId, sessions, appendMsg);
    } catch (e) {
        const tk = $('#thinkingMsg');
        if (tk) tk.remove();
        appendMsg('assistant', '无法连接到服务器', mn);
    }
    $('#sendBtn').disabled = false;
}

// ---- 会话切换 ----

function clearChat() { $('#chatArea').innerHTML = ''; }

async function switchSession(id) {
    currentSessionId = String(id);
    tempSession = false;
    clearChat();
    $('#chatForm').style.display = 'flex';
    if (!sessions[id].messages?.length) {
        const history = await fetchHistory(currentSessionId);
        if (history) sessions[id].messages = history;
    }
    if (sessions[id].messages?.length) {
        sessions[id].messages.forEach((m, i) => appendMsg(m.role, m.content, m.model, i));
    } else {
        const hint = document.createElement('div');
        hint.className = 'welcome-hint';
        hint.innerHTML = '<span class="emoji">✨</span>开始新对话吧';
        $('#chatArea').appendChild(hint);
    }
    const userMsgs = (sessions[id].messages || []).filter(m => m.role === 'user');
    if (userMsgs.length) lastUserQuestion = userMsgs[userMsgs.length - 1].content;
    renderSessions();
}

// ---- 事件绑定 ----

function bindEvents() {
    // 主题
    $('#themeToggle').onclick = () => {
        const next = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
        document.documentElement.setAttribute('data-theme', next);
        localStorage.setItem('rain-theme', next);
        $('#themeToggle').textContent = next === 'dark' ? '🌙' : '☀️';
    };

    // Avatar 下拉菜单
    $('#avatarBtn').onclick = e => {
        e.stopPropagation();
        $('#avatarMenu').classList.toggle('show');
    };
    document.addEventListener('click', () => $('#avatarMenu').classList.remove('show'));

    // 退出登录
    $('#ddLogout').onclick = () => logout();

    // API Key 弹窗
    $('#ddApiKey').onclick = () => {
        $('#avatarMenu').classList.remove('show');
        $('#apiKeyModal').classList.add('show');
        updateKeyStatus();
    };
    $('#modalClose').onclick = () => $('#apiKeyModal').classList.remove('show');
    $('#apiKeyModal').onclick = e => {
        if (e.target === $('#apiKeyModal')) $('#apiKeyModal').classList.remove('show');
    };

    // 新建对话
    $('#newChatBtn').onclick = () => {
        currentSessionId = 'temp';
        tempSession = true;
        clearChat();
        $('#chatForm').style.display = 'flex';
        const hint = document.createElement('div');
        hint.className = 'welcome-hint';
        hint.innerHTML = '<span class="emoji">✨</span>输入你的第一条消息';
        $('#chatArea').appendChild(hint);
        renderSessions();
        $('#question').focus();
    };

    // Textarea 自适应高度 + Enter 发送
    const ta = $('#question');
    ta.oninput = () => {
        ta.style.height = 'auto';
        ta.style.height = Math.min(ta.scrollHeight, 120) + 'px';
    };
    ta.onkeydown = e => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            $('#chatForm').dispatchEvent(new Event('submit'));
        }
    };

    // 发送消息
    $('#chatForm').onsubmit = async e => {
        e.preventDefault();
        const q = ta.value.trim();
        if (!q) return;
        if (!currentSessionId && !tempSession) {
            showToast('请先新建对话或选择会话');
            return;
        }
        const mt = $('#modelType').value;
        const mn = getModelName();
        const pv = getSelectedProvider();
        const ragId = getRagId();
        const endpointId = getEndpointId();
        lastUserQuestion = q;

        appendMsg('user', q);
        ta.value = '';
        ta.style.height = 'auto';
        $('#sendBtn').disabled = true;
        showThinking();

        try {
            if (tempSession) {
                const result = await sendWithSSE(q, mt, pv, mn, '', ragId, endpointId, sessions, appendMsg);
                if (result.sessionId) {
                    currentSessionId = result.sessionId;
                    tempSession = false;
                    // 确保新会话已入列（sendWithSSE 已创建 sessions[id]，此处兜底）
                    if (!sessions[currentSessionId]) {
                        sessions[currentSessionId] = { name: '新会话', messages: [] };
                    }
                    renderSessions();
                    summarizeTitle(currentSessionId, q, sessions, renderSessions);

                    // 延迟 1.5s 等待后端 LLM 异步标题落库后刷新侧边栏
                    setTimeout(() => fetchSessions(sessions, renderSessions), 1500);
                }
            } else {
                if (!sessions[currentSessionId]) {
                    const tk = $('#thinkingMsg');
                    if (tk) tk.remove();
                    showToast('请先选择或创建会话');
                    $('#sendBtn').disabled = false;
                    return;
                }
                sessions[currentSessionId].messages.push({ role: 'user', content: q });
                await sendWithSSE(q, mt, pv, mn, currentSessionId, ragId, endpointId, sessions, appendMsg);
            }
        } catch (err) {
            const tk = $('#thinkingMsg');
            if (tk) tk.remove();
            appendMsg('assistant', '无法连接到服务器', mn);
        }
        $('#sendBtn').disabled = false;
    };
}

// ---- 模型下拉框 ----

let _modelRegistry = [];

async function renderModelDropdown() {
    const sel = $('#modelType');
    _modelRegistry = await fetchModels();
    sel.innerHTML = '';
    for (const provider of _modelRegistry) {
        const grp = document.createElement('optgroup');
        grp.label = provider.provider_name;
        for (const model of provider.models) {
            const opt = document.createElement('option');
            opt.value = model.id;
            opt.textContent = model.name;
            opt.dataset.provider = provider.provider;
            grp.appendChild(opt);
        }
        sel.appendChild(grp);
    }
    if (sel.options.length > 0) sel.options[0].selected = true;
}

function getSelectedProvider() {
    const sel = $('#modelType');
    return sel.options[sel.selectedIndex]?.dataset?.provider || 'aliyun';
}

// ---- 初始化 ----

(async () => {
    initTheme();
    bindEvents();

    // 从后端加载模型列表，动态渲染下拉框
    await renderModelDropdown();

    // 从后端 DB 加载 API Key 状态
    await fetchApiKeysFromDb();

    // 自动加载会话列表
    const result = await fetchSessions(sessions, renderSessions);
    if (result && result.length > 0) {
        // 自动激活第一个会话（最近使用的）
        const firstSid = String(result[0].sessionId);
        await switchSession(firstSid);
    } else {
        // 无会话：自动触发新建
        currentSessionId = 'temp';
        tempSession = true;
        $('#chatForm').style.display = 'flex';
        const hint = document.createElement('div');
        hint.className = 'welcome-hint';
        hint.innerHTML = '<span class="emoji">✨</span>输入你的第一条消息';
        $('#chatArea').appendChild(hint);
        $('#question').focus();
    }
})();