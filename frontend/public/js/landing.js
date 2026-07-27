// landing.js — 落地页装饰：打字机 + 实时判题状态轮播
// 尊重 prefers-reduced-motion：直接展示完整状态，跳过动画

const $typewriter = document.getElementById('typewriter');
const $verdict = document.getElementById('verdict');
const $meta = document.getElementById('verdict-meta');

// 4 段演示场景
const DEMOS = [
  {
    code:
`#include <iostream>
using namespace std;

int main() {
    long long a, b;
    cin >> a >> b;
    cout << a + b << endl;
    return 0;
}`,
    verdict: 'AC',
    meta: '5/5 用例 · 12 ms · 3 MB'
  },
  {
    code:
`#include <iostream>
using namespace std;

int main() {
    long long a, b;
    cin >> a >> b;
    cout << a - b << endl;  // 误写减法
    return 0;
}`,
    verdict: 'WA',
    meta: 'Case 2 失败 · Expected "3" Got "1"'
  },
  {
    code:
`#include <iostream>
using namespace std;

int main() {
    long long a, b;
    cin >> a >> b;
    while (true) {}  // 死循环
    return 0;
}`,
    verdict: 'TLE',
    meta: '500 ms 超时强杀 · 0/5 通过'
  },
  {
    code:
`#include <iostream>
int main() {
    cout << "hello"  // 漏分号
}`,
    verdict: 'CE',
    meta: 'g++: expected ";" before "}"'
  }
];

const KEYWORDS = new Set([
  'int', 'long', 'void', 'return', 'if', 'else', 'for', 'while',
  'using', 'namespace', 'include', 'true', 'false', 'nullptr',
  'endl', 'cout', 'cin', 'std', 'main', 'char', 'double', 'float',
  'const', 'static', 'auto'
]);

function escapeHtml(s) {
  return s.replace(/[&<>"']/g, (c) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  }[c]));
}

// 哨兵字符：高亮阶段用 \x00X\x00 / \x00/Y\x00 占位，避免多轮 replace 互相破坏
function highlight(code) {
  const SENT = '\x00';
  let s = escapeHtml(code);
  s = s.replace(/(\/\/[^\n]*)/g, `${SENT}C${SENT}$1${SENT}/${SENT}C${SENT}`);
  s = s.replace(/(&quot;[^&\n]*?&quot;)/g, `${SENT}S${SENT}$1${SENT}/${SENT}S${SENT}`);
  s = s.replace(/\b(\d+)\b/g, `${SENT}N${SENT}$1${SENT}/${SENT}N${SENT}`);
  const kwRegex = new RegExp(`\\b(${[...KEYWORDS].join('|')})\\b`, 'g');
  s = s.replace(kwRegex, `${SENT}K${SENT}$1${SENT}/${SENT}K${SENT}`);
  s = s
    .replace(new RegExp(`${SENT}C${SENT}`, 'g'), '<span class="tok-com">')
    .replace(new RegExp(`${SENT}S${SENT}`, 'g'), '<span class="tok-str">')
    .replace(new RegExp(`${SENT}N${SENT}`, 'g'), '<span class="tok-num">')
    .replace(new RegExp(`${SENT}K${SENT}`, 'g'), '<span class="tok-kw">')
    .replace(new RegExp(`${SENT}/${SENT}`, 'g'), '</span>');
  return s;
}

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function prefersReducedMotion() {
  return window.matchMedia('(prefers-reduced-motion: reduce)').matches;
}

async function playOne(demo, reduced) {
  // 重置状态条
  $verdict.textContent = '—';
  $verdict.className = 'status-value';
  $meta.textContent = '编译中…';
  $typewriter.textContent = '';

  if (reduced) {
    $typewriter.innerHTML = highlight(demo.code);
    await sleep(900);
  } else {
    // 打字机：逐字符输出纯文本
    const text = demo.code;
    const STEP_MS = 18;
    for (let i = 0; i <= text.length; i += 1) {
      $typewriter.textContent = text.slice(0, i);
      await sleep(STEP_MS);
    }
    // 打完后整体高亮
    $typewriter.innerHTML = highlight(demo.code);
    await sleep(450);
  }

  // 切 verdict
  $verdict.textContent = demo.verdict;
  $verdict.className = 'status-value verdict-' + demo.verdict;
  $meta.textContent = demo.meta;

  await sleep(reduced ? 2200 : 2600);

  $meta.textContent = '评测完成 · 准备下一题…';
  await sleep(reduced ? 200 : 400);
}

async function play() {
  const reduced = prefersReducedMotion();
  let i = 0;
  while (document.body.contains($typewriter)) {
    await playOne(DEMOS[i % DEMOS.length], reduced);
    i += 1;
  }
}

// DOMContentLoaded 后启动
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', play);
} else {
  play();
}