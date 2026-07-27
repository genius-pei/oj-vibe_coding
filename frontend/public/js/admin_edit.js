import { apiGet, apiPost, apiPut } from './api.js';
import { fetchCurrentUser } from './auth.js';

const params = new URLSearchParams(window.location.search);
const problemId = params.get('id');
const isEdit = !!problemId;

const titleInput = document.getElementById('f-title');
const difficultySelect = document.getElementById('f-difficulty');
const timeInput = document.getElementById('f-time');
const memInput = document.getElementById('f-mem');
const descInput = document.getElementById('f-desc');
const tagsInput = document.getElementById('f-tags');
const tagsPreview = document.getElementById('tags-preview');
const casesContainer = document.getElementById('testcases');
const caseTpl = document.getElementById('tpl-case');
const addCaseBtn = document.getElementById('btn-add-case');
const form = document.getElementById('problem-form');
const errorEl = document.getElementById('form-error');
const saveBtn = document.getElementById('btn-save');
const formTitle = document.getElementById('form-title');

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function ensureAdmin() {
  return fetchCurrentUser().then((user) => {
    if (!user || user.role !== 'admin') {
      window.location.replace('/login.html?next=' + encodeURIComponent(window.location.pathname + window.location.search));
      return null;
    }
    return user;
  });
}

function showError(message) {
  errorEl.textContent = message || '';
}

function parseTags(raw) {
  return String(raw || '')
    .split(/[,，]/)
    .map((s) => s.trim())
    .filter((s) => s.length > 0);
}

function renderTagsPreview() {
  const list = parseTags(tagsInput.value);
  if (list.length === 0) {
    tagsPreview.innerHTML = '<span class="muted">无标签</span>';
    return;
  }
  tagsPreview.innerHTML = list
    .map((t) => `<span class="tag">${escapeHtml(t)}</span>`)
    .join('');
}

function reindexCases() {
  const rows = casesContainer.querySelectorAll('.case-row');
  rows.forEach((row, idx) => {
    const head = row.querySelector('.case-index');
    if (head) head.textContent = `#${idx + 1}`;
  });
}

function appendCase(initial) {
  const node = caseTpl.content.firstElementChild.cloneNode(true);
  const removeBtn = node.querySelector('.case-remove');
  const inputArea = node.querySelector('.case-input');
  const outputArea = node.querySelector('.case-output');
  const isSample = node.querySelector('.case-is-sample');
  const scoreInput = node.querySelector('.case-score-input');

  if (initial) {
    if (typeof initial.input === 'string') inputArea.value = initial.input;
    if (typeof initial.expected_output === 'string') outputArea.value = initial.expected_output;
    if (typeof initial.is_sample === 'boolean') isSample.checked = initial.is_sample;
    if (initial.score != null && initial.score !== '') {
      scoreInput.value = String(initial.score);
    }
  }

  removeBtn.addEventListener('click', () => {
    node.remove();
    reindexCases();
    refreshSubmitState();
  });

  [inputArea, outputArea, scoreInput].forEach((el) => {
    el.addEventListener('input', refreshSubmitState);
  });
  isSample.addEventListener('change', refreshSubmitState);

  casesContainer.appendChild(node);
  reindexCases();
}

function collectCases() {
  const rows = casesContainer.querySelectorAll('.case-row');
  const out = [];
  rows.forEach((row) => {
    const inputArea = row.querySelector('.case-input');
    const outputArea = row.querySelector('.case-output');
    const isSample = row.querySelector('.case-is-sample');
    const scoreInput = row.querySelector('.case-score-input');
    const input = inputArea.value;
    const expected = outputArea.value;
    const scoreVal = parseInt(scoreInput.value, 10);
    out.push({
      input,
      expected_output: expected,
      is_sample: isSample.checked,
      score: Number.isFinite(scoreVal) && scoreVal >= 0 ? scoreVal : 0
    });
  });
  return out;
}

