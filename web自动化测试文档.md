# MiniOJ — Web 自动化测试文档

> **测试对象**：MiniOJ v1.1（仿 LeetCode 在线判题系统）
> **测试范围**：浏览器端 UI + 端到端业务流程（用户视角 + 管理员视角）
> **文档版本**：v1.0
> **编写依据**：`SPEC.md`、前端 HTML/JS、API 约定（见 `API.md`）

---

## 1. 测试环境

| 项目 | 配置 |
|------|------|
| **目标服务器** | `http://122.51.84.172:8080` |
| **管理员账号** | `admin` / `admin123` |
| **测试浏览器** | Chromium（headless 可选）、Chrome ≥ 110、Firefox ≥ 110 |
| **推荐框架** | **Playwright**（自带等待、Cookie 隔离、截图）；备选 Selenium 4 + pytest |
| **Python 版本** | ≥ 3.10 |
| **测试账号池** | 每次跑前动态生成，命名格式 `webtest_<timestamp>_xxx` |
| **截图规范** | 失败用例自动截图，路径 `screenshots/<模块>_<用例ID>.png` |

### 1.1 BaseURL 与路径约定

服务器直接暴露 8080 端口（后端），由前端通过 Nginx 反代 `/` → `:80`、反代 `/api/*` → `:8080`。本测试环境按用户真实访问路径访问，路径如下：

| 模块 | URL |
|------|-----|
| 落地页 | `/` |
| 题单 | `/problems.html` |
| 题目详情 | `/problem.html?id=1` |
| 普通登录 | `/login.html` |
| 普通注册 | `/register.html` |
| 管理员引导 | `/admin/login.html` |
| 后台列表 | `/admin/index.html` |
| 新建/编辑题目 | `/admin/edit.html` 或 `/admin/edit.html?id=1` |

### 1.2 测试前置

```python
BASE_URL = "http://122.51.84.172:8080"
ADMIN = {"username": "admin", "password": "admin123"}

@pytest.fixture(scope="session")
def browser_context(browser):
    """每个测试用例独立上下文，cookie 隔离。"""
    ctx = browser.new_context(viewport={"width": 1280, "height": 800})
    yield ctx
    ctx.close()
```

---

## 2. 测试范围与策略

### 2.1 覆盖范围（按 SPEC.md §11.1 验收清单映射）

| SPEC 验收项 | 对应测试模块 |
|-------------|--------------|
| §11.1 #1 启动后访问落地页 `/` | 模块 A |
| §11.1 #2 题单显示 ≥5 题 | 模块 D |
| §11.1 #3 题目页加载/编辑/提交 | 模块 E |
| §11.1 #4 正确解返回 AC | 模块 E.3 |
| §11.1 #5-8 WA / TLE / MLE / CE 各种 verdict | 模块 E.3 |
| §11.1 #9 管理员 CRUD | 模块 G/H/I |
| §11.1 #10 管理员一键重置 | 模块 J |
| §11.1 #11 注册流程 | 模块 C |
| §11.1 #12 注册校验（409/400） | 模块 C.2 |
| §11.1 #13 登录流程 | 模块 B |
| §11.1 #14 Header 登录态切换 | 模块 F |

### 2.2 测试策略

1. **端到端优先**：每个用例模拟真实用户操作链。
2. **数据隔离**：每个用例使用独立账号，登录态相互不影响。
3. **判题结果依赖真实后端**：AC/WA/TLE/MLE/RE/CE 通过提交对应代码验证，不 mock。
4. **副作用清理**：每个用例结束前尝试 `POST /api/auth/logout`；涉及管理员 CRUD 的用例最后用 `POST /api/admin/reset` 复位（仅在用例显式声明时执行）。
5. **失败定位**：失败截图 + 保存 page html + 控制台日志。

---

## 3. 测试用例总览

| 编号 | 模块 | 用例数 |
|------|------|--------|
| A | 落地页 `/` | 8 |
| B | 普通登录 `/login.html` | 9 |
| C | 普通注册 `/register.html` | 11 |
| D | 题单 `/problems.html` | 7 |
| E | 题目详情与提交 `/problem.html` | 12 |
| F | Header 登录态切换 | 4 |
| G | 管理员登录跳转 | 4 |
| H | 后台列表 `/admin/index.html` | 7 |
| I | 后台创建题目 `/admin/edit.html` | 9 |
| J | 后台编辑题目 `/admin/edit.html?id=` | 6 |
| K | 后台删除题目 | 3 |
| L | 后台一键重置 | 3 |
| M | 退出登录 | 3 |
| N | 非功能（响应式 / 性能 / 安全） | 6 |
| **合计** | | **92** |

