# MiniOJ — curl 接口自动化测试文档

> 本文档基于 **实际路由与字段** 整理（对照源码 `backend/src/http/router.cpp` / `handlers_*` / `submission_*` / `admin_*`），可直接拷贝运行。
>
> 未实现的端点会显式标注「❌ 尚未实现」，不要按示例照打。
>
> 一键冒烟脚本见仓库根 `api-smoke.sh`。

---

## 基础信息

| 项 | 值 |
|----|----|
| Base URL | `http://localhost:8080/api` |
| Content-Type | `application/json`（POST/PUT 必带） |
| 认证方式 | Session / Cookie，**Cookie 名 = `minioj_sid`** |
| 默认启动端口 | 8080（见 `backend/include/config.hpp`） |

---

## 准备工作

### 启动后端

#### 方式 A：Docker Compose（推荐，SPEC §9.2）

```bash
docker compose up -d                  # 起 mysql + backend + frontend
docker compose run --rm seed          # 首次：建表 + 灌种子题 + 创建 admin
# 访问 http://localhost                # 前端 80 → backend 8080
```

> 容器化路径下，后端运行在 `backend:8080` 容器内，Nginx 反代 `/api/*` 到该端口。本机 `curl` 时仍可用 `http://localhost:8080/api` 直接打后端（绕过前端反代，便于测试）。

#### 方式 B：裸机 / 本地开发（SPEC §9.4.3）

```bash
cd backend
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/minioj-backend                # 监听 0.0.0.0:8080
```

> 启动后会从环境变量（或 `backend/.env`）读 MySQL 连接信息，MySQL 不可达则启动失败。

### 创建 Cookie 文件目录

```bash
mkdir -p /tmp/minioj-cookies
```

### 公共常量（脚本里直接复用）

```bash
BASE_URL="http://localhost:8080/api"
COOKIE_USER="/tmp/minioj-cookies/user.txt"
COOKIE_ADMIN="/tmp/minioj-cookies/admin.txt"
```

---

## 1. 公开接口（无需登录）

### 1.1 获取题目列表

```bash
curl -sS "$BASE_URL/problems"
```

**预期**：200，JSON 数组，元素结构：

```json
{
  "id": 1,
  "title": "两数之和",
  "difficulty": "easy",
  "time_limit_ms": 500,
  "memory_limit_mb": 256,
  "tags": ["数组", "哈希表"]
}
```

> `difficulty` 取值固定为 `easy | medium | hard`（小写，见 `backend/src/http/problem_dto.cpp:13`）。

---

### 1.2 获取题目详情

```bash
# 正常
curl -sS "$BASE_URL/problems/1"

# 不存在 → 404
curl -sS -o /dev/null -w '%{http_code}\n' "$BASE_URL/problems/999999"
```

**预期**：

| 状态码 | 响应 |
|--------|------|
| 200 | 完整详情，含 `description_md`、`tags`（`[{id,name}]`）、`sample_testcases`（仅 `is_sample=1` 的用例） |
| 404 | `{"error":"problem not found"}` |

---

### 1.3 提交代码（判题）

```bash
curl -sS -X POST "$BASE_URL/submissions" \
  -H "Content-Type: application/json" \
  -d '{
    "problem_id": 1,
    "lang": "cpp",
    "code": "#include <iostream>\nusing namespace std;\nint main(){int a,b;cin>>a>>b;cout<<a+b<<endl;return 0;}"
  }'
```

**请求字段**（见 `backend/src/http/submission_request.cpp:45-58`）：

| 字段 | 类型 | 必填 | 约束 |
|------|------|------|------|
| `problem_id` | uint64 | ✅ | 必须 > 0 |
| `lang` | string | ✅ | 取值仅 `cpp` 或 `c` |
| `code` | string | ✅ | 非空，≤ 256 KiB |

**预期响应**：200，结构（见 `submission_dto.cpp`）：

