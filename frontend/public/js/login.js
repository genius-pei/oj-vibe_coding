import { apiPost } from './api.js';

const form = document.getElementById('login-form');
const usernameInput = document.getElementById('login-username');
const passwordInput = document.getElementById('login-password');
const submitBtn = document.getElementById('submit-btn');
const banner = document.getElementById('form-banner');
const usernameWrap = usernameInput.closest('.input-wrap');
const passwordWrap = passwordInput.closest('.input-wrap');

const BANNER_ICON =
  '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>';

function isWhitespaceOnly(value) {
  return value == null || String(value).trim() === '';
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

function setFieldState(wrap, state) {
  if (!wrap) return;
  wrap.dataset.state = state;
}

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function shake(wrap) {
  if (!wrap) return;
  wrap.classList.remove('shake');
  // 重置 animation 强制触发
  void wrap.offsetWidth;
  wrap.classList.add('shake');
}

function setLoading(loading) {
  submitBtn.disabled = loading;
  submitBtn.dataset.loading = loading ? 'true' : 'false';
}

function refreshSubmitState() {
  const u = usernameInput.value.trim();
  const p = passwordInput.value;
  submitBtn.disabled = isWhitespaceOnly(u) || isWhitespaceOnly(p);
  // 任何键入即清 banner
  if (banner && !banner.hidden) setBanner('');
  // idle state
  if (u && isWhitespaceOnly(p)) {
    setFieldState(passwordWrap, 'idle');
  }
}

usernameInput.addEventListener('input', () => {
  setFieldState(usernameWrap, 'idle');
  refreshSubmitState();
});
passwordInput.addEventListener('input', refreshSubmitState);

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (submitBtn.disabled) return;

  const username = usernameInput.value.trim();
  const password = passwordInput.value;

  if (isWhitespaceOnly(username) || isWhitespaceOnly(password)) {
    setBanner('请填写用户名与密码');
    if (isWhitespaceOnly(username)) {
      setFieldState(usernameWrap, 'err');
      shake(usernameWrap);
      usernameInput.focus();
    } else {
      setFieldState(passwordWrap, 'err');
      shake(passwordWrap);
      passwordInput.focus();
    }
    return;
  }

  setBanner('');
  setLoading(true);

  try {
    await apiPost('/api/auth/login', { username, password });
    const next = new URLSearchParams(window.location.search).get('next');
    window.location.href = next && next.startsWith('/') ? next : '/problems.html';
  } catch (error) {
    if (error && error.status === 401) {
      setBanner('用户名或密码错误');
      setFieldState(usernameWrap, 'err');
      setFieldState(passwordWrap, 'err');
      shake(usernameWrap);
      passwordInput.focus();
      passwordInput.select();
    } else if (error && error.status === 400) {
      setBanner(error.message || '登录信息不合法');
    } else {
      setBanner(`登录失败：${error && error.message ? error.message : '网络异常'}`);
    }
  } finally {
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

refreshSubmitState();