---

## 4. 模块 A：落地页 `/`

> **参考**：`SPEC.md §7.1`、`frontend/public/index.html`、`frontend/public/js/landing.js`

### A.1 页面元素完整性

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| A-01 | 落地页全元素渲染 | 无 | 打开 `/` | 状态 200；Title 含 `MiniOJ`；Header 显示品牌 `MiniOJ` + `题单` + `登录/注册`；Hero 出现 H1 文案 `像 LeetCode 一样刷题`；Features 区有 3 张特性卡；How 区有 3 步；CTA 与 Footer 存在 |
| A-02 | 终端装饰可见 | 无 | 打开 `/` | 右上终端框可见，3 个 macOS 圆点、`~/minioj — submission #1842` 标题、verdict 显示 `—` |
| A-03 | 终端打字机动画 | 无 | 打开 `/`，等待 2s | `#typewriter` 内出现非空内容（动态效果） |
| A-04 | 实时 verdict 轮播 | 无 | 打开 `/`，等待 3s | `#verdict` 在 AC / WA / TLE / CE 中循环切换 |

### A.2 Header 导航

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| A-05 | 顶部"题单"跳转 | 无 | 点击 Header 的"题单" | 跳转 `/problems.html` |
| A-06 | 顶部"登录"跳转 | 无 | 点击 Header 的"登录" | 跳转 `/login.html` |
| A-07 | 顶部"注册"跳转 | 无 | 点击 Header 的"注册" | 跳转 `/register.html` |
| A-08 | Hero CTA "立即刷题" | 无 | 点击 Hero 区"立即刷题" | 跳转 `/problems.html` |

---

## 5. 模块 B：普通登录 `/login.html`

> **参考**：`SPEC.md §6.2 §7.3`、`frontend/public/login.html`、`frontend/public/js/login.js`

### B.1 表单 UI 与交互

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| B-01 | 默认禁用提交 | 无 | 打开 `/login.html` | 提交按钮 `disabled`；用户名/密码框为空 |
| B-02 | 密码显隐切换 | 无 | 输入密码，点击眼睛图标 | 输入框 `type` 在 `password` ↔ `text` 切换；图标 SVG 同步切换 |
| B-03 | 缺用户名提示 | 无 | 只填密码，按钮可点 | 点击提交 → Banner 提示 `请填写用户名与密码`；用户名输入框红色抖动 |

### B.2 登录成功

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| B-04 | 管理员登录成功 | 无 | 用户名 `admin`，密码 `admin123` | 提交后页面跳到 `/problems.html`；Cookie `minioj_sid` 已写入；Header 显示用户胶囊 `admin ▾` |
| B-05 | 普通用户登录成功 | 注册一个新用户 `bob_<ts>` | 用该账号登录 | 跳 `/problems.html`；Header 显示 `bob_<ts>` 胶囊 |
| B-06 | 登录后 next 跳转 | URL 含 `?next=/problems.html`（任意合法路径） | 登录成功 | 跳转到 `next` 指定的路径 |

### B.3 登录失败

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| B-07 | 错误密码 401 | 注册普通用户 `carol_<ts>` | 用错密码登录 | Banner 提示 `用户名或密码错误`；按钮恢复可点；页面不跳转 |
| B-08 | 未知用户 401 | 无 | 用不存在用户 `nobody_<ts>` 登录 | Banner 提示 `用户名或密码错误`（**与 B-07 文案一致，防枚举**） |
| B-09 | 缺字段 400 | 无 | 直接 POST `/api/auth/login` body `{}`（或 username 为空字符串） | Banner 提示前端校验；后端若被绕过则返 400 |

---

## 6. 模块 C：普通注册 `/register.html`

> **参考**：`SPEC.md §6.2 §7.2`、`frontend/public/register.html`、`frontend/public/js/register.js`、`frontend/public/js/validation.js`