```json
{
  "verdict": "WA",
  "time_ms": 12,
  "memory_mb": 3,
  "compile_output": "",
  "per_case": [
    {"index": 1, "verdict": "AC",  "time_ms": 2,  "memory_mb": 2},
    {"index": 2, "verdict": "WA",  "time_ms": 5,  "memory_mb": 2,
     "expected": "1 2", "actual": "1 3"}
  ]
}
```

> `expected` / `actual` **仅当 `verdict == "WA"` 时返回**（submission_dto.cpp:25）。

**错误响应**：

| 场景 | 状态码 | 响应 |
|------|--------|------|
| `lang` 缺失 / 非法值 | 400 | `{"error":"lang must be \"cpp\" or \"c\""}` |
| `code` 缺失 / 空 | 400 | `{"error":"code must not be empty"}` |
| `code` > 256 KiB | 400 | `{"error":"code must not exceed 256 KiB"}` |
| `problem_id` 不存在 | 404 | `{"error":"problem not found"}` |
| JSON 解析失败 | 400 | `{"error":"invalid JSON body"}` |

---

## 2. 用户账号接口

### 2.1 注册 `POST /api/auth/register`

```bash
# 正常（合法用户名 + 8 位含字母数字的密码）
curl -sS -i -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice01","password":"Passw0rd!"}' \
  -c "$COOKIE_USER"

# 用户名过短（< 3）→ 400
curl -sS -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"username":"ab","password":"Passw0rd!"}'

# 用户名含非法字符 → 400
curl -sS -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"username":"a-li-ce","password":"Passw0rd!"}'

# 密码过短（< 8）→ 400
curl -sS -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice02","password":"short1"}'

# 密码无字母或无数字 → 400
curl -sS -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice03","password":"12345678"}'

# 重复用户名 → 409
curl -sS -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice01","password":"Passw0rd!"}'
```

**校验规则**（`backend/src/auth/validator.cpp` + `validator.hpp:8-11`）：

- `username`：3–20 位，仅允许字母/数字/下划线，唯一
- `password`：8–64 位，必须同时包含字母和数字

**响应**：

| 状态码 | 响应体 | 副作用 |
|--------|--------|--------|
| 201 | `{"id":2,"username":"alice01","role":"user"}` | `Set-Cookie: minioj_sid=...`（注册即自动登录） |
| 400 | `{"error":"username must be 3 to 20 characters long"}` | — |
| 400 | `{"error":"username may only contain letters, digits, and underscores"}` | — |
| 400 | `{"error":"password must be 8 to 64 characters long"}` | — |
| 400 | `{"error":"password must contain both letters and digits"}` | — |
| 409 | `{"error":"username already exists"}` | — |

---

### 2.2 登录 `POST /api/auth/login`

```bash
curl -sS -i -X POST "$BASE_URL/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice01","password":"Passw0rd!"}' \
  -c "$COOKIE_USER"
```

**响应**：

| 状态码 | 响应体 | 副作用 |
|--------|--------|--------|
| 200 | `{"id":2,"username":"alice01","role":"user"}` 或 `...,"role":"admin"` | `Set-Cookie: minioj_sid=...` |
| 400 | `{"error":"missing or invalid 'username'"}` / `missing or invalid 'password'` | — |
| 401 | `{"error":"invalid username or password"}` | — |

> 用户名错误和密码错误统一返回 401 + 同一文案，避免账号枚举（见 `handlers_auth.cpp:155`）。

---

### 2.3 登出 `POST /api/auth/logout`

```bash
# 已登录
curl -sS -i -X POST "$BASE_URL/auth/logout" -b "$COOKIE_USER"

# 未登录也能登出（幂等）
curl -sS -X POST "$BASE_URL/auth/logout"
```

**响应**：

| 状态码 | 响应体 | 副作用 |
|--------|--------|--------|
| 200 | `{"status":"ok"}` | `Set-Cookie: minioj_sid=; Max-Age=0` 清空 Cookie；DB 删 session |

---

### 2.4 当前用户 `GET /api/auth/me`

