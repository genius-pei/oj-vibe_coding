import { apiGet, apiPost } from './api.js';

const difficultyLabels = {
  easy: '简单',
  medium: '中等',
  hard: '困难'
};

const TEMPLATES = {
  cpp: `#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // TODO: 在此读取输入并输出答案

    return 0;
}
`,
  c: `#include <stdio.h>

int main(void) {
    // TODO: 在此读取输入并输出答案
    return 0;
}
`
};

const params = new URLSearchParams(window.location.search);
const problemId = params.get('id');

const titleEl = document.getElementById('problem-title');
const idEl = document.getElementById('problem-id');
const metaEl = document.getElementById('problem-meta');
const bodyEl = document.getElementById('problem-body');
const samplesEl = document.getElementById('problem-samples');
const resultEl = document.getElementById('result');

const langEl = document.getElementById('lang');
const templateBtn = document.getElementById('btn-template');
const resetBtn = document.getElementById('btn-reset');
const submitBtn = document.getElementById('btn-submit');

let editor = null;
let currentProblem = null;
let lastTemplateLang = null;

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = value == null ? '' : String(value);
  return div.innerHTML;
}

function initEditor() {
  if (typeof window.ace === 'undefined') {
    bodyEl.textContent = '代码编辑器加载失败';
    return null;
  }
  const ed = window.ace.edit('editor', {
    mode: 'ace/mode/c_cpp',
    theme: 'ace/theme/one_dark',
    fontSize: 13,
    fontFamily: "'JetBrains Mono', 'SF Mono', 'Cascadia Code', Menlo, Consolas, monospace",
    tabSize: 4,
    useSoftTabs: true,
    showPrintMargin: false,
    highlightActiveLine: true,
    wrap: false,
    showLineNumbers: true,
    showGutter: true,
    animatedScroll: true,
  });
  ed.session.setUseWrapMode(false);
  ed.renderer.setScrollMargin(8, 8);
  return ed;
}

function renderTags(tags) {
  if (!tags || tags.length === 0) return '';
  return tags
    .map((t) => `<span class="tag">${escapeHtml(t.name || t)}</span>`)
    .join('');
}