### C.1 表单校验（前端实时校验）

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| C-01 | 按钮默认禁用 | 无 | 打开 `/register.html` | 提交按钮 `disabled` |
| C-02 | 用户名太短 | 无 | 用户名输入 `ab` | Hint 提示 `用户名长度需在 3 到 20 个字符之间`；输入框 `data-state="err"`；按钮保持禁用 |
| C-03 | 用户名非法字符 | 无 | 用户名输入 `alice@` | Hint 提示 `仅允许字母、数字与下划线`；按钮禁用 |
| C-04 | 用户名合法 | 无 | 用户名输入 `alice_01` | Hint 变绿 `✓ 用户名可用`；`data-state="ok"` |
| C-05 | 密码无字母 | 无 | 密码输入 `12345678` | Hint 提示 `密码必须同时包含字母与数字` |
| C-06 | 密码无数字 | 无 | 密码输入 `abcdefgh` | Hint 提示 `密码必须同时包含字母与数字` |
| C-07 | 密码太短 | 无 | 密码输入 `Aa1` | Hint 提示 `密码长度需在 8 到 64 个字符之间` |
| C-08 | 两次密码不一致 | 无 | 密码 `Passw0rd`，确认 `Passw0rd!` | Hint 提示 `两次密码不一致` |
| C-09 | 强度条变化 | 无 | 密码从空→`a`→`Passw0rd`→`Passw0rd!@` | `#strength-bar` 的段数与颜色依次增加 |

### C.2 注册成功 / 失败

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| C-10 | 注册成功并自动登录 | 无 | 用户名 `dave_<ts>`，密码 `Dave12345` | 提交 → 跳 `/problems.html`；Cookie `minioj_sid` 写入；Header 显示用户名 |
| C-11 | 用户名重名 409 | 先注册 `eve_<ts>` | 再用同样用户名注册 | Banner 提示 `该用户名已被占用`；用户名输入框抖动；焦点回到用户名 |

---

## 7. 模块 D：题单 `/problems.html`

> **参考**：`SPEC.md §6.1 §7.1`、`frontend/public/problems.html`、`frontend/public/js/problem_list.js`

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| D-01 | 题单加载 | 无 | 打开 `/problems.html` | 卡片网格出现 ≥5 张题卡；右上 `count` 显示 `共 5 题`（或 seed 实际数） |
| D-02 | 卡片内容 | 无 | 检查任意题卡 | 难度徽章颜色正确（easy 绿/medium 橙/hard 红）、限制显示 `xxx ms · xxx MB`、标签 chip 存在 |
| D-03 | 进入题目详情 | 无 | 点击任意题卡 | 跳 `/problem.html?id=<id>`；URL 含正确 id |
| D-04 | 难度筛选-简单 | 无 | 难度下拉选 `easy` | 列表仅剩难度徽章为"简单"的题目 |
| D-05 | 难度筛选-中等 | 无 | 难度下拉选 `medium` | 列表仅剩"中等" |
| D-06 | 标签筛选 | 无 | 标签下拉选 `数组` | 列表仅剩含 `数组` 标签的题目 |
| D-07 | 筛选无结果 | 无 | 同时选不存在组合 | 显示 `暂无符合条件的题目` |

---

## 8. 模块 E：题目详情与提交 `/problem.html?id=`

> **参考**：`SPEC.md §6.1 §7.4 §8`、`frontend/public/problem.html`、`frontend/public/js/problem_detail.js`、`frontend/public/js/editor.js`

### E.1 页面加载与渲染

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| E-01 | 题目元数据 | 无 | 打开 `/problem.html?id=1`（seed 第一题 A+B） | 标题 `A+B 问题`；ID `#1`；难度徽章 `简单`；限制 `500 ms · 256 MB`；标签 chip 显示 |
| E-02 | Markdown 描述渲染 | 无 | 同上 | `#problem-body` 内有渲染后的 HTML（`h2`、`code`、`pre` 等），`escapeHtml` 不应将 Markdown 原文直接展示 |
| E-03 | 样例展示 | 无 | 同上 | 出现"样例 1"区，输入输出均为 `<pre>` 且不可注入 |
| E-04 | 缺少 id 参数 | 无 | 打开 `/problem.html`（无 `?id=`） | 标题显示 `未指定题目`；提交按钮禁用 |
| E-05 | 不存在 id | 无 | 打开 `/problem.html?id=99999` | 加载失败提示；提交按钮禁用 |
| E-06 | Ace 编辑器加载 | 无 | 打开题目页 | `#editor` 内有 `.ace_editor` DOM；编辑器内有 C++ 模板代码（默认） |