```bash
# 已登录
curl -sS "$BASE_URL/auth/me" -b "$COOKIE_USER"

# 未登录
curl -sS -i "$BASE_URL/auth/me"
```

**响应**：

| 状态码 | 响应体 |
|--------|--------|
| 200 | `{"id":2,"username":"alice01","role":"user"}` |
| 401 | `{"error":"not logged in"}` 或 `{"error":"session expired or invalid"}` |

---

## 3. 管理员接口（需要 Session + `role=admin`）

> ✅ **当前实现状态**：
> - 题目 CRUD 的 5 个端点 ✅
> - **role 中间件已挂载**（`backend/src/http/admin_auth.cpp::installAdminAuth`），`/api/admin/*` 路由强制校验
> - 一键重置 `POST /api/admin/reset` ✅（配套 `backend/scripts/seed.cpp`，亦可走 seed 进程）
> - 单独的用例 CRUD、admin 独立登录端点 ❌ **未实装**（按 SPEC §6.3「已收敛」项，不提供）

### 3.1 列出全部题目（管理视图）`GET /api/admin/problems`

```bash
# admin 用户
curl -sS "$BASE_URL/admin/problems" -b "$COOKIE_ADMIN"

# 未登录 / 普通用户 → 401 / 403
curl -sS -o /dev/null -w '%{http_code}\n' "$BASE_URL/admin/problems"                       # → 401
curl -sS -o /dev/null -w '%{http_code}\n' "$BASE_URL/admin/problems" -b "$COOKIE_USER"   # → 403
```

**响应**：200，结构同 1.1 题单，**额外包含** `description_md` 完整字段（前端管理页要用来编辑）。

---

### 3.2 创建题目 `POST /api/admin/problems`

```bash
curl -sS -X POST "$BASE_URL/admin/problems" \
  -H "Content-Type: application/json" \
  -b "$COOKIE_ADMIN" \
  -d '{
    "title": "两数之和",
    "description_md": "给定两个整数 a, b，输出它们的和。",
    "difficulty": "easy",
    "time_limit_ms": 500,
    "memory_limit_mb": 256,
    "tags": ["数组", "哈希表"],
    "testcases": [
      {"input": "1 2\n",  "expected_output": "3\n", "is_sample": true,  "score": 50},
      {"input": "5 7\n",  "expected_output": "12\n","is_sample": false, "score": 50}
    ]
  }'
```

**响应实际文案**：`{"id": <new_id>, "message": "problem created"}` ← 旧文档占位，实际后端只返 `{"id": <new_id>}`。

**请求字段**（`backend/src/http/admin_request.cpp`）：

| 字段 | 类型 | 必填 | 约束 |
|------|------|------|------|
| `title` | string | ✅ | 非空 |
| `description_md` | string | ✅ | 非空 |
| `difficulty` | string | ✅ | `easy` / `medium` / `hard` |
| `time_limit_ms` | uint | ✅ | > 0 |
| `memory_limit_mb` | uint | ✅ | > 0 |
| `tags` | string[] | 否 | 不存在则视为空 |
| `testcases` | object[] | ✅ | 1–1000 项；元素含 `input` / `expected_output` 必填，可选 `is_sample`（bool）/ `score`（uint） |

**响应**：

| 状态码 | 响应体 |
|--------|--------|
| 201 | `{"id": 1, "message": "problem created"}`（**具体文案以 handler 为准**） |
| 400 | `{"error":"title is required"}` / `invalid difficulty, must be easy|medium|hard` / `time_limit_ms must be greater than 0` / `memory_limit_mb must be greater than 0` / `testcases must contain at least one entry` / `testcases must not exceed 1000 entries` / `input is required` / `expected_output is required` |

---

### 3.3 获取单个题目（管理视图）`GET /api/admin/problems/:id`

```bash
curl -sS "$BASE_URL/admin/problems/1" -b "$COOKIE_ADMIN"
```

**响应**：200，包含**全部** `testcases`（不只 sample，含 `is_sample` / `score`），其它字段同 3.2 请求体。404 同 1.2。

