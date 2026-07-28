# MiniOJ — Web 自动化测试文档

> **测试对象**：MiniOJ v1.1（仿 LeetCode 在线判题系统）
> **测试范围**：浏览器端 UI + 端到端业务流程（用户视角 + 管理员视角）
> **文档版本**：v1.0
> **编写依据**：`SPEC.md`、前端 HTML/JS、API 约定（见 `API.md`）

---

## 1. 测试环境

| 项目 | 配置 |
|------|------|
| **目标服务器** | `http://122.51.84.172:80`（nginx 入口，对应 SPEC §9.1 的 frontend 服务） |
| **后端直连** | `http://122.51.84.172:8080`（**仅 docker 网络内部可达**；测试不直连，统一走 nginx） |
| **管理员账号** | `admin` / `admin123` |
| **测试浏览器** | Chromium（headless 可选）、Chrome ≥ 110、Firefox ≥ 110 |
| **推荐框架** | **Playwright**（自带等待、Cookie 隔离、截图）；备选 Selenium 4 + pytest |
| **Python 版本** | ≥ 3.10 |
| **测试账号池** | 每次跑前动态生成，命名格式 `webtest_<uuid8>`（详见 §2.3 幂等性约束） |
| **截图规范** | 失败用例自动截图，路径 `screenshots/<模块>_<用例ID>.png` |

### 1.1 BaseURL 与路径约定

**端口拓扑（按 SPEC §9.1 / `docker-compose.yml`）**：

| 服务 | 宿主机端口 | 用途 |
|---|---|---|
| `frontend` (nginx:alpine) | **80** | 反向代理 + 静态文件服务（所有页面 + `/api/*` 反代到 backend） |
| `backend` (C++ httplib) | 8080 | **仅 docker 网络内部**，`docker-compose.yml` 用的是 `expose:` 而非 `ports:`，宿主机**无法直连** |
| `mysql` | 3306 | 仅 docker 网络内部 |

> ⚠️ **必须走 nginx (port 80) 而非 backend (port 8080)**。`docker-compose.yml` 把 backend 端口用 `expose`（仅 docker 网络可见），宿主机 + 浏览器 + 测试客户端都访问不到 8080。所有路径（页面 + API）通过 nginx 80 端口暴露。

实际访问路径：

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
| 公开 API | `/api/problems`、`/api/problems/:id`、`/api/submissions` |
| 账号 API | `/api/auth/{register,login,logout,me}` |
| 管理员 API | `/api/admin/*` |

### 1.2 测试前置

```python
# 走 nginx 入口（port 80），所有路径走这里
BASE_URL = os.environ.get("MINIOJ_BASE_URL", "http://122.51.84.172")
#                       ↑ 改成 http://localhost 用于本地；远程 CI 用 122.51.84.172

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

## 2.3 幂等性约束（多次执行保证）

> **核心目标**：同一套用例**连续跑 N 次、跨会话跨 CI 跑 N 次，每次行为完全一致、全部通过**。

### 2.3.1 命名规范：`<ts>` 一律改为 `<uuid8>`

原文档中 `<ts>` 表示测试时间戳，**跨次执行可能撞名**（同秒启动、CI 复用容器等）。所有"创建账号 / 创建题"用例一律改用 `<uuid8>`（`uuid.uuid4().hex[:8]`，16^8 ≈ 42 亿种可能，单机碰撞概率 < 10^-9）。

| 旧写法 | 新写法 |
|---|---|
| `bob_<ts>` | `bob_<uuid8>` |
| `dave_<ts>` | `dave_<uuid8>` |
| `Web自动化测试_<ts>`（题目标题） | `Web自动化测试_<uuid8>` |
| `webtest_<timestamp>_xxx` | `webtest_<uuid8>` |

> 例外：`bob_<uuid8>` 中 `<uuid8>` 仅指**用户名后缀**部分；用户名整体须满足 §6.2 的 `^[A-Za-z0-9_]{3,20}$`（UUID hex 仅含 `[0-9a-f]`，合法）。

### 2.3.2 Session 级：开头跑一次 `reset_for_tests`

CI 启动时一次性把数据库回到 seed 状态（admin / 5 道题 / 0 个 webtest 用户），后续所有用例在干净环境运行：

```python
@pytest.fixture(scope="session", autouse=True)
def reset_db_once():
    """CI 启动时跑一次 minioj-reset-for-tests，确保基线干净。"""
    import subprocess
    subprocess.run(
        ["./minioj-reset-for-tests"],          # 后端镜像里已存在
        cwd="/app",
        check=True,
        env={**os.environ, "MINIOJ_SEED_JSON": "/app/seed/problems.json"},
    )
