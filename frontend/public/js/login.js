import { apiPost } from './api.js';

const form = document.getElementById('login-form');
const usernameInput = document.getElementById('login-username');
const passwordInput = document.getElementById('login-password');
const submitBtn = document.getElementById('submit-btn');
const errorEl = document.getElementById('form-error');

function isWhitespaceOnly(value) {
  return value == null || String(value).trim() === '';
}

function refreshSubmitState() {
  const u = usernameInput.value.trim();
  const p = passwordInput.value;
  submitBtn.disabled = isWhitespaceOnly(u) || isWhitespaceOnly(p);
  errorEl.textContent = '';
}

usernameInput.addEventListener('input', refreshSubmitState);
passwordInput.addEventListener('input', refreshSubmitState);

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (submitBtn.disabled) {
    return;
  }
  errorEl.textContent = '';
  submitBtn.disabled = true;
  submitBtn.textContent = '登录中…';

  try {
    await apiPost('/api/auth/login', {
      username: usernameInput.value.trim(),
      password: passwordInput.value
    });
    window.location.href = '/';
  } catch (error) {
    if (error && error.status === 401) {
      errorEl.textContent = error.message || '用户名或密码错误';
    } else if (error && error.status === 400) {
      errorEl.textContent = error.message || '登录信息不合法';
    } else {
      errorEl.textContent = `登录失败：${error && error.message ? error.message : '网络异常'}`;
    }
    submitBtn.disabled = false;
    submitBtn.textContent = '登 录';
    passwordInput.focus();
    passwordInput.select();
  }
});

refreshSubmitState();
