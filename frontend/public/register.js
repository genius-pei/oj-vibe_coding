import { apiPost } from './api.js';

const USERNAME_RE = /^[A-Za-z0-9_]{3,20}$/;

const form = document.getElementById('register-form');
const usernameInput = document.getElementById('reg-username');
const passwordInput = document.getElementById('reg-password');
const confirmInput = document.getElementById('reg-confirm');
const hintUsername = document.getElementById('hint-username');
const hintPassword = document.getElementById('hint-password');
const hintConfirm = document.getElementById('hint-confirm');
const submitBtn = document.getElementById('submit-btn');
const errorEl = document.getElementById('form-error');

function setHint(el, message, invalid) {
  el.textContent = message;
  el.classList.toggle('invalid', Boolean(invalid));
}

function setInvalid(input, invalid) {
  input.classList.toggle('invalid', Boolean(invalid));
}

function validateUsername() {
  const value = usernameInput.value.trim();
  if (value === '') {
    setHint(hintUsername, '3-20 位字母、数字或下划线', false);
    setInvalid(usernameInput, false);
    return false;
  }
  if (!USERNAME_RE.test(value)) {
    if (value.length < 3 || value.length > 20) {
      setHint(hintUsername, '用户名长度需在 3 到 20 个字符之间', true);
    } else {
      setHint(hintUsername, '仅允许字母、数字与下划线', true);
    }
    setInvalid(usernameInput, true);
    return false;
  }
  setHint(hintUsername, '✓ 用户名可用', false);
  setInvalid(usernameInput, false);
  return true;
}

function validatePassword() {
  const value = passwordInput.value;
  if (value === '') {
    setHint(hintPassword, '8-64 位，至少包含一个字母与一个数字', false);
    setInvalid(passwordInput, false);
    return false;
  }
  if (value.length < 8 || value.length > 64) {
    setHint(hintPassword, '密码长度需在 8 到 64 个字符之间', true);
    setInvalid(passwordInput, true);
    return false;
  }
  let hasLetter = false;
  let hasDigit = false;
  for (const ch of value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
      hasLetter = true;
    } else if (ch >= '0' && ch <= '9') {
      hasDigit = true;
    }
    if (hasLetter && hasDigit) break;
  }
  if (!hasLetter || !hasDigit) {
    setHint(hintPassword, '密码必须同时包含字母与数字', true);
    setInvalid(passwordInput, true);
    return false;
  }
  setHint(hintPassword, '✓ 密码强度符合要求', false);
  setInvalid(passwordInput, false);
  return true;
}

function validateConfirm() {
  const value = confirmInput.value;
  if (value === '') {
    setHint(hintConfirm, '再次输入以确认', false);
    setInvalid(confirmInput, false);
    return false;
  }
  if (value !== passwordInput.value) {
    setHint(hintConfirm, '两次密码不一致', true);
    setInvalid(confirmInput, true);
    return false;
  }
  setHint(hintConfirm, '✓ 密码一致', false);
  setInvalid(confirmInput, false);
  return true;
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
  submitBtn.disabled = true;
  submitBtn.textContent = '注册中…';

  try {
    await apiPost('/api/auth/register', {
      username: usernameInput.value.trim(),
      password: passwordInput.value
    });
    window.location.href = '/';
  } catch (error) {
    if (error && error.status === 409) {
      setHint(hintUsername, '该用户名已被占用', true);
      setInvalid(usernameInput, true);
      errorEl.textContent = '该用户名已被占用';
    } else if (error && error.status === 400) {
      errorEl.textContent = error.message || '注册信息不合法';
    } else {
      errorEl.textContent = `注册失败：${error && error.message ? error.message : '网络异常'}`;
    }
    submitBtn.disabled = false;
    submitBtn.textContent = '注 册';
    usernameInput.focus();
  }
});

refreshSubmitState();