function validate() {
  const title = titleInput.value.trim();
  if (!title) return '请填写标题';
  if (title.length > 255) return '标题不能超过 255 个字符';
  const diff = difficultySelect.value;
  if (!['easy', 'medium', 'hard'].includes(diff)) return '难度取值不合法';
  const time = parseInt(timeInput.value, 10);
  if (!Number.isFinite(time) || time <= 0) return '时间限制必须大于 0';
  const mem = parseInt(memInput.value, 10);
  if (!Number.isFinite(mem) || mem <= 0) return '内存限制必须大于 0';
  if (!descInput.value.trim()) return '请填写题目描述';
  const cases = collectCases();
  if (cases.length === 0) return '请至少添加 1 条测试用例';
  if (cases.length > 1000) return '测试用例不能超过 1000 条';
  for (let i = 0; i < cases.length; i += 1) {
    const c = cases[i];
    if (c.input == null || c.expected_output == null) return `第 ${i + 1} 条用例的输入或期望输出为空`;
  }
  return '';
}

function refreshSubmitState() {
  saveBtn.disabled = !!validate();
}

function buildPayload() {
  return {
    title: titleInput.value.trim(),
    description_md: descInput.value,
    difficulty: difficultySelect.value,
    time_limit_ms: parseInt(timeInput.value, 10),
    memory_limit_mb: parseInt(memInput.value, 10),
    tags: parseTags(tagsInput.value),
    testcases: collectCases()
  };
}

async function loadForEdit() {
  saveBtn.disabled = true;
  try {
    const detail = await apiGet(`/api/admin/problems/${encodeURIComponent(problemId)}`);
    titleInput.value = detail.title || '';
    difficultySelect.value = detail.difficulty || 'medium';
    timeInput.value = String(detail.time_limit_ms ?? 1000);
    memInput.value = String(detail.memory_limit_mb ?? 256);
    descInput.value = detail.description_md || '';
    const tagNames = (detail.tags || []).map((t) => (t && t.name) || t);
    tagsInput.value = tagNames.join(', ');
    renderTagsPreview();
    casesContainer.innerHTML = '';
    (detail.testcases || []).forEach((tc) => appendCase(tc));
    if ((detail.testcases || []).length === 0) {
      appendCase();
    }
    formTitle.textContent = `编辑题目 #${detail.id}`;
    document.title = `编辑 #${detail.id} — MiniOJ 后台`;
    refreshSubmitState();
  } catch (error) {
    if (error && (error.status === 401 || error.status === 403)) {
      window.location.replace('/login.html?next=' + encodeURIComponent(window.location.pathname + window.location.search));
      return;
    }
    showError(`加载失败：${error.message || '网络异常'}`);
  }
}

async function submit(event) {
  event.preventDefault();
  const err = validate();
  if (err) {
    showError(err);
    return;
  }
  showError('');
  saveBtn.disabled = true;
  const originalHtml = saveBtn.innerHTML;
  saveBtn.innerHTML = '<span class="btn-spinner" aria-hidden="true"></span>保存中…';

  const payload = buildPayload();
  try {
    if (isEdit) {
      await apiPut(`/api/admin/problems/${encodeURIComponent(problemId)}`, payload);
      showError('');
      window.location.href = '/admin/index.html?saved=' + encodeURIComponent(problemId);
    } else {
      const response = await apiPost('/api/admin/problems', payload);
      const newId = response && response.id;
      window.location.href = '/admin/index.html?created=' + encodeURIComponent(String(newId));
    }
  } catch (error) {
    if (error && error.status === 400) {
      showError(error.message || '参数不合法');
    } else if (error && error.status === 404) {
      showError('题目不存在');
    } else if (error && (error.status === 401 || error.status === 403)) {
      window.location.replace('/login.html?next=' + encodeURIComponent(window.location.pathname + window.location.search));
      return;
    } else {
      showError(`保存失败：${error.message || '网络异常'}`);
    }
    saveBtn.disabled = false;
    saveBtn.innerHTML = originalHtml;
  }
}

addCaseBtn.addEventListener('click', () => {
  appendCase();
  refreshSubmitState();
});

tagsInput.addEventListener('input', () => {
  renderTagsPreview();
  refreshSubmitState();
});

[titleInput, difficultySelect, timeInput, memInput, descInput].forEach((el) => {
  el.addEventListener('input', refreshSubmitState);
  el.addEventListener('change', refreshSubmitState);
});

form.addEventListener('submit', submit);

ensureAdmin().then((user) => {
  if (!user) return;
  if (isEdit) {
    loadForEdit();
  } else {
    formTitle.textContent = '新建题目';
    document.title = '新建题目 — MiniOJ 后台';
    appendCase({ is_sample: true, score: 100 });
    refreshSubmitState();
  }
});