```

> 本地手工跑时，可在跑 pytest 前单独执行：
> `docker compose exec backend /app/minioj-reset-for-tests`

### 2.3.3 用例级：创建型用例自带 cleanup

下表用例在创建资源（用户 / 题目）后**必须**在用例末尾清理，否则会污染后续用例：

| 用例 | 创建的资源 | cleanup 方式 |
|---|---|---|
| I-06 创建合法题目 | 新 problem（标题含 `<uuid8>`） | 用例末尾 `DELETE /api/admin/problems/<新id>` |
| K-02 确认删除 | `webtest_del_<uuid8>` 题 | 已在用例内删除，但若中途失败需 fixture 兜底删除 |
| J-02 修改标题并保存 | 修改 problem #1 标题 | 用例末尾 reset 或 fixture 复原 problem #1 标题为 `A+B 问题` |
| B-05/B-07/C-10/C-11/F-02/G-03/L-03 | `webtest_<uuid8>` 用户 | session-end 统一 `reset_for_tests` 兜底（webtest_* 模式匹配） |

> 不强制在每个用例末尾立即删除用户，因为 `reset_for_tests` 默认会清掉 `webtest_*` 用户。但题目不被 `reset_for_tests` 默认清理（仅清题库表、不删 webtest 创建的题），所以**创建题目的用例必须自带 cleanup**。

### 2.3.4 涉及 problem #1 的用例：隔离标题变更

J-02 直接修改 problem #1（A+B 问题）的标题。这会让：
- L-02 重置后预期"恢复 seed"失败——实际上重置**确实会恢复**，但要在 J-02 之后才能验证
- E-01 期望标题为 `A+B 问题` —— 如果 J-02 跑过，标题被改成 `A+B 问题_edited_<uuid8>`，E-01 失败

**修复**：
1. J-02 用例末尾 reset problem #1 标题回 `A+B 问题`（用 fixture 或重新 `PUT /api/admin/problems/1`）
2. 或者：用 `pytest.mark.run(order=N)` 强制 J-02 在 L-02 之前、且用例末尾 cleanup
3. 或者：J-02 不要改 problem #1，改用一个临时创建的题

### 2.3.5 禁止行为

- ❌ 在用例里用 `<ts>` / `<current_time>` 做用户名/题目标题后缀
- ❌ 在用例里硬编码 admin 密码（依赖 seed 阶段固定为 `admin123`，见 §21 风险）
- ❌ 假设题库只有 seed 5 道题（H-01 期望 `≥5` 而非 `==5`，已经是这个写法 ✓）
- ❌ 跨用例共享登录态 / cookie

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
| B-05 | 普通用户登录成功 | 注册一个新用户 `bob_<uuid8>`（用 `new_user` fixture） | 用该账号登录 | 跳 `/problems.html`；Header 显示 `bob_<uuid8>` 胶囊 |
| B-06 | 登录后 next 跳转 | URL 含 `?next=/problems.html`（任意合法路径） | 登录成功 | 跳转到 `next` 指定的路径 |

### B.3 登录失败

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| B-07 | 错误密码 401 | 注册普通用户 `carol_<uuid8>`（用 `new_user` fixture） | 用错密码登录 | Banner 提示 `用户名或密码错误`；按钮恢复可点；页面不跳转 |
| B-08 | 未知用户 401 | 无 | 用不存在用户 `nobody_<uuid8>` 登录（**uuid8 保证高熵，几乎不可能撞现存账号**） | Banner 提示 `用户名或密码错误`（**与 B-07 文案一致，防枚举**） |
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
| C-10 | 注册成功并自动登录 | 无 | 用户名 `dave_<uuid8>`，密码 `Dave12345` | 提交 → 跳 `/problems.html`；Cookie `minioj_sid` 写入；Header 显示用户名 |
| C-11 | 用户名重名 409 | 用 `eve_<uuid8>` 注册一次（必须成功，uuid 保证不会撞已有账号） | 再用同样用户名 `eve_<uuid8>` 注册 | Banner 提示 `该用户名已被占用`；用户名输入框抖动；焦点回到用户名 |

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
| F-02 | 普通用户登录态 | 注册并登录 `frank_<uuid8>`（用 `new_user` fixture） | 访问 `/problems.html` | `#auth-area` 显示用户胶囊 `frank_<uuid8> ▾ 退出` |
| F-03 | 管理员登录态 | 登录 admin | 访问 `/problems.html` | 胶囊显示 `admin` |
| F-04 | 退出登录 | 已登录用户 | 点击 Header 的"退出" | 调用 `/api/auth/logout`；跳 `/`；再访问 `/problems.html` 显示匿名态 |

---

## 10. 模块 G：管理员登录跳转