function renderSamples(samples) {
  if (!samples || samples.length === 0) return '';
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

function renderMeta(problem) {
  metaEl.innerHTML = `
    <span class="badge diff-${escapeHtml(problem.difficulty)}">${escapeHtml(difficultyLabels[problem.difficulty] || problem.difficulty)}</span>
    <span class="limit">${problem.time_limit_ms} ms</span>
    <span class="limit">${problem.memory_limit_mb} MB</span>
    ${problem.tags && problem.tags.length ? `<span class="tag-list">${renderTags(problem.tags)}</span>` : ''}
  `;
}

function applyTemplate() {
  if (!editor) return;
  const lang = langEl.value;
  editor.setValue(TEMPLATES[lang] || '', -1);
  lastTemplateLang = lang;
}

function setSubmitting(submitting) {
  submitBtn.disabled = submitting;
  submitBtn.innerHTML = submitting
    ? '<span class="btn-spinner" aria-hidden="true"></span>判题中…'
    : '提 交';
}

function renderVerdictBadge(verdict) {
  return `<span class="verdict-badge ${escapeHtml(verdict)}">${escapeHtml(verdict)}</span>`;
}

function renderResult(response) {
  if (!response) {
    resultEl.hidden = true;
    resultEl.innerHTML = '';
    return;
  }
  resultEl.hidden = false;

  const verdict = response.verdict || '?';
  const maxTime = response.time_ms != null ? `${response.time_ms} ms` : '—';
  const memory = response.memory_mb != null ? `${response.memory_mb} MB` : '—';

  const compileBlock = response.compile_output
    ? `<pre class="compile-output">${escapeHtml(response.compile_output)}</pre>`
    : '';

  const cases = Array.isArray(response.per_case) ? response.per_case : [];
  const casesHtml = cases.length
    ? `
      <div class="case-list">
        ${cases
          .map((c) => {
            const failed = c.verdict !== 'AC';
            const detail =
              c.verdict === 'WA'
                ? `<div class="case-detail">
                     <div class="detail-row"><span class="detail-label">expected:</span><pre>${escapeHtml(c.expected || '')}</pre></div>
                     <div class="detail-row"><span class="detail-label">actual:</span><pre>${escapeHtml(c.actual || '')}</pre></div>
                   </div>`
                : '';
            return `
              <div class="case-row ${failed ? 'failed' : 'passed'}">
                <span class="case-index">#${c.index}</span>
                <span class="case-verdict ${escapeHtml(c.verdict)}">${escapeHtml(c.verdict)}</span>
                <span class="case-meta">${c.time_ms != null ? c.time_ms + ' ms' : ''}${c.memory_mb != null ? ' · ' + c.memory_mb + ' MB' : ''}</span>
              </div>
              ${detail}
            `;
          })
          .join('')}
      </div>
    `
    : '';

  resultEl.innerHTML = `
    <div class="result-summary">
      ${renderVerdictBadge(verdict)}
      <span class="result-time">${maxTime} · ${memory}</span>
    </div>
    ${compileBlock}
    ${casesHtml}
  `;
  resultEl.scrollTop = 0;
}

function renderErrorResult(message) {
  resultEl.hidden = false;
  resultEl.innerHTML = `
    <div class="result-summary">
      <span class="verdict-badge CE">ERR</span>
      <span class="result-time">${escapeHtml(message || '未知错误')}</span>
    </div>
  `;
}

async function submit() {
  if (!editor || submitBtn.disabled || !currentProblem) return;
  resultEl.hidden = false;
  resultEl.classList.add('is-judging');
  resultEl.innerHTML = `
    <div class="result-summary">
      <span class="verdict-badge CE"><span class="btn-spinner" aria-hidden="true"></span></span>
      <span>判题中…</span>
      <span class="result-time">编译 → 运行 → 比对</span>
    </div>
  `;
  setSubmitting(true);

  const code = editor.getValue();
  const lang = langEl.value;

  try {
    const response = await apiPost('/api/submissions', {
      problem_id: currentProblem.id,
      lang,
      code
    });
    renderResult(response);
  } catch (error) {
    renderErrorResult(error && error.message ? error.message : '网络异常');
  } finally {
    resultEl.classList.remove('is-judging');
    setSubmitting(false);
  }
}

async function load() {
  if (!problemId) {
    titleEl.textContent = '未指定题目';
    idEl.textContent = '#—';
    bodyEl.textContent = '请通过题单进入题目详情页';
    submitBtn.disabled = true;
    return;
  }
  try {
    const problem = await apiGet(`/api/problems/${encodeURIComponent(problemId)}`);
    currentProblem = problem;
    setTitle(problem.title);
    idEl.textContent = `#${problem.id}`;
    titleEl.textContent = problem.title;
    renderMeta(problem);
    bodyEl.innerHTML = window.marked.parse(problem.description_md || '');
    samplesEl.innerHTML = renderSamples(problem.sample_testcases);
    submitBtn.disabled = false;

    editor = initEditor();
    if (editor) {
      applyTemplate();
    } else {
      submitBtn.disabled = true;
    }
  } catch (error) {
    titleEl.textContent = '加载失败';
    idEl.textContent = '#—';
    metaEl.textContent = '';
    bodyEl.textContent = error.message || '网络异常';
    submitBtn.disabled = true;
  }
}

templateBtn.addEventListener('click', applyTemplate);

resetBtn.addEventListener('click', () => {
  if (!editor) return;
  if (window.confirm('清空当前代码？此操作不可撤销。')) {
    editor.setValue('', -1);
    editor.focus();
  }
});

langEl.addEventListener('change', () => {
  if (langEl.value !== lastTemplateLang) {
    applyTemplate();
  }
});

submitBtn.addEventListener('click', submit);

load();