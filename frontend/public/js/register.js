import { apiPost } from './api.js';
import {
  validateUsernameInput,
  validatePasswordInput,
  validateConfirmInput
} from './validation.js';

const form = document.getElementById('register-form');
const usernameInput = document.getElementById('reg-username');
const passwordInput = document.getElementById('reg-password');
const confirmInput = document.getElementById('reg-confirm');
const hintUsername = document.getElementById('hint-username');
const hintPassword = document.getElementById('hint-password');
const hintConfirm = document.getElementById('hint-confirm');
const submitBtn = document.getElementById('submit-btn');
const errorEl = document.getElementById('form-error');

const DEFAULT_BTN_HTML = '注 册';

function setHint(el, message, state) {
  // state: undefined | 'ok' | 'invalid'
  el.textContent = message;
  el.classList.remove('ok', 'invalid');
  if (state === 'ok' || state === 'invalid') {
    el.classList.add(state);
  }
}

function setInvalid(input, invalid) {
  input.classList.toggle('invalid', Boolean(invalid));
}

function applyHint(el, result, input) {
  setHint(el, result.message, result.ok ? 'ok' : 'invalid');
  setInvalid(input, !result.ok);
  return result.ok;
}

function validateUsername() {
  return applyHint(hintUsername, validateUsernameInput(usernameInput.value), usernameInput);
}

function validatePassword() {
  return applyHint(hintPassword, validatePasswordInput(passwordInput.value), passwordInput);
}

function validateConfirm() {
  return applyHint(
    hintConfirm,
    validateConfirmInput(confirmInput.value, passwordInput.value),
    confirmInput
  );
}

function setLoading(loading) {
  submitBtn.disabled = loading;
  submitBtn.innerHTML = loading
    ? '<span class="btn-spinner" aria-hidden="true"></span>注册中…'
    : DEFAULT_BTN_HTML;
}

function refreshSubmitState() {
  const ok = validateUsername() && validatePassword() && validateConfirm();
  submitBtn.disabled = !ok;
  errorEl.textContent = '';
}

usernameInput.addEventListener('input', () => {
  validateUsername();
  validateConfirm();
  refreshSubmitState();
});

passwordInput.addEventListener('input', () => {
  validatePassword();
  validateConfirm();
  refreshSubmitState();
});

confirmInput.addEventListener('input', () => {
  validateConfirm();
  refreshSubmitState();
});

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (submitBtn.disabled) {
    return;
  }
  errorEl.textContent = '';
  setLoading(true);

  try {
    await apiPost('/api/auth/register', {
      username: usernameInput.value.trim(),
      password: passwordInput.value
    });
    window.location.href = '/';
  } catch (error) {
    if (error && error.status === 409) {
      setHint(hintUsername, '该用户名已被占用', 'invalid');
      setInvalid(usernameInput, true);
      errorEl.textContent = '该用户名已被占用';
    } else if (error && error.status === 400) {
      errorEl.textContent = error.message || '注册信息不合法';
    } else {
      errorEl.textContent = `注册失败：${error && error.message ? error.message : '网络异常'}`;
    }
    setLoading(false);
    usernameInput.focus();
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
  });
});

refreshSubmitState();