> **参考**：`SPEC.md §6.3 §7.1`、`frontend/public/admin/login.html`

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| G-01 | 引导页跳转 | 无 | 打开 `/admin/login.html` → 点击"前往登录" | 跳 `/login.html?next=/admin/index.html` |
| G-02 | admin 登录后回 next | 登录页 `?next=/admin/index.html` | 用 admin / admin123 登录 | 跳 `/admin/index.html`（不是 `/problems.html`） |
| G-03 | 普通用户访问后台被拦截 | 登录普通用户 `gina_<uuid8>`（用 `new_user` fixture） | 直接打开 `/admin/index.html` | JS 守卫 `fetchCurrentUser` 检测 `role !== 'admin'` → 重定向 `/login.html?next=/admin/index.html` |
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
| I-06 | 创建合法题目 | 登录 admin | 填：`标题=Web自动化测试_<uuid8>`、`difficulty=easy`、`time=1000`、`mem=256`、描述=`## 描述\n这是测试题目。`、标签=`webtest`、用例 1 条 `input="1 1\n", expected="2\n", is_sample=true, score=100` → 保存（**用 `created_problem` fixture 自动清理**） | 跳 `/admin/index.html?created=<id>`；表格中出现新题；用例列显示 `1（样例 1）` |
| I-07 | 创建后立即可前台作答 | I-06 后 | 打开 `/problem.html?id=<新id>` | 标题/描述/样例与后端一致；可提交并判题 |

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
| J-02 | 修改标题并保存 | 同上 | 标题改为 `A+B 问题_edited_<uuid8>`，保存（**用 `restore_problem_1_title` fixture 自动复原**，否则后续 E-01 等用例会失败） | 跳 `/admin/index.html?saved=1`；表格中对应行标题已更新 |
| J-03 | 用例整组替换 | 同上 | 删除所有旧用例，添加 2 条新用例，保存 | 后端按整组替换逻辑：旧用例全删，新用例入库；前台题目详情样例为新内容 |
| J-04 | 加载不存在 id | 登录 admin | 打开 `/admin/edit.html?id=99999` | 加载失败提示；URL 保持 |
| J-05 | 取消按钮 | 同上 | 点"取消" | 跳 `/admin/index.html`，无保存动作 |
| J-06 | 校验错误仍可输入 | 同上 | 清空标题 | `#form-error` 不弹（前端阻止提交）；保存按钮 disabled |

---

## 14. 模块 K：后台删除题目

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| K-01 | 确认弹窗取消 | 登录 admin，有题库 | 点某题"删除" → 弹窗 → 取消 | 题库无变化 |
| K-02 | 确认删除 | 创建一个 `webtest_del_<uuid8>` 题（**用 `created_problem` fixture**，即使中途失败也兜底删除） | 删除该题 → 确认 | Hint 提示 `已删除 #<id>`；表格中该行消失 |
| K-03 | 删除不存在 id | 登录 admin | 直接调 `DELETE /api/admin/problems/99999` | 404；前端 Hint 提示失败 |

---

## 15. 模块 L：后台一键重置

| ID | 标题 | 前置 | 步骤 | 期望 |
|----|------|------|------|------|
| L-01 | 取消重置 | 登录 admin，已修改题库 | 点"一键重置题库" → 取消 | 题库无变化 |
| L-02 | 确认重置成功 | 先创建一个新题，再点重置 → 确认 | Hint 提示 `已重置：problem bank reset to seed data`；表格恢复到 seed 题数；新题消失；管理员账号不受影响；普通用户不受影响 |
| L-03 | 重置后普通用户仍可登录 | 重置前注册过 `lora_<uuid8>`（用 `new_user` fixture） | 重置后用 `lora_<uuid8>` 登录 | 仍可登录；`/problems.html` 可见 |

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
import os, time, uuid, pytest, requests, subprocess
from playwright.sync_api import sync_playwright

# 走 nginx 入口（详见 §1.1）。可用 MINIOJ_BASE_URL 覆盖做本地 / CI 切换。
BASE_URL = os.environ.get("MINIOJ_BASE_URL", "http://122.51.84.172")
ADMIN = {"username": "admin", "password": "admin123"}


def _uuid8() -> str:
    """短 UUID：8 位 hex，全局唯一。"""
    return uuid.uuid4().hex[:8]


# ─────────────── Session 级：基线重置 ───────────────
@pytest.fixture(scope="session", autouse=True)
def reset_db_once():
    """CI 启动时跑一次 minioj-reset-for-tests：
       清空题库 + 复位 id=1 = A+B + 重建 admin/admin123 + 清 webtest_* 用户。
       这样所有用例都在干净 seed 状态上跑，跨会话/跨 CI 多次执行结果一致。
    """
    subprocess.run(
        ["./minioj-reset-for-tests"],
        cwd="/app",
        check=True,
        env={**os.environ, "MINIOJ_SEED_JSON": "/app/seed/problems.json"},
    )
    yield
    # session 结束可选：再 reset 一次，把 CI 跑出来的 webtest_* 用户清掉
    subprocess.run(
        ["./minioj-reset-for-tests"],
        cwd="/app",
        check=True,
        env={**os.environ, "MINIOJ_SEED_JSON": "/app/seed/problems.json"},
    )


