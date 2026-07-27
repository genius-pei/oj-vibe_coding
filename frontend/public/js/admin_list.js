import { apiGet, apiPost, apiDelete } from './api.js';
import { fetchCurrentUser } from './auth.js';

const DIFFICULTY_LABELS = {
  easy: '简单',
  medium: '中等',
  hard: '困难'
};

const tbody = document.getElementById('problem-tbody');
const hintEl = document.getElementById('hint');
const resetBtn = document.getElementById('btn-reset');

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function escapeAttr(value) {
  return escapeHtml(value).replace(/"/g, '&quot;');
}

function ensureAdmin() {
  return fetchCurrentUser().then((user) => {
    if (!user) {
      window.location.replace('/login.html?next=' + encodeURIComponent('/admin/index.html'));
      return null;
    }
    if (user.role !== 'admin') {
      window.location.replace('/login.html?next=' + encodeURIComponent('/admin/index.html'));
      return null;
    }
    return user;
  });
}

function showHint(message, level) {
  if (!message) {
    hintEl.textContent = '';
    hintEl.className = 'hint';
    return;
  }
  hintEl.textContent = message;
  hintEl.className = 'hint ' + (level || '');
}

function renderTags(tags) {
  if (!tags || tags.length === 0) {
    return '<span class="muted">—</span>';
  }
  return tags
    .map((t) => `<span class="tag">${escapeHtml(t.name || t)}</span>`)
    .join('');
}

function renderRow(problem) {
  const diffLabel = DIFFICULTY_LABELS[problem.difficulty] || problem.difficulty;
  const sampleCount = (problem.testcases || []).filter((tc) => tc.is_sample).length;
  const totalCount = (problem.testcases || []).length;
  return `
    <tr data-id="${problem.id}">
      <td class="col-id"><span class="id">#${problem.id}</span></td>
      <td class="col-title">
        <a class="title-link" href="/problem.html?id=${problem.id}">${escapeHtml(problem.title)}</a>
      </td>
      <td class="col-diff">
        <span class="badge diff-${escapeAttr(problem.difficulty)}">${escapeHtml(diffLabel)}</span>
      </td>
      <td class="col-limits">
        <span class="limit">${problem.time_limit_ms} ms</span>
        <span class="limit">${problem.memory_limit_mb} MB</span>
      </td>
      <td class="col-tags"><div class="tags">${renderTags(problem.tags)}</div></td>
      <td class="col-cases">
        <span class="count">${totalCount}</span>
        <span class="muted">（样例 ${sampleCount}）</span>
      </td>
      <td class="col-actions">
        <a class="row-btn edit" href="/admin/edit.html?id=${problem.id}">编辑</a>
        <button type="button" class="row-btn danger" data-action="delete" data-id="${problem.id}" data-title="${escapeAttr(problem.title)}">删除</button>
      </td>
    </tr>
  `;
}

function renderEmpty(message) {
  tbody.innerHTML = `<tr class="placeholder"><td colspan="7">${escapeHtml(message)}</td></tr>`;
}

function renderList(items) {
  if (!items || items.length === 0) {
    renderEmpty('题库为空，点击右上角「+ 新建题目」或「一键重置题库」载入 seed 数据');
    return;
  }
  tbody.innerHTML = items.map(renderRow).join('');
}

async function loadProblems() {
  renderEmpty('题库加载中…');
  try {
    const items = await apiGet('/api/admin/problems');
    renderList(items);
  } catch (error) {
    if (error && (error.status === 401 || error.status === 403)) {
      window.location.replace('/login.html?next=' + encodeURIComponent('/admin/index.html'));
      return;
    }
    renderEmpty(`加载失败：${escapeHtml(error.message || '网络异常')}`);
  }
}

async function handleReset() {
  const ok = window.confirm(
    '确定要重置题库吗？\n\n' +
      '此操作将删除当前所有题目与用例，并恢复为 seed/problems.json 中的初始数据。\n' +
      '普通用户账号不受影响。'
  );
  if (!ok) return;
  resetBtn.disabled = true;
  showHint('正在重置题库…');
  try {
    const response = await apiPost('/api/admin/reset', {});
    showHint(`已重置：${response.message || 'OK'}`, 'ok');
    await loadProblems();
  } catch (error) {
    showHint(`重置失败：${escapeHtml(error.message || '网络异常')}`, 'err');
  } finally {
    resetBtn.disabled = false;
  }
}

async function handleDelete(button) {
  const id = button.getAttribute('data-id');
  const title = button.getAttribute('data-title') || '';
  const ok = window.confirm(
    `确定删除题目「${title}」(#${id}) 吗？\n\n` +
      '此操作会一并删除该题的所有测试用例，不可撤销。'
  );
  if (!ok) return;
  button.disabled = true;
  showHint(`正在删除 #${id}…`);
  try {
    await apiDelete(`/api/admin/problems/${encodeURIComponent(id)}`);
    showHint(`已删除 #${id}`, 'ok');
    await loadProblems();
  } catch (error) {
    showHint(`删除失败：${escapeHtml(error.message || '网络异常')}`, 'err');
    button.disabled = false;
  }
}

tbody.addEventListener('click', (event) => {
  const btn = event.target.closest('button[data-action="delete"]');
  if (!btn) return;
  handleDelete(btn);
});

resetBtn.addEventListener('click', handleReset);

ensureAdmin().then((user) => {
  if (!user) return;
  if (user.role === 'admin') {
    showHint(`当前以管理员 ${user.username} 登录`);
  }
  loadProblems();
});