---

### 3.4 更新题目 `PUT /api/admin/problems/:id`

```bash
curl -sS -X PUT "$BASE_URL/admin/problems/1" \
  -H "Content-Type: application/json" \
  -b "$COOKIE_ADMIN" \
  -d '{
    "title": "两数之和（改）",
    "description_md": "...",
    "difficulty": "medium",
    "time_limit_ms": 1000,
    "memory_limit_mb": 128,
    "tags": ["数组"],
    "testcases": [
      {"input": "1 2\n", "expected_output": "3\n", "is_sample": true, "score": 100}
    ]
  }'
```

> 该端点在 DAO 内**事务级 upsert** tags + testcases：先全删再全插（见 `db::updateProblem`），所以请求体里 `testcases` 会被**整组替换**，不是增量。

**响应**：200 / 400 / 404（同上语义）。

---

### 3.5 删除题目 `DELETE /api/admin/problems/:id`

```bash
# 正常
curl -sS -i -X DELETE "$BASE_URL/admin/problems/1" -b "$COOKIE_ADMIN"

# 非数字 ID
curl -sS -i -X DELETE "$BASE_URL/admin/problems/abc" -b "$COOKIE_ADMIN"

# 不存在
curl -sS -i -X DELETE "$BASE_URL/admin/problems/999999" -b "$COOKIE_ADMIN"
```

**响应**（`handlers_admin.cpp:121-136`）：

| 场景 | 状态码 | 响应 |
|------|--------|------|
| 删除成功 | **204** | 空 body（注意不是 200） |
| ID 非法 | 400 | `{"error":"invalid problem id"}` |
| 题目不存在 | 404 | `{"error":"problem not found"}` |

---

### 3.6 单独用例 CRUD `❌ 尚未实现`

```
POST   /api/admin/testcases        ❌
PUT    /api/admin/testcases/:id    ❌
DELETE /api/admin/testcases/:id    ❌
```

> 当前通过 `POST/PUT /api/admin/problems` 在事务内一并管理（见 `db::createProblem` / `updateProblem`）。

### 3.7 管理员登录 `POST /api/admin/login` `❌ 尚未实现`

> 当前 admin 账号只能通过 `/api/auth/login` 用普通账号登录（admin 的 `role` 字段值为 `admin`）。管理路由层**未做 role 校验**，依赖反向代理或网络隔离。

### 3.8 管理员登出 `POST /api/admin/logout` `❌ 尚未实现`

> 与 3.7 同源问题：当前没有专属的管理员登出入口，admin 与普通用户共用 `/api/auth/logout`。该端点列在 SPEC §6.3 但 §10 Phase 5 TODO 漏写，**SPEC 内部口径不一致**（见 §8 偏差清单）。

### 3.9 一键重置 `POST /api/admin/reset` ✅

```bash
# 重新初始化题库为 backend/seed/problems.json 中的内置题
# （清空 problem_tags / testcases / problems / tags 四张表后再灌入，users / sessions 保留）
curl -sS -X POST "$BASE_URL/admin/reset" -b "$COOKIE_ADMIN"
```

**响应**：200

```json
{ "message": "problem bank reset to seed data", "seed": "<seed JSON 路径>" }
```

> 若想用本地 JSON 而非默认 `backend/seed/problems.json`，可设环境变量 `MINIOJ_SEED_JSON`。
> 等价的容器化做法：`docker compose --profile seed run --rm seed --reset`。

---

## 4. 权限与越权测试

