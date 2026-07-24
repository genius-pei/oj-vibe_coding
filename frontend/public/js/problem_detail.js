import { apiGet } from './api.js';

const difficultyLabels = {
  easy: '简单',
  medium: '中等',
  hard: '困难'
};

const params = new URLSearchParams(window.location.search);
const problemId = params.get('id');

const titleEl = document.getElementById('problem-title');
const metaEl = document.getElementById('problem-meta');
const bodyEl = document.getElementById('problem-body');
const samplesEl = document.getElementById('problem-samples');
const resultEl = document.getElementById('result');

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function renderTags(tags) {
  if (!tags || tags.length === 0) {
    return '';
  }
  return tags.map((t) => `<span class="tag">${escapeHtml(t.name)}</span>`).join('');
}

function renderSamples(samples) {
  if (!samples || samples.length === 0) {
    return '';
  }
  return `
    <section class="samples">
      <h2>样例</h2>
      ${samples
        .map(
          (s, i) => `
        <article class="sample">
          <h3>样例 ${i + 1}</h3>
          <pre><strong>输入：</strong>\n${escapeHtml(s.input)}</pre>
          <pre><strong>输出：</strong>\n${escapeHtml(s.expected_output)}</pre>
        </article>`
        )
        .join('')}
    </section>
  `;
}

function setTitle(text) {
  document.title = `${text} — MiniOJ`;
}

async function load() {
  if (!problemId) {
    titleEl.textContent = '未指定题目';
    metaEl.textContent = '';
    bodyEl.textContent = '请通过题单进入题目详情页';
    return;
  }
  try {
    const problem = await apiGet(`/api/problems/${encodeURIComponent(problemId)}`);
    setTitle(problem.title);
    titleEl.textContent = problem.title;
    metaEl.innerHTML = `
      <span class="badge diff-${escapeHtml(problem.difficulty)}">${escapeHtml(difficultyLabels[problem.difficulty] || problem.difficulty)}</span>
      <span class="limit">${problem.time_limit_ms} ms</span>
      <span class="limit">${problem.memory_limit_mb} MB</span>
    `;
    if (problem.tags && problem.tags.length > 0) {
      metaEl.insertAdjacentHTML('beforeend', `<span class="tag-list">${renderTags(problem.tags)}</span>`);
    }
    bodyEl.innerHTML = window.marked.parse(problem.description_md || '');
    samplesEl.innerHTML = renderSamples(problem.sample_testcases);
  } catch (error) {
    titleEl.textContent = '加载失败';
    metaEl.textContent = '';
    bodyEl.textContent = error.message;
    if (resultEl) resultEl.textContent = '';
  }
}

load();