// register.js - Multi-step registration form
(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);

  const state = { inviteCode: "", email: "", code: "", username: "", password: "" };
  let currentStep = 1;
  let countdownTimer = null;

  // String constants (defined early to avoid TDZ)
  const C = {
    msg_no_code: "\u8BF7\u8F93\u5165\u9080\u8BF7\u7801",
    msg_invalid_email: "\u8BF7\u8F93\u5165\u6709\u6548\u7684\u90AE\u7BB1\u5730\u5740",
    text_verifying: "\u9A8C\u8BC1\u4E2D...",
    text_sending: "\u53D1\u9001\u4E2D...",
    text_registering: "\u6CE8\u518C\u4E2D...",
    text_send_failed: "\u53D1\u9001\u5931\u8D25",
    text_resend: "\u91CD\u53D1",
    network_error: "\u7F51\u7EDC\u9519\u8BEF"
  };

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
  const saved = localStorage.getItem("rain-theme") || "light";
  if (saved === "dark") document.documentElement.setAttribute("data-theme", "dark");
  tt.textContent = saved === "dark" ? "\u{1F319}" : "\u2600\uFE0F";
  tt.onclick = () => {
    const isDark = document.documentElement.getAttribute("data-theme") === "dark";
    document.documentElement.setAttribute("data-theme", isDark ? "" : "dark");
    localStorage.setItem("rain-theme", isDark ? "dark" : "light");
    tt.textContent = isDark ? "\u2600\uFE0F" : "\u{1F319}";
  };

  // Step 1: Verify invite code
  $("btnStep1").onclick = async () => {
    const code = $("inviteCode").value.trim();
    if (!code) { showToast(C.msg_no_code, true); return; }
    $("btnStep1").disabled = true;
    $("btnStep1").textContent = C.text_verifying;
    try {
      const r = await fetch("/api/invite/verify", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code })
      });
      const d = await r.json();
      if (d.valid) {
        state.inviteCode = code;
        showToast("\u9080\u8BF7\u7801\u6709\u6548");
        goToStep(2);
      } else {
        showToast(d.message || C.msg_no_code, true);
      }
    } catch (e) { showToast(C.network_error, true); }
    finally { $("btnStep1").disabled = false; $("btnStep1").textContent = "\u9A8C\u8BC1\u9080\u8BF7\u7801"; }
  };
  $("inviteCode").addEventListener("keydown", (e) => { if (e.key === "Enter") $("btnStep1").click(); });

  // Step 2: Send verification code
  $("btnSendCode").onclick = async () => {
    const email = $("email").value.trim();
    if (!email || !email.includes("@")) { showToast(C.msg_invalid_email, true); return; }
    $("btnSendCode").disabled = true;
    $("btnSendCode").textContent = C.text_sending;
    try {
      const r = await fetch("/api/verify/send", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email })
      });
      const d = await r.json();
      if (r.ok && d.success) {
        state.email = email;
        showToast("\u9A8C\u8BC1\u7801\u5DF2\u53D1\u9001\u5230\u90AE\u7BB1");
        goToStep(3);
        startCountdown();
      } else {
        const msg = d.error?.message || C.text_send_failed;
        showToast(msg, true);
      }
    } catch (e) { showToast(C.network_error, true); }
    finally { $("btnSendCode").disabled = false; $("btnSendCode").textContent = "\u53D1\u9001\u9A8C\u8BC1\u7801"; }
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
        btn.textContent = C.text_resend;
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
    if (!code || code.length !== 6) { showToast("\u8BF7\u8F93\u51656\u4F4D\u9A8C\u8BC1\u7801", true); return; }
    $("btnStep3").disabled = true;
    $("btnStep3").textContent = C.text_verifying;
    try {
      const r = await fetch("/api/verify/check", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email: state.email, code })
      });
      const d = await r.json();
      if (r.ok && d.data?.valid) {
        state.code = code;
        showToast("\u9A8C\u8BC1\u7801\u6709\u6548");
        goToStep(4);
      } else {
        showToast(d.data?.message || d.error?.message || "\u9A8C\u8BC1\u7801\u65E0\u6548", true);
      }
    } catch (e) { showToast(C.network_error, true); }
    finally { $("btnStep3").disabled = false; $("btnStep3").textContent = "\u6821\u9A8C\u9A8C\u8BC1\u7801"; }
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
    if (!username || username.length < 3) { showToast("\u7528\u6237\u540D\u81F3\u5C113\u4E2A\u5B57\u7B26", true); return; }
    if (password.length < 6) { showToast("\u5BC6\u7801\u81F3\u5C116\u4E2A\u5B57\u7B26", true); return; }
    if (password !== confirm) { showToast("\u4E24\u6B21\u5BC6\u7801\u4E0D\u4E00\u81F4", true); return; }
    $("btnStep4").disabled = true;
    $("btnStep4").textContent = C.text_registering;
    try {
      const r = await fetch("/register", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          inviteCode: state.inviteCode,
          email: state.email,
          code: state.code,
          username,
          password
        })
      });
      const d = await r.json();
      if (r.ok && d.success) {
        state.username = username;
        goToStep(5);
        setTimeout(() => { window.location.href = "/chat"; }, 2000);
      } else {
        showToast(d.error?.message || d.message || "\u6CE8\u518C\u5931\u8D25", true);
      }
    } catch (e) { showToast(C.network_error, true); }
    finally { $("btnStep4").disabled = false; $("btnStep4").textContent = "\u5B8C\u6210\u6CE8\u518C"; }
  };
  $("passwordConfirm").addEventListener("keydown", (e) => { if (e.key === "Enter") $("btnStep4").click(); });

  // Step 5: Finish
  $("btnFinish").onclick = () => { window.location.href = "/chat"; };

  console.log("register.js loaded");
})();