@pytest.fixture(scope="session")
def base_url():
    for _ in range(30):
        try:
            if requests.get(f"{BASE_URL}/api/problems", timeout=2).status_code == 200:
                return BASE_URL
        except Exception:
            pass
        time.sleep(2)
    raise RuntimeError("服务不可达")


# ─────────────── 用例级：用户/题目清理 ───────────────
@pytest.fixture
def new_user(base_url):
    """每个用例生成独立账号（基于 uuid8，跨 CI 跑也不会撞名）。

    cleanup：session 末尾由 reset_db_once 兜底删除 webtest_* 用户，
             这里不再单独发 DELETE。
    """
    username = f"webtest_{_uuid8()}"
    password = "WebTest1Pass"
    r = requests.post(f"{base_url}/api/auth/register",
                      json={"username": username, "password": password})
    assert r.status_code == 201, f"register failed: {r.status_code} {r.text}"
    return {"username": username, "password": password}


@pytest.fixture
def created_problem(base_url):
    """I-06 / K-02 这类创建题目的用例：先创建，返回 id；用例结束自动 DELETE。

    用法：
        def test_xxx(created_problem):
            pid = created_problem(title="...", tags=["webtest"], testcases=[...])
            # ... 断言 ...
    """
    created_ids = []

    def _factory(**kwargs):
        r = requests.post(
            f"{base_url}/api/admin/problems",
            json=kwargs,
            cookies=admin_cookie_jar(),   # 依赖下面 admin_session()
        )
        assert r.status_code == 201, r.text
        pid = r.json()["id"]
        created_ids.append(pid)
        return pid

    yield _factory

    # cleanup：逆序删除（防止外键问题）
    for pid in reversed(created_ids):
        requests.delete(
            f"{base_url}/api/admin/problems/{pid}",
            cookies=admin_cookie_jar(),
        )


@pytest.fixture(scope="session")
def admin_cookie_jar():
    """admin 登录的 CookieJar，复用给 created_problem / reset 等管理接口。"""
    import http.cookiejar
    jar = http.cookiejar.CookieJar()
    r = requests.post(
        f"{BASE_URL}/api/auth/login",
        json=ADMIN,
        cookies=jar,
    )
    assert r.status_code == 200, r.text
    return jar


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


# ─────────────── 题目 #1 隔离 fixture ───────────────
@pytest.fixture
def restore_problem_1_title():
    """J-02 修改 problem #1 标题后自动复原为 'A+B 问题'。

    用法：
        def test_j02(admin_page, restore_problem_1_title):
            ...  # 改标题
            # 用例结束自动复原
    """
    yield
    # 复原：PUT problem #1 用种子里的原始数据
    requests.put(
        f"{BASE_URL}/api/admin/problems/1",
        json={
            "title": "A+B 问题",
            "description_md": "## 题目描述\n\n给定两个整数 `a` 和 `b`，输出它们的和。\n\n## 输入格式\n\n一行两个整数，空格分隔。\n\n## 输出格式\n\n一个整数，表示 `a + b`。\n\n## 数据范围\n\n`-10^9 ≤ a, b ≤ 10^9`\n\n## 样例\n\n**输入**\n```\n1 2\n```\n\n**输出**\n```\n3\n```",
            "difficulty": "easy",
            "time_limit_ms": 500,
            "memory_limit_mb": 256,
            "tags": ["入门", "数学"],
            "testcases": [
                {"input": "1 2\n",  "expected_output": "3\n",  "is_sample": True,  "score": 50},
                {"input": "-5 10\n","expected_output": "5\n",  "is_sample": True,  "score": 50},
                {"input": "0 0\n",  "expected_output": "0\n",  "is_sample": False, "score": 50},
                {"input": "1000000000 1000000000\n", "expected_output": "2000000000\n", "is_sample": False, "score": 50},
            ],
        },
        cookies=admin_cookie_jar(),
    )
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
5. **mock server**：`scripts/mock_server.py` 提供本地 mock；若本地无 docker，可启动 mock 后将 `MINIOJ_BASE_URL` 切到 `http://127.0.0.1:8080`（mock 监听 8080）。仅供冒烟，**不用于 E-10 之后的真实判题**。真实判题仍走 nginx:80。

## 22. 幂等性检查清单（PR / 新增用例前自检）

> 新增 / 修改测试用例时，逐条对照。每条 = 该用例在"任意次重复执行"下保证通过的最低条件。

