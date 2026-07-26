import { apiGet } from './api.js';

const difficultyLabels = {
  easy: '简单',
  medium: '中等',
  hard: '困难'
};

const listEl = document.getElementById('problem-list');
const difficultyEl = document.getElementById('filter-difficulty');
const tagEl = document.getElementById('filter-tag');
const countEl = document.getElementById('problem-count');

let allTags = [];

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

function mergeTags(tagsFromServer) {
  const set = new Set(allTags);
  tagsFromServer.forEach((t) => set.add(t));
  allTags = Array.from(set);
}

function renderTagOptions() {
  const currentValue = tagEl.value;
  const opts = ['<option value="">全部</option>']
    .concat(allTags.map((t) => `<option value="${escapeAttr(t)}">${escapeHtml(t)}</option>`));
  tagEl.innerHTML = opts.join('');
  if (allTags.includes(currentValue)) {
    tagEl.value = currentValue;
  }
}

function renderTags(tags) {
  if (!tags || tags.length === 0) {
    return '<span class="tags muted">无标签</span>';
  }
  return `<div class="tags">${tags.map((t) => `<span class="tag">${escapeHtml(t)}</span>`).join('')}</div>`;
}

function renderCard(problem) {
  return `
    <a class="card diff-${escapeAttr(problem.difficulty)}" href="/problem.html?id=${problem.id}">
      <div class="card-top">
        <span class="badge">${escapeHtml(difficultyLabels[problem.difficulty] || problem.difficulty)}</span>
        <span class="id">#${problem.id}</span>
      </div>
      <h3 class="title">${escapeHtml(problem.title)}</h3>
      <div class="limits">
        <span class="limit-value">${problem.time_limit_ms} ms</span>
        <span class="sep">·</span>
        <span class="limit-value">${problem.memory_limit_mb} MB</span>
      </div>
      ${renderTags(problem.tags)}
    </a>
  `;
}

function renderEmpty(message, isError) {
  listEl.innerHTML = `<p class="empty${isError ? ' is-error' : ''}">${escapeHtml(message)}</p>`;
  countEl.textContent = '';
}

function renderList(items) {
  if (!Array.isArray(items) || items.length === 0) {
    renderEmpty('暂无符合条件的题目');
    return;
  }
  countEl.textContent = `共 ${items.length} 题`;
  listEl.innerHTML = items.map(renderCard).join('');
}

async function load() {
  renderEmpty('题单加载中…');
  try {
    const items = await apiGet(`/api/problems${buildQuery()}`);
    if (Array.isArray(items)) {
      mergeTags(items.flatMap((p) => p.tags || []));
      renderTagOptions();
    }
    renderList(items);
  } catch (error) {
    renderEmpty(`加载失败：${error.message || '网络异常'}`, true);
  }
}

difficultyEl.addEventListener('change', load);
tagEl.addEventListener('change', load);

load();