import { apiGet, apiPost } from './api.js';

const AREA_ID = 'auth-area';
const ANONYMOUS_HTML =
  '<a href="/login.html">登录</a><a href="/register.html">注册</a>';

const area = () => document.getElementById(AREA_ID);

export async function fetchCurrentUser() {
  try {
    return await apiGet('/api/auth/me');
  } catch (error) {
    if (error && error.status === 401) {
      return null;
    }
    throw error;
  }
}

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function renderAnonymous(node) {
  if (!node) return;
  node.innerHTML = ANONYMOUS_HTML;
}

function renderUser(node, user) {
  if (!node) return;
  node.innerHTML = '';
  const chip = document.createElement('span');
  chip.className = 'user-chip';
  chip.innerHTML = `
    <span class="name">${escapeHtml(user.username)}</span>
    <button type="button" class="logout">退出</button>
  `;
  const logoutBtn = chip.querySelector('.logout');
  logoutBtn.addEventListener('click', async (event) => {
    event.preventDefault();
    logoutBtn.disabled = true;
    try {
      await apiPost('/api/auth/logout', {});
    } catch (_) {
      // 即使后端报错也允许前端清理 cookie 后跳走
    } finally {
      logoutBtn.disabled = false;
      window.location.href = '/';
    }
  });
  node.appendChild(chip);
}

export async function applyAuthHeader() {
  const node = area();
  if (!node) {
    return null;
  }
  const user = await fetchCurrentUser();
  if (user) {
    renderUser(node, user);
  } else {
    renderAnonymous(node);
  }
  return user;
}

document.addEventListener('DOMContentLoaded', () => {
  applyAuthHeader();
});