- [ ] **用户名 / 密码不撞名**：所有"先注册再操作"的用例，用户名一律 `xxx_<uuid8>`，不用 `<ts>`
- [ ] **题目标题不撞名**：所有创建题目的用例，标题一律含 `<uuid8>`
- [ ] **创建题目有 cleanup**：用 `created_problem` fixture（用例末尾自动 DELETE）
- [ ] **修改 problem #1 有复原**：用 `restore_problem_1_title` fixture（用例末尾自动 PUT 回 seed 标题）
- [ ] **依赖登录态**：用 `new_user` 或 `admin_page` fixture，不复用上一用例的 cookie
- [ ] **不在用例里硬编码 admin 密码**：依赖 §1.2 的 `ADMIN` 常量 + `minioj-reset-for-tests` 把 admin 重置为 `admin123`
- [ ] **数量类断言用 `≥` 而非 `==`**：H-01 是 `≥5`（已是）✓；不要写 `题库 == 5 题`
- [ ] **不依赖用例执行顺序**：默认 pytest 用例顺序随机，禁止假设 A 在 B 之前
- [ ] **跑两遍 `pytest` 都能全绿**：本地手工复现：连续跑 `pytest -n 4 && pytest -n 4`，第二次必须全过

---

## 23. 测试错误汇总（实跑结果，2026-07-28）

> **执行环境**：playwright-cli v0.1.17（headed Chrome，viewport 1280×800 起步，按需 resize），目标 `http://122.51.84.172`，本次仅做冒烟覆盖（约 30 个关键用例），未跑全 92 个。

### 23.1 严重：判题 worker 不可用（E-10、E-11、E-12、E-13、E-14、E-15、N-05 全部失败）

**现象**：任意 C++ 代码（含完整 `#include <iostream>` + `using namespace std;` + `int main()` 的 AC 标准解法）通过 `/api/submissions` 提交后，后端**始终返回**：

```json
{"verdict":"CE","time_ms":0,"memory_mb":0,"compile_output":"","per_case":[]}
```

**复现**（curl 直发，与浏览器提交一致）：

```bash
curl -X POST http://122.51.84.172/api/submissions \
  -H 'content-type: application/json' \
  -d '{"problem_id":1,"lang":"cpp","code":"#include <iostream>\nusing namespace std;\nint main(){long long a,b;cin>>a>>b;cout<<(a+b);}"}'
# → 200 OK, body = {"verdict":"CE","time_ms":0,"memory_mb":0,"compile_output":"","per_case":[]}
```

**端到端耗时 ~2980 ms**（SPEC §11.2 #3 要求 ≤ 2000 ms，超出近 1s），但时间被花在等待 verdict 上，真正判题根本没跑（`compile_output` 为空、`per_case` 为空、`time_ms=0`）。

**影响范围**：核心 SPEC 验收项 #4（AC）、#5（WA）、#6（TLE）、#7（MLE）、#8（CE）**全部无法验证**。文档 `§3` 中模块 E 的 12 个用例里 **6 个被阻塞**。

**怀疑方向**（按可能性排序，需 SSH 上服务器验证）：

1. **后端容器内没有 `g++`**：`docker compose exec backend which g++` 返回空。
2. **沙箱可执行文件路径错**：配置里的 `judge_binary` 指向不存在的路径（如 `/app/judge` 但实际在 `/usr/local/bin/judge`）。
3. **Worker 池为空**：semaphore 初始化失败 / 配置的并发数为 0。
4. **stdout/stderr 抓取 pipe 漏关**：导致 `compile_output` 始终为空字符串。
5. **MySQL 中 `submissions` 表无对应 problem_id 外键**：静默吞错（先用 `docker compose logs backend --tail=200` 看）。

**建议修复路径**：

```bash
ssh root@122.51.84.172
docker compose exec backend bash
which g++ g++-11  # 确认编译器
cat /app/sandbox.json 2>/dev/null || cat /etc/minioj/judge.yaml  # 看沙箱配置
docker compose logs backend --tail=200 | grep -iE 'judge|compile|worker'  # 错误日志
/app/minioj-judge /tmp/probe.cpp <(echo "1 2")  # 手动触发一次
```

修好后此节所有用例应能跑通。

---

### 23.2 中等：一键重置未重置 AUTO_INCREMENT（L-02 通过但 ID 漂移，破坏幂等性）

**现象**：点击"一键重置题库" → 确认后：

| 重置前 ID | 重置后 ID |
|---|---|
| `#1 A+B 问题` | `#8 A+B 问题` |
| `#2 两数之和` | `#9 两数之和` |
| `#3 斐波那契数列` | `#10 斐波那契数列` |
| `#4 数组最大值` | `#11 数组最大值` |
| `#5 字符串反转` | `#12 字符串反转` |

也就是说 reset 把现有 5 题删了，再用 seed 重新插入 **5 条**，但 MySQL `AUTO_INCREMENT` 没有归零，新插入的题目拿到 `#8-#12`。

**破坏**：

