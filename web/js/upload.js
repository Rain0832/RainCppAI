// ==========================================================================
// upload.js — RainCppAI 图像识别页
// ==========================================================================

const $ = s => document.querySelector(s);

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

$('#imageInput').onchange = () => {
    const f = $('#imageInput').files[0];
    if (f) {
        $('#fileLabel').textContent = f.name;
        $('#fileLabel').classList.add('has-file');
    }
};

function appendMsg(role, content, imgUrl) {
    const hint = document.querySelector('.welcome-hint');
    if (hint) hint.remove();
    const d = document.createElement('div');
    d.className = 'msg ' + role;
    d.innerHTML = content;
    if (imgUrl) {
        const img = document.createElement('img');
        img.src = imgUrl;
        d.appendChild(img);
    }
    $('#chatArea').appendChild(d);
    $('#chatArea').scrollTop = $('#chatArea').scrollHeight;
}

$('#uploadForm').onsubmit = async e => {
    e.preventDefault();
    if (!$('#imageInput').files.length) return;
    const file = $('#imageInput').files[0];
    const reader = new FileReader();
    reader.onload = async ev => {
        const b64Url = ev.target.result,
            b64Data = b64Url.split(',')[1];
        appendMsg('user', `上传图片: ${file.name}`, b64Url);
        $('#sendBtn').disabled = true;
        try {
            const r = await fetch('/upload/send', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ filename: file.name, image: b64Data })
            });
            if (!r.ok) throw new Error('上传失败');
            const d = await r.json();
            appendMsg('assistant',
                `识别结果: <strong>${d.class_name}</strong><br>置信度: ${Math.round(d.confidence * 100)}%`);
        } catch (err) {
            appendMsg('assistant', `错误: ${err.message}`);
        }
        $('#sendBtn').disabled = false;
        $('#imageInput').value = '';
        $('#fileLabel').textContent = '点击选择图片';
        $('#fileLabel').classList.remove('has-file');
    };
    reader.readAsDataURL(file);
};