```bash
# 用普通用户登录
curl -sS -X POST "$BASE_URL/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice01","password":"Passw0rd!"}' \
  -c "$COOKIE_USER"

# 三态断言：未登录 / 普通用户 / admin
for case in 'no cookie' 'normal user' 'admin user'; do :; done

# 1) 未登录访问 admin 列表
echo "[未登录]  $(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/admin/problems")"
# 期望: 401

# 2) 普通用户访问 admin 列表
echo "[普通用户] $(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/admin/problems" -b "$COOKIE_USER")"
# 期望: 403

# 3) admin 用户访问 admin 列表
echo "[admin]    $(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/admin/problems" -b "$COOKIE_ADMIN")"
# 期望: 200

# 4) 普通用户 POST 创建题目（应当被拦截）
curl -sS -i -X POST "$BASE_URL/admin/problems" \
  -H "Content-Type: application/json" \
  -b "$COOKIE_USER" \
  -d '{"title":"hack","description_md":"x","difficulty":"easy","time_limit_ms":1,"memory_limit_mb":1,"testcases":[{"input":"","expected_output":""}]}'
# 期望: 403，body 含 "admin role required"
```

**实际行为**：
- 未登录 → `401 {"error":"not logged in"}`
- 普通用户（已登录但 role≠admin） → `403 {"error":"admin role required"}`
- admin → 正常 200 / 201 / 204 等业务码

---

## 5. 一键冒烟脚本

### 5.1 bash 版：`api-smoke.sh`（兼容 / 最小依赖）

覆盖题单 / 详情 / 注册 / 登录 / 登出 / 提交 / 错误码等 21 条断言（与 SPEC §10 一致），依赖仅 `curl` + `jq`（可选）。

```bash
BASE_URL=http://127.0.0.1:8080/api \
ADMIN_USERNAME=admin ADMIN_PASSWORD=<seed 日志中的随机密码> \
./api-smoke.sh
```

### 5.2 Python 版：`test/python/test_minioj_api.py`（**与本表 1:1 对应**）

基于 `unittest` + `requests`，覆盖本文档 §1-§3.9 全部「期望响应」表，**~132 条断言、每条都同时校验 HTTP 状态码 + 响应 body 关键文案**。与 `api-smoke.sh` 的差异：

| 维度 | bash 版 | Python 版 |
|------|---------|----------|
| 依赖 | curl、jq（可选） | `pip install requests` |
| 用例数 | 21 条 | ~132 条 |
| session 隔离 | 多 cookie 文件，文件格式敏感 | `requests.Session()` 自带 jar |
| 错误码 / body 同时断言 | 多数只校验 status | **100% 同时校验 status + body 关键文案**（对应本文档各表「响应体」一列） |
| 退出码 | bash `exit 1` | `SystemExit(1)` 同样可被 CI 捕获 |
| 报告 | stdout 流式 | stdout 流式 + 末尾 `汇总 PASS/FAIL` |

```bash
# 依赖：pip install requests
python3 test/python/test_minioj_api.py
# 末尾输出：
#   Ran 9 tests in 8.5s
#   OK
#   ========== 汇总 ==========
#     PASS: 132
#     FAIL: 0
#   ALL TESTS PASSED
```

环境变量：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BASE_URL` | `http://127.0.0.1:8081/api` | 后端根地址 |
| `ADMIN_USER` | `admin` | 管理员账号（与 seed 写入一致） |
| `ADMIN_PASS` | `AdminPass1` | 管理员密码（与 seed --admin-password 一致） |
| `TEST_USER_PASSWORD` | `Passw0rdX` | 普通用户测试用密码（满足 8 位+含字母数字） |

覆盖明细（每个 `rec.check(...)` 为一条断言）：

| doc 章节 | 测试方法 | 断言点（含文案校验） |
|----------|---------|----------|
| §1.1 | `test_01_problem_list` | 题单 5 字段、tag 筛选、非法 difficulty → 400 |
| §1.2 | `test_02_problem_detail` | 详情 sample_testcases；404 + `problem not found`；非法 id → 400 |
| §1.3 | `test_03_submissions` | TLE/AC/WA（含 `expected`/`actual`）/CE（含 `compile_output`）；5 类错误码（lang/cpp、code 空、code 缺、code > 256 KiB、invalid JSON）+ 完整文案 |
| §2.1 | `test_04_register` | 201 + 3 字段 + Cookie 3 属性；8 类校验失败 + 4 段完整错误文案；409 重名 |
| §2.2 | `test_05_login` | 缺 username/password → 400；401 错密码与未知用户 + 防枚举文案一致 |
| §2.3 | `test_06_logout` | 200 + `status:ok` + `Max-Age=0`；幂等 |
| §2.4 | `test_07_me` | 登录 200；未登录 401 + `not logged in`；session 失效 401 + `session expired or invalid`；格式错 → 401 |
| §3.1-§3.5 | `test_08_admin_crud` | CRUD 全部错误码（404/400/403/401/201/204）+ 三态鉴权 + PUT 整组替换验证 + 各错误文案（`title is required` / `invalid problem id` / `problem not found`） |
| §3.9 | `test_09_admin_reset` | 三态权限；reset message + seed 字段；题数 5；旧 tag 清空；users/sessions 不受影响 |