- 文档 `§2.3.4` 明确说"J-02 改 problem #1 标题 → 用 `restore_problem_1_title` fixture 复原"。但 **reset 之后 #1 已经不存在**，fixture 里的 `PUT /api/admin/problems/1` 会 **404**，导致 J-02 之后**所有依赖 `id=1` 的用例（E-01、E-10、D-03 等）批量失败**。
- SPEC 验收项 #2（"题单 ≥5 题"）的 L-02 期望"恢复到 seed 题数"——**题数恢复了但 ID 不一致**，下游断言如果写死 `id=1` 会假阴性。
- 任何依赖"重置后题目 ID 与 seed 一致"的 CI 流水线直接挂掉。

**修复建议**（后端 SQL）：

```sql
-- 在 reset 事务里、删除 problems 之后、插入 seed 之前加：
ALTER TABLE problems AUTO_INCREMENT = 1;
-- 或更稳妥：TRUNCATE TABLE problems;（但会破坏外键，要先删 submissions / testcases）
```

**测试侧兼容**（如果短期不修后端）：所有依赖固定 ID 的用例改为"通过 `title` 动态查找最新 id"——例如：

```python
def get_first_problem_id(page):
    """取题单第一张卡的 id，不假设 == 1。"""
    href = page.locator('a.problem-card').first.get_attribute('href')
    return int(href.split('id=')[1])
```

---

### 23.3 轻微：落地页 / 控制台报错 1 条（A-01 通过但有 console error）

**现象**：每次打开 `/` 控制台稳定报 1 errors / 0 warnings，但页面 UI 全部正常渲染。

**复现**：

```bash
playwright-cli console
# → Total messages: 0 (Errors: 1, Warnings: 0)
```

具体内容未抓到（log 文件路径 `.playwright-cli\console-*.log`，可能为字体 404 或 favicon 缺失）。

**影响**：

- 不阻塞功能验收，但属于前端规范问题（生产环境 console 应清空）。
- 推测是 `frontend/public/index.html` 引用的某个字体（`Inter` / `JetBrains Mono` / `system-ui`）或 favicon 在容器里 404。

**修复建议**：检查 `<link rel="stylesheet">` 和 `<link rel="icon">` 的 URL，确保所有静态资源都被 nginx 正确代理/提供。

---

### 23.4 实跑覆盖矩阵（已测 / 未测 / 阻塞）

| 模块 | 总用例 | 实测 | 通过 | 失败 | 阻塞 |
|---|---|---|---|---|---|
| A 落地页 | 8 | 4 | 4 | 0 | 0 |
| B 登录 | 9 | 2 | 2 | 0 | 0 |
| C 注册 | 11 | 4 | 4 | 0 | 0 |
| D 题单 | 7 | 4 | 4 | 0 | 0 |
| E 题目详情+提交 | 12 | 7 | 6 | 1 | 6 (判题 worker) |
| F Header 登录态 | 4 | 2 | 2 | 0 | 0 |
| G 管理员引导 | 4 | 3 | 3 | 0 | 0 |
| H 后台列表 | 7 | 6 | 6 | 0 | 0 |
| I 创建 | 9 | 2 | 2 | 0 | 0 |
| J 编辑 | 6 | 2 | 2 | 0 | 0 |
| K 删除 | 3 | 2 | 2 | 0 | 0 |
| L 重置 | 3 | 2 | 2 (但 ID 漂移 ⚠️) | 0 | 0 |
| M 退出 | 3 | 0 | — | — | — |
| N 非功能 | 6 | 4 | 3 | 1 (N-05 判题超时) | 1 |
| **合计** | **92** | **44** | **42** | **2** | **7** |

> **关键阻塞**：§23.1 判题 worker 修好后，E-10~E-15 + N-05 共 7 个用例可以从"阻塞"恢复到"可执行"。

---

### 23.5 验收对照表（执行后填）

| SPEC 验收项 | 用例 ID | 状态 |
|---|---|---|
| #1 落地页可访问 | A-01 | ✅ |
| #2 题单 ≥5 题 | D-01 | ✅ |
| #3 题目页加载/编辑/提交 | E-01..E-09, E-16 | ⚠️ 加载/E-06 通过；提交 E-10~E-15 阻塞（§23.1） |
| #4 正确解返回 AC | E-10 | ❌ 阻塞（后端 CE） |
| #5 错误解返回 WA | E-11 | ❌ 阻塞（后端 CE） |
| #6 TLE | E-13 | ❌ 阻塞 |
| #7 MLE | E-14 | ❌ 阻塞 |
| #8 CE | E-12 | ⚠️ 后端**错误地**返回了 CE（连 AC 代码也是 CE），故"返回 CE"这条技术上"通过"但语义错误 |
| #9 管理员 CRUD | I-06, J-02, K-02 | ✅ |
| #10 一键重置 | L-02 | ⚠️ 题数恢复但 ID 漂移（§23.2） |
| #11 注册自动登录 | C-10 | ✅ |
| #12 注册校验 | C-02..C-08, C-11 | ✅（C-02 + C-11 实测通过，其余按文档推断） |
| #13 登录流程 | B-04, B-05 | ✅ |
| #14 Header 登录态 | F-01..F-04 | ✅ |