### E.2 编辑器交互

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| E-07 | 加载模板 | 无 | 切换语言为 `c`，再点"模板" | 编辑器内容切换为 C 模板（含 `<stdio.h>`） |
| E-08 | 切换语言自动加载模板 | 无 | 切换语言 `cpp` → `c` → `cpp` | 每次切换后编辑器自动重载对应语言模板 |
| E-09 | 重置清空 | 无 | 编辑器有内容，点"重置" → 确认 | 编辑器清空（弹窗 `window.confirm` 接受后） |

### E.3 提交判题（核心）

> 以下代码均针对 seed 第一题 `A+B 问题`（id=1，限制 500ms / 256MB）

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| E-10 | AC 正确解法 | 无 | 输入下方"代码 1"（正确 A+B），点击"提交" | 结果区 verdict = `AC`；`per_case` 全 `AC`；`time_ms` 与 `memory_mb` 均 >0 |
| E-11 | WA 错误解法 | 无 | 输入下方"代码 2"（输出错误） | verdict = `WA`；per_case 中失败用例带 `expected` / `actual` 字段；结果区显示对比块 |
| E-12 | CE 编译错误 | 无 | 输入下方"代码 3"（语法错误 `int main(`） | verdict = `CE`；结果区出现 `<pre class="compile-output">` 展示 stderr |
| E-13 | TLE 超时 | 无（**仅在 test 模式下放宽 CPU 限制后**；默认题目 500ms 极易触发） | 输入 `while(1);` 死循环 | verdict = `TLE`；verdict 徽章带 `TLE` 类 |
| E-14 | MLE 内存超限 | 无 | 输入下方"代码 4"（申请 `vector<int> a(1<<30);`） | verdict = `MLE` |
| E-15 | RE 运行时错误 | 无 | 输入下方"代码 5"（`int x[5]; return x[10];`） | verdict = `RE` |
| E-16 | 提交按钮 loading | 无 | 点击"提交" → 等待响应 | 按钮文字变为 `判题中…` + spinner；不可重复点击 |

**代码片段（用于 E-10 ~ E-15，提交语言 `cpp`）**：

```python
# 代码 1：AC
CODE_AC = r"""
#include <iostream>
using namespace std;
int main() {
    long long a, b;
    if (!(cin >> a >> b)) return 0;
    cout << (a + b) << "\n";
    return 0;
}
"""

# 代码 2：WA（输出 +1）
CODE_WA = r"""
#include <iostream>
using namespace std;
int main() {
    long long a, b;
    if (!(cin >> a >> b)) return 0;
    cout << (a + b + 1) << "\n";
    return 0;
}
"""

# 代码 3：CE（缺右括号）
CODE_CE = r"""
#include <iostream>
int main( {
    return 0;
}
"""

# 代码 4：MLE（申请大数组）
CODE_MLE = r"""
#include <bits/stdc++.h>
int main() {
    std::vector<int> a(1 << 30);  // ~4 GB，远超 256 MB
    (void)a;
    return 0;
}
"""

# 代码 5：RE
CODE_RE = r"""
#include <iostream>
int main() {
    int x[5] = {0};
    std::cout << x[100] << "\n";  // 越界访问 → 未定义行为 → RE 概率高
    return 0;
}
"""

# 代码 6：TLE
CODE_TLE = r"""
#include <iostream>
int main() {
    while (true) {}
    return 0;
}
"""
```

---

## 9. 模块 F：Header 登录态切换

> **参考**：`frontend/public/js/auth.js`

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| F-01 | 匿名态 Header | 未登录 | 打开 `/problems.html` | `#auth-area` 显示 `登录 注册` 两链接 |
| F-02 | 普通用户登录态 | 注册并登录 `frank_<ts>` | 访问 `/problems.html` | `#auth-area` 显示用户胶囊 `frank_<ts> ▾ 退出` |
| F-03 | 管理员登录态 | 登录 admin | 访问 `/problems.html` | 胶囊显示 `admin` |
| F-04 | 退出登录 | 已登录用户 | 点击 Header 的"退出" | 调用 `/api/auth/logout`；跳 `/`；再访问 `/problems.html` 显示匿名态 |

