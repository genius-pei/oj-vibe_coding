import { apiPost } from './api.js';
import {
  validateUsernameInput,
  validatePasswordInput,
  validateConfirmInput,
  evaluatePasswordStrength
} from './validation.js';

const form = document.getElementById('register-form');
const usernameInput = document.getElementById('reg-username');
const passwordInput = document.getElementById('reg-password');
const confirmInput = document.getElementById('reg-confirm');

const hintUsername = document.getElementById('hint-username');
const hintPassword = document.getElementById('hint-password');
const hintConfirm = document.getElementById('hint-confirm');

const strengthBar = document.getElementById('strength-bar');
const submitBtn = document.getElementById('submit-btn');
const banner = document.getElementById('form-banner');

const usernameWrap = usernameInput.closest('.input-wrap');
const passwordWrap = passwordInput.closest('.input-wrap');
const confirmWrap = confirmInput.closest('.input-wrap');

const BANNER_ICON =
  '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>';

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function setHint(el, message, state) {
  el.textContent = message;
  el.classList.remove('ok', 'invalid');
  if (state === 'ok' || state === 'invalid') el.classList.add(state);
}

function setFieldState(wrap, state) {
  if (!wrap) return;
  wrap.dataset.state = state;
}

function setBanner(message) {
  if (!message) {
    banner.hidden = true;
    banner.innerHTML = '';
    return;
  }
  banner.hidden = false;
  banner.innerHTML = `${BANNER_ICON}<span>${escapeHtml(message)}</span>`;
}

function shake(wrap) {
  if (!wrap) return;
  wrap.classList.remove('shake');
  void wrap.offsetWidth;
  wrap.classList.add('shake');
}

function setLoading(loading) {
  submitBtn.disabled = loading;
  submitBtn.dataset.loading = loading ? 'true' : 'false';
}

function applyUsername(result) {
  setHint(hintUsername, result.message, result.ok ? 'ok' : 'invalid');
  // 用户名格式 OK 不代表已注册，所以未填或错误时显示 err；填了且格式通过显示 ok
  if (usernameInput.value.trim() === '') {
    setFieldState(usernameWrap, 'idle');
  } else {
    setFieldState(usernameWrap, result.ok ? 'ok' : 'err');
  }
  return result.ok;
}

function applyPassword(result) {
  setHint(hintPassword, result.message, result.ok ? 'ok' : 'invalid');
  if (passwordInput.value === '') {
    setFieldState(passwordWrap, 'idle');
  } else {
    setFieldState(passwordWrap, result.ok ? 'ok' : 'err');
  }
  return result.ok;
}

function applyConfirm(result) {
  setHint(hintConfirm, result.message, result.ok ? 'ok' : 'invalid');
  if (confirmInput.value === '') {
    setFieldState(confirmWrap, 'idle');
  } else {
    setFieldState(confirmWrap, result.ok ? 'ok' : 'err');
  }
  return result.ok;
}

function updateStrengthBar() {
  const s = evaluatePasswordStrength(passwordInput.value);
  if (s.color === 'empty') {
    strengthBar.removeAttribute('data-strength');
  } else {
    strengthBar.setAttribute('data-strength', s.color);
    // 用 hint 文本附带强度
    const base = validatePasswordInput(passwordInput.value).message;
    if (passwordInput.value) {
      setHint(hintPassword, `${base} · 强度：${s.label}`, passwordInput.value.length >= 8 && /\d/.test(passwordInput.value) && /[A-Za-z]/.test(passwordInput.value) ? 'ok' : 'invalid');
    }
  }
}

function validateAll() {
  const u = applyUsername(validateUsernameInput(usernameInput.value));
  const p = applyPassword(validatePasswordInput(passwordInput.value));
  const c = applyConfirm(validateConfirmInput(confirmInput.value, passwordInput.value));
  updateStrengthBar();
  submitBtn.disabled = !(u && p && c);
  if (banner && !banner.hidden) setBanner('');
}

usernameInput.addEventListener('input', validateAll);
passwordInput.addEventListener('input', validateAll);
confirmInput.addEventListener('input', validateAll);

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (submitBtn.disabled) return;

  const uResult = validateUsernameInput(usernameInput.value);
  const pResult = validatePasswordInput(passwordInput.value);
  const cResult = validateConfirmInput(confirmInput.value, passwordInput.value);
  const ok = uResult.ok && pResult.ok && cResult.ok;
  if (!ok) {
    applyUsername(uResult);
    applyPassword(pResult);
    applyConfirm(cResult);
    // 聚焦首个无效字段
    let first = null;
    if (!uResult.ok) first = usernameInput;
    else if (!pResult.ok) first = passwordInput;
    else if (!cResult.ok) first = confirmInput;
    if (first) {
      const wrap = first.closest('.input-wrap');
      shake(wrap);
      first.focus();
    }
    setBanner('请修正表单中标红的位置后重试');
    return;
  }

  setBanner('');
  setLoading(true);

  try {
    await apiPost('/api/auth/register', {
      username: usernameInput.value.trim(),
      password: passwordInput.value
    });
    const next = new URLSearchParams(window.location.search).get('next');
    window.location.href = next && next.startsWith('/') ? next : '/problems.html';
  } catch (error) {
    if (error && error.status === 409) {
      setBanner('该用户名已被占用');
      setHint(hintUsername, '该用户名已被占用', 'invalid');
      setFieldState(usernameWrap, 'err');
      shake(usernameWrap);
      usernameInput.focus();
    } else if (error && error.status === 400) {
      setBanner(error.message || '注册信息不合法');
    } else {
      setBanner(`注册失败：${error && error.message ? error.message : '网络异常'}`);
    }
    setLoading(false);
  }
});

// 密码显隐切换
document.querySelectorAll('.toggle-pw').forEach((btn) => {
  btn.addEventListener('click', () => {
    const targetId = btn.getAttribute('data-target');
    const input = targetId ? document.getElementById(targetId) : null;
    if (!input) return;
    const showing = input.type === 'text';
    input.type = showing ? 'password' : 'text';
    btn.setAttribute('aria-label', showing ? '显示密码' : '隐藏密码');
    btn.setAttribute('title', showing ? '显示密码' : '隐藏密码');
    btn.innerHTML = showing
      ? '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>'
      : '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"/><line x1="1" y1="1" x2="23" y2="23"/></svg>';
    input.focus();
  });
});

validateAll();