import { apiGet } from './api.js';

const difficultyLabels = {
  easy: '简单',
  medium: '中等',
  hard: '困难'
};

const listEl = document.getElementById('problem-list');
const difficultyEl = document.getElementById('filter-difficulty');
const tagEl = document.getElementById('filter-tag');
const messageEl = document.createElement('p');
messageEl.className = 'empty';
listEl.parentNode.insertBefore(messageEl, listEl);

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function escapeAttr(value) {
  return escapeHtml(value).replace(/"/g, '&quot;');
}

function buildQuery() {
  const params = new URLSearchParams();
  if (difficultyEl.value) {
    params.set('difficulty', difficultyEl.value);
  }
  if (tagEl.value) {
    params.set('tag', tagEl.value);
  }
  const query = params.toString();
  return query ? `?${query}` : '';
}

function renderTags(tags) {
  if (!tags || tags.length === 0) {
    return '<span class="tags muted">无标签</span>';
  }
  return `<span class="tags">${tags.map((t) => `<span class="tag">${escapeHtml(t)}</span>`).join('')}</span>`;
}

function renderCard(problem) {
  return `
    <a class="card diff-${escapeAttr(problem.difficulty)}" href="/problem.html?id=${problem.id}">
      <div class="card-header">
        <span class="badge">${escapeHtml(difficultyLabels[problem.difficulty] || problem.difficulty)}</span>
        <h3 class="title">${escapeHtml(problem.title)}</h3>
      </div>
      <div class="limits">${problem.time_limit_ms} ms · ${problem.memory_limit_mb} MB</div>
      ${renderTags(problem.tags)}
    </a>
  `;
}

function renderEmpty(message) {
  listEl.innerHTML = '';
  messageEl.textContent = message;
}

function renderError(error) {
  listEl.innerHTML = '';
  messageEl.textContent = `加载失败：${error.message}`;
}

async function load() {
  try {
    const items = await apiGet(`/api/problems${buildQuery()}`);
    if (!Array.isArray(items) || items.length === 0) {
      renderEmpty('暂无符合条件的题目');
      return;
    }
    messageEl.textContent = '';
    listEl.innerHTML = items.map(renderCard).join('');
  } catch (error) {
    renderError(error);
  }
}

difficultyEl.addEventListener('change', load);
tagEl.addEventListener('change', load);

load();