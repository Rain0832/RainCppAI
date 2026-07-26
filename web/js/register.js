// register.js - Multi-step registration form
(function () {
  const $ = (id) => document.getElementById(id);

  const state = { inviteCode: "", email: "", code: "", username: "", password: "" };
  let currentStep = 1;
  let countdownTimer = null;

  function showToast(msg, isError) {
    const t = $("toast");
    t.textContent = msg;
    t.style.background = isError ? "#ef4444" : "";
    t.classList.add("show");
    setTimeout(() => t.classList.remove("show"), 3000);
    setTimeout(() => { t.style.background = ""; }, 3100);
  }

  function goToStep(n) {
    currentStep = n;
    document.querySelectorAll(".step-panel").forEach((el, i) => {
      el.classList.toggle("active", i + 1 === n);
    });
    document.querySelectorAll(".stepper .step").forEach((el) => {
      const s = parseInt(el.dataset.step);
      el.classList.remove("active", "done");
      if (s === n) el.classList.add("active");
      else if (s < n) el.classList.add("done");
    });
  }

  // Theme toggle
  const tt = $("themeToggle");
  const saved = localStorage.getItem("theme");
  if (saved === "dark") document.documentElement.setAttribute("data-theme", "dark");
  tt.textContent = saved === "dark" ? "\u{1F319}" : "\u2600\uFE0F";
  tt.onclick = () => {
    const isDark = document.documentElement.getAttribute("data-theme") === "dark";
    document.documentElement.setAttribute("data-theme", isDark ? "" : "dark");
    localStorage.setItem("theme", isDark ? "" : "dark");
    tt.textContent = isDark ? "\u2600\uFE0F" : "\u{1F319}";
  };

  // Step 1: Verify invite code
  $("btnStep1").onclick = async () => {
    const code = $("inviteCode").value.trim();
    if (!code) { showToast(msg_no_code, true); return; }
    $("btnStep1").disabled = true;
    $("btnStep1").textContent = text_verifying;
    try {
      const r = await fetch("/api/invite/verify", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code })
      });
      const d = await r.json();
      if (d.valid) {
        state.inviteCode = code;
        showToast("邀请码有效");
        goToStep(2);
      } else {
        showToast(d.message || "邀请码无效", true);
      }
    } catch (e) { showToast(network_error, true); }
    finally { $("btnStep1").disabled = false; $("btnStep1").textContent = "验证邀请码"; }
  };
  $("inviteCode").addEventListener("keydown", (e) => { if (e.key === "Enter") $("btnStep1").click(); });

  // Step 2: Send verification code
  $("btnSendCode").onclick = async () => {
    const email = $("email").value.trim();
    if (!email || !email.includes("@")) { showToast(msg_invalid_email, true); return; }
    $("btnSendCode").disabled = true;
    $("btnSendCode").textContent = text_sending;
    try {
      const r = await fetch("/api/verify/send", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email })
      });
      const d = await r.json();
      if (r.ok && d.success) {
        state.email = email;
        showToast("验证码已发送到邮箱");
        goToStep(3);
        startCountdown();
      } else {
        const msg = d.error?.message || text_send_failed;
        showToast(msg, true);
      }
    } catch (e) { showToast(network_error, true); }
    finally { $("btnSendCode").disabled = false; $("btnSendCode").textContent = "发送验证码"; }
  };
  $("email").addEventListener("keydown", (e) => { if (e.key === "Enter") $("btnSendCode").click(); });

  // Countdown for resend button
  function startCountdown() {
    let sec = 60;
    const btn = $("btnResend");
    btn.disabled = true;
    clearInterval(countdownTimer);
    countdownTimer = setInterval(() => {
      sec--;
      btn.textContent = sec + "s";
      if (sec <= 0) {
        clearInterval(countdownTimer);
        btn.textContent = text_resend;
        btn.disabled = false;
      }
    }, 1000);
  }
  $("btnResend").onclick = async () => {
    if (!$("btnResend").disabled) {
      $("btnSendCode").click();
      startCountdown();
    }
  };

  // Step 3: Verify code
  $("btnStep3").onclick = async () => {
    const code = $("verifyCode").value.trim();
    if (!code || code.length !== 6) { showToast("请输入6位验证码", true); return; }
    $("btnStep3").disabled = true;
    $("btnStep3").textContent = text_verifying;
    try {
      const r = await fetch("/api/verify/check", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email: state.email, code })
      });
      const d = await r.json();
      if (r.ok && d.data?.valid) {
        state.code = code;
        showToast("验证码有效");
        goToStep(4);
      } else {
        showToast(d.data?.message || d.error?.message || "验证码无效", true);
      }
    } catch (e) { showToast(network_error, true); }
    finally { $("btnStep3").disabled = false; $("btnStep3").textContent = "校验验证码"; }
  };
  // Auto-verify when user types 6 digits
  $("verifyCode").addEventListener("input", (e) => {
    if (e.target.value.length === 6) $("btnStep3").click();
  });

  // Step 4: Register
  $("btnStep4").onclick = async () => {
    const username = $("username").value.trim();
    const password = $("password").value;
    const confirm = $("passwordConfirm").value;
    if (!username || username.length < 3) { showToast("用户名至少3个字符", true); return; }
    if (password.length < 6) { showToast("密码至少6个字符", true); return; }
    if (password !== confirm) { showToast("两次密码不一致", true); return; }
    $("btnStep4").disabled = true;
    $("btnStep4").textContent = text_registering;
    try {
      const r = await fetch("/register", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ inviteCode: state.inviteCode, email: state.email, code: state.code, username, password })
      });
      const d = await r.json();
      if (r.ok && d.success) {
        state.username = username;
        state.password = password;
        goToStep(5);
        setTimeout(() => { window.location.href = "/chat"; }, 2000);
      } else {
        showToast(d.error?.message || d.message || "注册失败", true);
      }
    } catch (e) { showToast(network_error, true); }
    finally { $("btnStep4").disabled = false; $("btnStep4").textContent = "完成注册"; }
  };
  $("passwordConfirm").addEventListener("keydown", (e) => { if (e.key === "Enter") $("btnStep4").click(); });

  // Step 5: Finish
  $("btnFinish").onclick = () => { window.location.href = "/chat"; };

  // String constants (avoid inline unicode encoding issues)
  const msg_no_code = "请输入邀请码";
  const msg_invalid_email = "请输入有效的邮箱地址";
  const text_verifying = "验证中...";
  const text_sending = "发送中...";
  const text_registering = "注册中...";
  const text_send_failed = "发送失败";
  const text_resend = "重发";
  const network_error = "网络错误";
})();