如果 backend 不在线或 `BASE_URL` 错，`setUpClass` 会输出 `ConnectionRefusedError` 并给 `ERRORS=1`，适合放进 CI pipeline。

---

## 6. 错误码速查

| HTTP 状态码 | 含义 | 典型场景 |
|-------------|------|----------|
| 200 | 请求成功 | 查询 / 提交 |
| 201 | 创建成功 | 注册 / 创建题目 |
| 204 | 删除成功，无 body | `DELETE /api/admin/problems/:id` |
| 400 | 请求参数错误 | 缺字段、JSON 非法、校验失败 |
| 401 | 认证失败 | 未登录 / Session 过期 / 密码错 |
| 403 | 权限不足 | 已登录但非 admin 访问 `/api/admin/*`（`admin_auth.cpp:53`） |
| 404 | 资源不存在 | 题目 ID 无效 |
| 409 | 资源冲突 | 用户名重复 |
| 500 | 服务器内部错误 | DB 故障、判题子系统异常 |

---

## 7. verdict 枚举

| 值 | 含义 |
|----|------|
| `AC` | All Correct，全用例通过 |
| `WA` | Wrong Answer，输出不匹配（**会附 `expected` / `actual`**） |
| `TLE` | Time Limit Exceeded，单用例 > 500ms |
| `MLE` | Memory Limit Exceeded，单用例 > 256MB |
| `RE` | Runtime Error，非零退出码 / 信号 |
| `CE` | Compilation Error，编译失败（`compile_output` 字段非空） |

---

## 8. 与 SPEC 偏差清单（写文档时核对出来的问题）

| # | 现象 | 原因 |
|---|------|------|
| 1 | `difficulty` 是 `easy|medium|hard` 小写 | `problem_dto.cpp:13` 返回小写 |
| 2 | 提交请求字段是 `code`/`lang`/`problem_id`，**不是** `code`/`problem_id` | `submission_request.cpp:45-52` 强制要求 `lang` |
| 3 | 删除题目返回 **204**，不是 200/500 | `handlers_admin.cpp:130` `res.status = 204` |
| 4 | 注册/登录响应是 `{id, username, role}`，不是 `message` | `handlers_auth.cpp:117-120, 169-171` |
| 5 | 登录失败统一返 401 同文案 | `handlers_auth.cpp:155` 防枚举 |
| 6 | 错误响应**统一**是 `{"error": "..."}`，调用方直接读 `.error` | `writeError()` 封装 |
| 7 | Cookie 名是 `minioj_sid` | `session.hpp:10` 常量 |
| 8 | admin 路由无 role 校验 | ~~`router.cpp:15` 未挂中间件~~ → 已通过 `admin_auth.cpp::installAdminAuth` 在 pre-routing 阶段挂载 |
| 9 | 单独的用例 CRUD / admin 独立登录端点 未实装 | SPEC §6.3「已收敛」项（明确不做） |
| 10 | 一键重置 `POST /api/admin/reset` | ✅ 已实装，强制 role=admin，handler 调 `db::resetProblemBank` |
| 11 | seed 进程 `backend/scripts/seed.cpp` | ✅ 已实装，支持 `--reset` + `--admin-password` + 环境变量 `MINIOJ_SEED_JSON` |