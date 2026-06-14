// ==========================================================================
// api.js — RainCppAI 前端网络层
// 负责所有 HTTP 请求：SSE 流式对话、会话管理、TTS、历史记录、登出、API Key
// ==========================================================================

// ---- helpers ----

export function getModelName() {
    const s = document.querySelector('#modelType');
    return s.options[s.selectedIndex].text;
}

export function getApiKey(mt) {
    const m = { '1': 'rain-key-dashscope', '2': 'rain-key-doubao', '3': 'rain-key-dashscope' };
    return localStorage.getItem(m[mt] || '') || '';
}

export function getRagId() {
    return localStorage.getItem('rain-key-rag-id') || '';
}

export function getEndpointId() {
    return localStorage.getItem('rain-key-doubao-ep') || '';
}

// ---- toast ----

export function showToast(msg) {
    const t = document.querySelector('#toast');
    t.textContent = msg;
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 2000);
}

// ---- 登出 ----

export async function logout() {
    const userId = sessionStorage.getItem('userId');
    try {
        await fetch('/user/logout', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ userId, type: 'manual', gameType: 0 })
        });
    } catch (_) { }
    sessionStorage.clear();
    window.location.href = '/entry';
}

// ---- API Key 管理 ----

const KM = {
    dashscope: 'rain-key-dashscope',
    'rag-id': 'rain-key-rag-id',
    doubao: 'rain-key-doubao',
    'doubao-ep': 'rain-key-doubao-ep',
    'baidu-id': 'rain-key-baidu-id',
    'baidu-secret': 'rain-key-baidu-secret'
};

export function saveApiKey(n) {
    const v = document.querySelector(`#key-${n}`).value.trim();
    if (!v) { showToast('请输入有效的 Key'); return; }
    localStorage.setItem(KM[n], v);
    document.querySelector(`#key-${n}`).value = '';
    showToast('已保存');
    updateKeyStatus();
}

export function updateKeyStatus() {
    Object.keys(KM).forEach(n => {
        const el = document.querySelector(`#st-${n}`);
        if (!el) return;
        const set = !!localStorage.getItem(KM[n]);
        el.textContent = set ? '已配置' : '未配置';
        el.className = 'key-status ' + (set ? 'set' : 'unset');
    });
}

// ---- 会话标题更新 ----

export async function summarizeTitle(sid, question, sessions, renderSessions) {
    const firstSentence = question.split(/[。？！?!.\n]/)[0].trim();
    const title = firstSentence.slice(0, 18) + (firstSentence.length > 18 ? '...' : '');
    sessions[sid].name = title || `会话 ${sid.slice(0, 8)}`;
    renderSessions();
    const ak = getApiKey(document.querySelector('#modelType').value);
    if (ak) {
        fetch('/chat/update-title', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ sessionId: sid, title: sessions[sid].name })
        }).catch(() => { });
    }
}

// ---- TTS ----

export async function playTTS(text) {
    try {
        const r = await fetch('/chat/tts', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ text })
        });
        const d = await r.json();
        if (d.success && d.url) {
            new Audio(d.url).play();
        } else {
            showToast('语音合成失败');
        }
    } catch (e) {
        showToast('语音请求失败');
    }
}

// ---- 会话历史 ----

export async function fetchHistory(sessionId) {
    try {
        const r = await fetch('/chat/history', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ sessionId })
        });
        const d = await r.json();
        if (d.success && Array.isArray(d.history)) {
            return d.history.map(m => ({ role: m.is_user ? 'user' : 'assistant', content: m.content }));
        }
    } catch (e) { console.error(e); }
    return null;
}

export async function fetchSessions(sessions, renderSessions) {
    try {
        const r = await fetch('/chat/sessions');
        const d = await r.json();
        if (d.success && Array.isArray(d.sessions)) {
            d.sessions.forEach(s => {
                const sid = String(s.sessionId);
                sessions[sid] = { name: s.name || `会话 ${sid.slice(0, 8)}`, messages: [] };
            });
            renderSessions();
            return d.sessions;
        }
    } catch (e) { console.error(e); }
    return null;
}

// ---- SSE 流式发送 ----

export async function sendWithSSE(question, modelType, apiKey, modelName, sessionId, ragId, endpointId, sessions, appendMsg) {
    const tk = document.querySelector('#thinkingMsg');
    if (tk) tk.remove();

    const chatArea = document.querySelector('#chatArea');
    const d = document.createElement('div');
    d.className = 'msg assistant';
    if (modelName) d.innerHTML = `<div class="model-tag">${modelName}</div>`;
    d.innerHTML += '<div class="msg-content"><span class="sseContent"></span></div>';
    chatArea.appendChild(d);
    const span = d.querySelector('.sseContent');

    let fullContent = '';
    let resolvedSid = sessionId;

    try {
        const response = await fetch('/chat/send-stream', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ question, modelType, sessionId, apiKey, ragId, endpointId })
        });

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buf = '';

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            buf += decoder.decode(value, { stream: true });

            const lines = buf.split('\n');
            buf = lines.pop();

            for (const line of lines) {
                const trimmed = line.trim();
                if (!trimmed || trimmed === 'data: [DONE]') continue;
                if (trimmed.startsWith('data: ')) {
                    try {
                        const payload = JSON.parse(trimmed.slice(6));
                        if (payload.sessionId && !resolvedSid) {
                            resolvedSid = String(payload.sessionId);
                            sessions[resolvedSid] = {
                                name: question.slice(0, 18) + '...',
                                messages: [{ role: 'user', content: question }]
                            };
                            continue;
                        }
                        if (payload.token) {
                            fullContent += payload.token;
                            span.innerHTML = DOMPurify.sanitize(marked.parse(fullContent));
                            chatArea.scrollTop = chatArea.scrollHeight;
                        }
                        if (payload.error) {
                            span.textContent = '错误: ' + payload.error;
                        }
                    } catch (_) { }
                }
            }
        }
    } catch (err) {
        span.textContent = '无法连接到服务器';
    }

    const esc = fullContent.replace(/`/g, '\\`').replace(/\$/g, '\\$');
    const acts = document.createElement('div');
    acts.className = 'msg-actions';
    acts.innerHTML = `<button class="action-btn" onclick="window.__playTTS(\`${esc}\`)">🔊 朗读</button>`
        + `<button class="action-btn" onclick="window.__regenerate()">🔄 重新生成</button>`
        + `<button class="del-btn" onclick="window.__deleteMsg(this)">✕ 删除</button>`;
    d.appendChild(acts);

    if (sessions[resolvedSid]) {
        sessions[resolvedSid].messages.push({ role: 'assistant', content: fullContent });
    }

    return { content: fullContent, sessionId: resolvedSid };
}