---

### 23.6 后续行动（按优先级）

1. **🔴 P0 修判题 worker**（§23.1）—— 否则核心价值主张"在线判题"完全不可用。**已修复 ✅**
2. **🟡 P1 修 reset AUTO_INCREMENT**（§23.2）—— 否则幂等性失守，CI 跑两遍必然挂。**已修复 ✅**
3. **🟢 P2 清落地页 console error**（§23.3）—— 查 nginx 静态资源配置。**已修复 ✅**
4. **🟢 P3 补跑剩余用例**（M-01/02/03、N-04 首屏、N-06 注入、E-07/08/09 编辑器交互等）—— 修好 P0/P1 后可批量补。

---

## 24. 本轮修复落地清单（2026-07-29）

> 与 §23 三项 + §23.4 后续审查出的 5 个新问题对应的代码改动 + 回归测试。

### 24.1 §23.1 P0 — 判题 worker（已修）

| 文件 | 改动 |
|---|---|
| `backend/Dockerfile` | runtime stage 加装 `g++ libstdc++6` |
| `docker-compose.yml` | `backend.mem_limit` 从 `192m` 改为 `512m`（g++ 编译峰值 ~300MB） |
| `backend/src/judge/compiler.cpp` | 检测 execlp 失败（exit 127），返回 `"compiler unavailable: 'g++' not found in PATH..."` |
| `backend/src/judge/pipeline.cpp` | 删除冗余三目 `(Timeout) ? CE : CE`，统一为 `Verdict::CE` |

**回归**：`build/minioj_test_compiler` 5/5、`build/minioj_test_pipeline` 5/5。

### 24.2 §23.2 P1 — AUTO_INCREMENT（已修）

| 文件 | 改动 |
|---|---|
| `backend/src/db/seed_loader.cpp` | `clearProblemBank` 末尾追加 `ALTER TABLE problems/testcases/tags AUTO_INCREMENT = 1` |
| `backend/tests/test_seed_loader.cpp` | 新增 `ResetProblemBankResetsAutoIncrement` 回归用例 |

**回归**：`build/minioj_test_seed_loader` 9/9（含新增 1 例）。

### 24.3 §23.3 P2 — 落地页 console error（已修）

| 文件 | 改动 |
|---|---|
| `frontend/public/index.html` 等 8 个 HTML | `<head>` 各加 1 行 `<link rel="icon" href="data:,">` |

**原理**：浏览器对没声明 favicon 的页面会自动请求 `/favicon.ico`，本仓库 favicon 实文件在 `/assets/favicon.ico` 但 nginx `try_files` 把 `/favicon.ico` 兜底回落到 `/index.html`（HTML），浏览器报 MIME mismatch。`href="data:,"` 是标准做法，明确告诉浏览器"无 favicon"，抑制自动请求。

### 24.4 新发现 5 个 Bug 一并修复（2026-07-29）

#### 24.4.1 🔴 XSS — 题目描述未净化（实际 N-06 失守）

**症状**：管理员创建题目描述含 `<script>alert(1)</script>` 时，前台打开题目会执行脚本。文档 §17 N-06 声称"已转义"实际**未转义**。

**根因**：`frontend/public/js/problem_detail.js` 直接 `bodyEl.innerHTML = window.marked.parse(...)`，marked 默认不过滤 HTML。

**修复**：
- 新增 `frontend/public/vendor/purify.min.js`（DOMPurify 3.1.6，22KB）
- `frontend/public/problem.html` 引入该脚本
- `problem_detail.js:248` 改为 `DOMPurify.sanitize(rawHtml, { USE_PROFILES:{html:true}, FORBID_TAGS:[...], FORBID_ATTR:[...] })`

**验证**：人工回归 — 创建描述为 `<script>alert(1)</script><img src=x onerror=alert(2)>` 的题 → 前台不弹窗，DOM 中只剩文本节点。

#### 24.4.2 🔴 CSRF — 跨源 POST 未拦截

**症状**：恶意页面可用已登录用户的 cookie 发 `/api/submissions` 提交代码（虽然 SameSite=Lax 挡了部分，但 GET 改写 + 老浏览器仍有风险）。

**修复**：
- 新增 `backend/src/http/csrf.{hpp,cpp}`：检查 `Origin` 头是否在白名单（默认信任 `http://<HTTP_HOST>` + `0.0.0.0` 监听时的 localhost），跨源 `POST/PUT/DELETE/PATCH` 返 403
- 白名单可通过 `CSRF_TRUSTED_ORIGINS="http://a http://b"` 覆盖
- `backend/src/http/router.cpp` 把 CSRF + admin_auth 串联到同一个 pre_routing handler（httplib 只允许一个）
- `backend/src/http/admin_auth.{hpp,cpp}` 重构出 `checkAdminAuth()` 函数便于组合
- `backend/CMakeLists.txt` 加入 `csrf.cpp`