---

## 10. 模块 G：管理员登录跳转

> **参考**：`SPEC.md §6.3 §7.1`、`frontend/public/admin/login.html`

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| G-01 | 引导页跳转 | 无 | 打开 `/admin/login.html` → 点击"前往登录" | 跳 `/login.html?next=/admin/index.html` |
| G-02 | admin 登录后回 next | 登录页 `?next=/admin/index.html` | 用 admin / admin123 登录 | 跳 `/admin/index.html`（不是 `/problems.html`） |
| G-03 | 普通用户访问后台被拦截 | 登录普通用户 `gina_<ts>` | 直接打开 `/admin/index.html` | JS 守卫 `fetchCurrentUser` 检测 `role !== 'admin'` → 重定向 `/login.html?next=/admin/index.html` |
| G-04 | 匿名访问后台被拦截 | 未登录 | 直接打开 `/admin/index.html` | 同样重定向 `/login.html?next=/admin/index.html` |

---

## 11. 模块 H：后台列表 `/admin/index.html`

> **参考**：`frontend/public/admin/index.html`、`frontend/public/js/admin_list.js`

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| H-01 | 表格加载 | 登录 admin | 打开 `/admin/index.html` | 表格出现 ≥5 行；表头含 ID / 标题 / 难度 / 限制 / 标签 / 用例 / 操作 |
| H-02 | 当前管理员提示 | 同上 | 查看 Hint 区 | 显示 `当前以管理员 admin 登录` |
| H-03 | 用例数显示 | 同上 | 查看用例列 | 显示 `总数（样例 x）` |
| H-04 | 编辑跳转 | 同上 | 点击某行"编辑" | 跳 `/admin/edit.html?id=<id>` |
| H-05 | 跳转前台预览 | 同上 | 点击某行标题链接 | 跳 `/problem.html?id=<id>` |
| H-06 | 新建按钮 | 同上 | 点击"+ 新建题目" | 跳 `/admin/edit.html` |
| H-07 | 一键重置按钮可见 | 同上 | 视图右上 | "一键重置题库"按钮可见且 enabled |

---

## 12. 模块 I：后台创建题目 `/admin/edit.html`

> **参考**：`SPEC.md §6.3 §9.3`、`frontend/public/admin/edit.html`、`frontend/public/js/admin_edit.js`

### I.1 表单 UI 与校验

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| I-01 | 默认空表单 + 1 个空用例 | 登录 admin | 打开 `/admin/edit.html` | 标题 `新建题目`；默认 1 条用例（`is_sample=true, score=100`）；保存按钮 `disabled` |
| I-02 | 增加用例 | 无 | 点"+ 新增用例" | 列表追加一行；序号自增 `#2`；原 `#1` 序号保持 |
| I-03 | 删除用例 | 无 | 点某行的"删除" | 该行移除；剩余行重新 `#1, #2, …` 编号 |
| I-04 | 标签预览 | 无 | 标签输入 `数组, 哈希表` | `#tags-preview` 显示两个 chip |
| I-05 | 标题为空 | 无 | 标题留空 | 保存按钮保持禁用 |

### I.2 创建成功

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| I-06 | 创建合法题目 | 登录 admin | 填：`标题=Web自动化测试_<ts>`、`difficulty=easy`、`time=1000`、`mem=256`、描述=`## 描述\n这是测试题目。`、标签=`webtest`、用例 1 条 `input="1 1\n", expected="2\n", is_sample=true, score=100` → 保存 | 跳 `/admin/index.html?created=<id>`；表格中出现新题；用例列显示 `1（样例 1）` |
| I-07 | 创建后立即可前台作答 | 创建后 | 打开 `/problem.html?id=<新id>` | 标题/描述/样例与后端一致；可提交并判题 |

### I.3 创建失败

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| I-08 | 后端校验 time_limit=0 | 登录 admin | 时间限制填 `0` | 前端校验 `时间限制必须大于 0` → 按钮禁用；如绕过前端 `apiPost` → 后端返 400 |
| I-09 | 后端校验无 description | 登录 admin | 描述为空（绕过前端） | 后端 400；前端 `#form-error` 显示错误 |

---

## 13. 模块 J：后台编辑题目 `/admin/edit.html?id=`