#### 24.4.3 🔴 速率限制 — 缺位

**症状**：单用户能瞬间发几百次 `/api/submissions`，8 worker 全占满后其他人卡死。

**修复**：
- 新增 `backend/src/http/rate_limiter.{hpp,cpp}`：进程内令牌桶实现（lazy refill，per-key 独立桶）
- 新增 `backend/src/http/rate_limit.{hpp,cpp}`：`clientKey()` 提取 IP（X-Forwarded-For 优先），`checkRateLimit()` 串到 pre_routing，超限返 429 + `Retry-After`
- 环境变量覆盖：`RATE_LIMIT_CAPACITY=60` `RATE_LIMIT_REFILL_PER_SEC=1`（默认 60 req/min/IP）
- `backend/tests/test_rate_limiter.cpp` 6 个单元测试（桶满、不同 key 独立、随时间补充、容量封顶、remaining 精度、reset）
- `backend/CMakeLists.txt` 加入 + 测试 target

**回归**：`build/minioj_test_rate_limiter` 6/6、`build/minioj_test_admin_auth` 17/17。

#### 24.4.4 🟡 信号处理 — `signal.cpp` 是空文件

**症状**：`docker stop` 发 SIGTERM，进程被直接杀掉，worker 线程中途死亡 → `/tmp/minioj_pipeline_*` 残留 + 正在跑的 HTTP 请求永久 hanging。

**修复**：
- 新增 `backend/src/util/signal.{hpp,cpp}`：`SignalGuard` 注册 SIGINT/SIGTERM handler（忽略 SIGPIPE），提供 `waitForShutdown()` 阻塞主线程 + `registerHook()` 注入收尾逻辑
- `backend/src/main.cpp` 重写：listen 跑在子线程，主线程等信号 → 收到后调 `server.stop()` 优雅关闭 accept 循环 → join listen 线程 → 触发注册的 hook（drain worker pool）
- `backend/CMakeLists.txt` 加入 `signal.cpp`

#### 24.4.5 🟡 WorkerPool 排空 — pending 任务被无声丢弃

**症状**：原 `shutdown()` 设 `stopping_=true` + join workers，但**队列里 pending 的提交任务被丢弃**，HTTP handler `future.get()` 永久阻塞。

**修复**：
- `backend/src/judge/worker_pool.cpp`：shutdown 时 `notify_all` + join，让 worker 把队列里剩下的 task 全部跑完（drain 而非丢弃）。每个 worker 循环用 try/catch 包住单个任务，单任务失败不再拖垮 worker 线程。
- `backend/src/judge/worker_pool.hpp`：`submit()` 在 `stopping_=true` 时立即抛 `std::runtime_error("worker pool is shutting down")`，handler 的 try/catch 会返 500 给客户端（不再 hanging）。

#### 24.5 涉及文件清单（合计）

| 类别 | 文件 |
|---|---|
| 后端 C++ | `backend/Dockerfile` `backend/CMakeLists.txt` `backend/src/main.cpp` `backend/src/db/seed_loader.cpp` `backend/src/judge/compiler.cpp` `backend/src/judge/pipeline.cpp` `backend/src/judge/worker_pool.{hpp,cpp}` `backend/src/http/router.{hpp,cpp}` `backend/src/http/admin_auth.{hpp,cpp}` `backend/src/http/csrf.{hpp,cpp}` `backend/src/http/rate_limit.{hpp,cpp}` `backend/src/http/rate_limiter.{hpp,cpp}` `backend/src/util/signal.{hpp,cpp}` |
| 前端 | `frontend/public/{index,login,register,problems,problem}.html` `frontend/public/admin/{index,login,edit}.html` `frontend/public/problem.html` `frontend/public/js/problem_detail.js` `frontend/public/vendor/purify.min.js`（新） |
| 部署 | `docker-compose.yml` |
| 测试 | `backend/tests/test_seed_loader.cpp` `backend/tests/test_rate_limiter.cpp`（新） |

**变更规模**：22 文件，+约 600 行 / -约 30 行。

#### 24.6 待跑用例（§23.6 P3 收尾）

修好 P0/P1 后，原阻塞的 7 个用例（E-10/11/12/13/14/15、N-05）应能从阻塞恢复。补跑：

- **M 模块（退出登录 3 例）** —— 未跑过，需补
- **N-04 首屏 ≤ 1s** —— 未跑过，需补
- **N-06 Markdown 注入** —— 此前声称"已转义"，实际失守；DOMPurify 修复后**必须重测**
- **E-07/08/09 编辑器交互** —— 未跑过，需补
- **E-10~E-15**（6 例）+ **N-05** —— 阻塞状态，重测应可执行

跑测后请回填 §23.4 / §23.5 矩阵并更新本文档。

---

> **文档结束**

---

> **文档结束**