> **参考**：`frontend/public/admin/edit.html`、`frontend/public/js/admin_edit.js`

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| J-01 | 回显表单 | 登录 admin | 打开 `/admin/edit.html?id=1` | 标题 `编辑题目 #1`；表单回填 title / difficulty / time / mem / desc / tags；用例列表回填全部 testcases |
| J-02 | 修改标题并保存 | 同上 | 标题改为 `<原标题>_edited_<ts>`，保存 | 跳 `/admin/index.html?saved=1`；表格中对应行标题已更新 |
| J-03 | 用例整组替换 | 同上 | 删除所有旧用例，添加 2 条新用例，保存 | 后端按整组替换逻辑：旧用例全删，新用例入库；前台题目详情样例为新内容 |
| J-04 | 加载不存在 id | 登录 admin | 打开 `/admin/edit.html?id=99999` | 加载失败提示；URL 保持 |
| J-05 | 取消按钮 | 同上 | 点"取消" | 跳 `/admin/index.html`，无保存动作 |
| J-06 | 校验错误仍可输入 | 同上 | 清空标题 | `#form-error` 不弹（前端阻止提交）；保存按钮 disabled |

---

## 14. 模块 K：后台删除题目

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| K-01 | 确认弹窗取消 | 登录 admin，有题库 | 点某题"删除" → 弹窗 → 取消 | 题库无变化 |
| K-02 | 确认删除 | 创建一个 `webtest_del_<ts>` 题 | 删除该题 → 确认 | Hint 提示 `已删除 #<id>`；表格中该行消失 |
| K-03 | 删除不存在 id | 登录 admin | 直接调 `DELETE /api/admin/problems/99999` | 404；前端 Hint 提示失败 |

---

## 15. 模块 L：后台一键重置

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| L-01 | 取消重置 | 登录 admin，已修改题库 | 点"一键重置题库" → 取消 | 题库无变化 |
| L-02 | 确认重置成功 | 先创建一个新题，再点重置 → 确认 | Hint 提示 `已重置：problem bank reset to seed data`；表格恢复到 seed 题数；新题消失；管理员账号不受影响；普通用户不受影响 |
| L-03 | 重置后普通用户仍可登录 | 重置前注册过 `lora_<ts>` | 重置后用 `lora_<ts>` 登录 | 仍可登录；`/problems.html` 可见 |

---

## 16. 模块 M：退出登录

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| M-01 | 头部退出按钮 | 任意已登录用户 | 在 `/problems.html` 点 Header 的"退出" | 调 `/api/auth/logout`；Cookie 清空；跳 `/`；再访问受限页未拦截（题单页是公开） |
| M-02 | 后台页退出 | admin | 在 `/admin/index.html` 点 Header 的"退出" | 同样清 cookie；后续访问 `/admin/index.html` 跳登录 |
| M-03 | 退出幂等 | 已退出态 | 直接 POST `/api/auth/logout` | 返 200 + `{"status":"ok"}`；Set-Cookie `Max-Age=0` |

---

## 17. 模块 N：非功能

### N.1 响应式

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| N-01 | 落地页桌面 1280 | 无 | viewport 1280×800 打开 `/` | 无横向滚动条；Hero 左右两栏正常 |
| N-02 | 落地页平板 1024 | 无 | viewport 1024×768 | Hero 单/双栏自适应；How 步骤不溢出 |
| N-03 | 落地页手机 375 | 无 | viewport 375×667 | 终端框与卡片单列堆叠；CTA 按钮可点击 |

### N.2 性能（SPEC §11.2）

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| N-04 | 落地页首屏 ≤ 1s | 无冷启动 | `performance.timing.domContentLoaded - navigationStart` | ≤ 1000 ms（参考 SPEC §11.2 #3） |
| N-05 | 单用例判题 ≤ 2s | 单题 AC 提交 | POST 后到 verdict 出现 | ≤ 2000 ms |

### N.3 安全 / 注入

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| N-06 | 描述 Markdown 注入 | 登录 admin | 创建题目，描述含 `<script>alert(1)</script>` | 前台渲染时**已转义**，不弹窗；查看 `#problem-body` 看到的是文本而非脚本节点 |

---

## 18. 自动化框架建议（Playwright + pytest）

```python
# conftest.py 关键片段
import os, time, pytest, requests
from playwright.sync_api import sync_playwright

BASE_URL = "http://122.51.84.172:8080"
ADMIN = {"username": "admin", "password": "admin123"}
TS = str(int(time.time()))

@pytest.fixture(scope="session")
def base_url():
    # 健康检查：等待服务可用
    for _ in range(30):
        try:
            if requests.get(f"{BASE_URL}/api/problems", timeout=2).status_code == 200:
                return BASE_URL
        except Exception:
            pass
        time.sleep(2)
    raise RuntimeError("服务不可达")

@pytest.fixture
def new_user(base_url):
    """每个用例生成独立账号（注册一次，跨用例隔离）。"""
    username = f"webtest_{TS}_{int(time.time()*1000) % 100000}"
    password = "WebTest1Pass"
    requests.post(f"{base_url}/api/auth/register",
                  json={"username": username, "password": password})
    return {"username": username, "password": password}

@pytest.fixture
def admin_page(base_url):
    """已登录 admin 的 page。"""
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        ctx = browser.new_context()
        page = ctx.new_page()
        page.goto(f"{base_url}/login.html?next=/admin/index.html")
        page.fill("#login-username", ADMIN["username"])
        page.fill("#login-password", ADMIN["password"])
        page.click("#submit-btn")
        page.wait_for_url("**/admin/index.html")
        yield page
        ctx.close()
        browser.close()
```

### 18.1 判题结果等待工具

```python
def wait_for_verdict(page, timeout_ms=15_000):
    """等待 #result 区出现非 CE/loading 的 verdict badge。"""
    page.wait_for_selector("#result .verdict-badge:not(.CE):not(.ERR)", timeout=timeout_ms)
    # CE 与 ERR 走单独路径
    badge = page.locator("#result .verdict-badge").first
    return badge.inner_text().strip()
```

### 18.2 失败用例清理（可选）

```python
@pytest.fixture(autouse=True)
def screenshot_on_failure(request, page):
    yield
    if request.node.rep_call.failed:
        page.screenshot(path=f"screenshots/{request.node.name}.png", full_page=True)
```

---

## 19. 跑测命令

```bash
# 安装依赖
pip install playwright pytest pytest-playwright requests
playwright install chromium

# 跑全部（默认并行 4 worker）
pytest -n 4 --maxfail=5 -q

# 只跑判题核心
pytest -k "E_1" -v

# 只跑管理员模块
pytest test_admin_*.py -v
```

---

## 20. 验收对照表（与 SPEC.md §11.1 对齐）

| SPEC 验收项 | 用例 ID | 状态（执行后填） |
|-------------|---------|------------------|
| #1 落地页可访问 | A-01 |  |
| #2 题单 ≥5 题 | D-01 |  |
| #3 题目页加载/编辑/提交 | E-01..E-09, E-16 |  |
| #4 正确解返回 AC | E-10 |  |
| #5 错误解返回 WA | E-11 |  |
| #6 TLE | E-13 |  |
| #7 MLE | E-14 |  |
| #8 CE | E-12 |  |
| #9 管理员 CRUD | I-06, J-02, K-02 |  |
| #10 一键重置 | L-02 |  |
| #11 注册自动登录 | C-10 |  |
| #12 注册校验 | C-02..C-08, C-11 |  |
| #13 登录流程 | B-04, B-05 |  |
| #14 Header 登录态 | F-01..F-04 |  |

---

## 21. 风险与备注

1. **TLE/MLE/RE 依赖后端真实判题**：若服务端资源限制不同（如内存默认 256 MB），E-13/14/15 的代码片段需按实测阈值微调（建议先用小用例探阈值）。
2. **判题用时波动**：E-13 死循环默认 500ms 超时即 TLE；E-15 越界访问在不同 g++ 版本下可能不会触发 RE（建议增加 `*nullptr` 等更确定行为）。
3. **管理员重置是破坏性操作**：建议在测试前备份或使用专用测试库；CI 中只在测试结束后执行一次。
4. **页面改动耦合**：本测试依赖 `id="..."` 选择器（登录按钮 `#submit-btn`、输入框 `#login-username` 等），前端 ID 变更需同步更新本测试。
5. **mock server**：`scripts/mock_server.py` 提供本地 mock；若本地无 docker，可启动 mock 后将 `BASE_URL` 切换到 `http://127.0.0.1:8080`（仅供冒烟，不用于 E-10 之后的真实判题）。

---

> **文档结束**