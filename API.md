# MiniOJ 后端 API 文档

> **基础信息**
> - 所有接口统一返回 JSON，Content-Type: `application/json; charset=utf-8`
> - 基础前缀：`/api`
> - 错误响应统一格式：`{"error": "<message>"}`
> - 鉴权：除标注"无"的接口外，Session 通过 `Cookie: minioj_sid=<32字节hex>` 传递
> - 角色：本系统有两类账号——`user`（普通用户，注册产生）与 `admin`（管理员，seed 脚本创建）。当前 admin 接口的鉴权中间件尚未接入，路由开放但需要内网/前置网关保护。

---

## 1. 接口总览

### 1.1 公开接口（无需鉴权）

| 方法 | 路径 | 作用 |
|------|------|------|
| GET | `/api/problems` | 题目列表（可按难度/标签筛选，不含 description） |
| GET | `/api/problems/:id` | 题目详情（含 description 与 sample 用例） |
| POST | `/api/submissions` | 提交代码判题，同步返回 AC/WA/TLE/CE/MLE/RE |

### 1.2 普通用户账号接口

| 方法 | 路径 | 作用 | 鉴权 |
|------|------|------|------|
| POST | `/api/auth/register` | 注册新用户（自动建立 Session） | 无 |
| POST | `/api/auth/login` | 用户名+密码登录 | 无 |
| POST | `/api/auth/logout` | 注销当前 Session（幂等） | Session（无 cookie 也返回 200） |
| GET  | `/api/auth/me` | 获取当前登录用户基本信息 | Session |

### 1.3 管理员接口（题库 CRUD）

| 方法 | 路径 | 作用 | 鉴权 |
|------|------|------|------|
| GET    | `/api/admin/problems` | 列出全部题目（含完整数据） | admin（**当前未注入鉴权**，见 §5 说明） |
| POST   | `/api/admin/problems` | 新建题目（带 tags + testcases） | admin（同上） |
| GET    | `/api/admin/problems/:id` | 读取单个题目详情（含全部 testcases） | admin（同上） |
| PUT    | `/api/admin/problems/:id` | 全量更新题目（替换 tags + testcases） | admin（同上） |
| DELETE | `/api/admin/problems/:id` | 删除题目（级联清理 testcases/tags 关联） | admin（同上） |

> **未实现的接口**（SPEC §6.3 列出，Phase 5 待办）：
> - `POST /api/admin/login`、`POST /api/admin/logout`
> - `POST /api/admin/testcases`、`PUT /api/admin/testcases/:id`、`DELETE /api/admin/testcases/:id`
> - `POST /api/admin/reset`

---

## 2. 公开接口

### 2.1 GET /api/problems

获取题目列表。每条仅含摘要信息（不含 `description_md`，不含非 sample 用例）。

**Query 参数**（均可选，组合筛选）：

| 参数 | 类型 | 说明 |
|------|------|------|
| `difficulty` | string | `easy` \| `medium` \| `hard`，未知值返回 400 |
| `tag` | string | 标签名（精确匹配），返回包含该标签的所有题目 |

**请求示例**
```
GET /api/problems?difficulty=easy&tag=%E6%95%B0%E7%BB%84
```

**响应 200**
```json
[
  {
    "id": 1,
    "title": "两数之和",
    "difficulty": "easy",
    "time_limit_ms": 500,
    "memory_limit_mb": 256,
    "tags": ["数组", "哈希表"]
  }
]
```

**字段说明**：`time_limit_ms` / `memory_limit_mb` 为该题的判题资源上限（与 §8.1 对齐）。

**错误码**：`400`（非法 difficulty）

---

### 2.2 GET /api/problems/:id

获取单个题目详情，包含 Markdown 描述 + sample 用例。

**路径参数**：`id`（uint64）

**响应 200**
```json
{
  "id": 1,
  "title": "两数之和",
  "description_md": "给定一个整数数组 `nums` 和目标值 `target`...请返回两个下标。",
  "difficulty": "easy",
  "time_limit_ms": 500,
  "memory_limit_mb": 256,
  "tags": [
    {"id": 1, "name": "数组"},
    {"id": 2, "name": "哈希表"}
  ],
  "sample_testcases": [
    {"id": 11, "input": "2 7 11 15\n9\n", "expected_output": "0 1\n"}
  ]
}
```

**错误码**：`400`（id 非数字）、`404`（题目不存在）

---

### 2.3 POST /api/submissions

提交代码进行同步判题。提交记录**不持久化**，仅返回本次结果。

**请求体**
```json
{
  "problem_id": 1,
  "lang": "cpp",
  "code": "#include <iostream>\n..."
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `problem_id` | uint64 | ✓ | 题目 id |
| `lang` | string | ✓ | `"cpp"` \| `"c"` |
| `code` | string | ✓ | 源码，1 B ~ 256 KiB |

**响应 200**（成功判题）
```json
{
  "verdict": "WA",
  "time_ms": 12,
  "memory_mb": 3,
  "compile_output": "",
  "per_case": [
    {"index": 1, "verdict": "AC",  "time_ms": 2, "memory_mb": 1},
    {"index": 2, "verdict": "WA",  "time_ms": 5, "memory_mb": 2,
     "expected": "0 1", "actual": "0 2"}
  ]
}
```

**verdict 取值**：`AC | WA | TLE | CE | MLE | RE`

**字段语义**：
- 顶层 `time_ms` / `memory_mb`：单次提交**最快/峰值**用例（汇总口径，见 `submission_dto.cpp`）
- `per_case[i].time_ms` / `memory_mb`：该用例的单次耗时与 RSS
- `per_case[i].expected` / `actual`：**仅当 verdict == WA 时出现**
- `compile_output`：仅当 verdict == CE 时非空，承载 stderr

**错误码**：
- `400`：JSON 不合法 / 字段缺失 / 字段类型错 / `lang` 非 `cpp|c` / `code` 超过 256 KiB
- `404`：题目不存在
- `500`：判题子系统异常（编译/运行子进程 spawn 失败等）

---

## 3. 用户账号接口

### 3.1 POST /api/auth/register

注册普通用户，成功等价于自动登录。

**请求体**
```json
{
  "username": "alice",
  "password": "Passw0rd!"
}
```

**校验规则**：
- `username`：3–20 位，`[A-Za-z0-9_]`，全局唯一
- `password`：8–64 位，至少含一个字母与一个数字

**响应 201**
```json
{
  "id": 2,
  "username": "alice",
  "role": "user"
}
```
同时设置 Cookie：`Set-Cookie: minioj_sid=<32字节hex>; HttpOnly; SameSite=Lax; Max-Age=3600[; Secure]`

**错误码**：
- `400`：JSON 不合法 / 字段缺失 / `username` 或 `password` 校验失败
- `409`：`username already exists`

---

### 3.2 POST /api/auth/login

用户名 + 密码登录。

**请求体**：同 §3.1。

**响应 200**
```json
{
  "id": 2,
  "username": "alice",
  "role": "user"
}
```
同样设置 `minioj_sid` Cookie。

**错误码**：
- `400`：JSON 不合法 / `username` 或 `password` 缺失或为空
- `401`：`invalid username or password`（统一文案，避免用户名枚举）
- `500`：数据库异常

> **已知 trade-off**：未做"用户不存在时跑假 bcrypt"的时序对齐（见 handlers_auth.cpp:153 TODO）。攻击者理论上可通过响应耗时区分用户存在与否。对内网场景可接受。

---

### 3.3 POST /api/auth/logout

销毁当前 Session。**幂等**：即使没有有效 Cookie 也返回 200。

**请求体**：任意（后端忽略），通常发 `{}`。

**响应 200**
```json
{ "status": "ok" }
```
始终设置清除 Cookie：`Set-Cookie: minioj_sid=; ...; Max-Age=0`。

**错误码**：`500`（数据库异常 — 已删 session 后才会到达）

---

### 3.4 GET /api/auth/me

探测当前登录态。**不返回 `password_hash`**。

**请求**：携带 `Cookie: minioj_sid=...`

**响应 200**
```json
{
  "id": 2,
  "username": "alice",
  "role": "user"
}
```

**错误码**：
- `401`：`not logged in`（无 Cookie）/ `session expired or invalid`（Cookie 过期或未注册）

---

## 4. 管理员接口（题库 CRUD）

> **当前状态**：`handlers_admin.cpp:22` 留有 `TODO(phase2): 注入 admin 角色鉴权`。注册路由已就位，但**未做角色校验**。**生产部署必须**通过 nginx 反代 ACL 或独立 admin 子域限制访问。

### 4.1 公共 DTO

**AdminProblemDetail**
```json
{
  "id": 1,
  "title": "两数之和",
  "description_md": "...",
  "difficulty": "easy",
  "time_limit_ms": 500,
  "memory_limit_mb": 256,
  "tags": [{"id": 1, "name": "数组"}],
  "testcases": [
    {
      "id": 11,
      "input": "...",
      "expected_output": "...",
      "is_sample": true,
      "score": 10
    }
  ]
}
```

**AdminTestCase**
```json
{
  "id": 11,
  "input": "...",
  "expected_output": "...",
  "is_sample": true,
  "score": 10
}
```

---

### 4.2 GET /api/admin/problems

列出**全部**题目（含完整数据与所有 testcases）。

**响应 200**：`AdminProblemDetail[]`

---

### 4.3 GET /api/admin/problems/:id

读取单个题目完整数据。

**响应 200**：`AdminProblemDetail`
**错误码**：`400`（id 非数字）、`404`（不存在）

---

### 4.4 POST /api/admin/problems

新建题目。tags 与 testcases 在同一事务内 upsert。

**请求体**
```json
{
  "title": "两数之和",
  "description_md": "...",
  "difficulty": "easy",
  "time_limit_ms": 500,
  "memory_limit_mb": 256,
  "tags": ["数组", "哈希表"],
  "testcases": [
    {"input": "2 7 11 15\n9\n", "expected_output": "0 1\n",
     "is_sample": true, "score": 10}
  ]
}
```

| 字段 | 类型 | 必填 | 校验 |
|------|------|------|------|
| `title` | string | ✓ | 1–255 字符 |
| `description_md` | string | ✓ | — |
| `difficulty` | string | ✓ | `easy` \| `medium` \| `hard` |
| `time_limit_ms` | uint | ✓ | > 0 |
| `memory_limit_mb` | uint | ✓ | > 0 |
| `tags` | string[] | ✗ | 每项非空字符串 |
| `testcases` | object[] | ✓ | 1–1000 项；每项必填 `input`、`expected_output` |

**testcase 项字段**：
- `id`：可选；存在时视为**已存在用例的引用**（创建题目时一般不填）
- `input` / `expected_output`：必填
- `is_sample`：bool，默认 false
- `score`：uint，默认 0

**响应 201**
```json
{ "id": 42 }
```

**错误码**：`400`（字段缺失/类型错/范围越界/JSON 不合法）、`500`（数据库异常）

---

### 4.5 PUT /api/admin/problems/:id

全量更新题目。**当前实现语义**：替换该题的 tags 与 testcases 集合（事务内先删后插）。

**请求体**：同 §4.4。

**响应 204**（无 body）

**错误码**：`400`、`404`（id 不存在）、`500`

---

### 4.6 DELETE /api/admin/problems/:id

删除题目。级联清理 `testcases`、`problem_tags` 关联。

**响应 204**

**错误码**：`400`、`404`、`500`

---

## 5. 通用约定

### 5.1 Cookie

| 名称 | 值 | 属性 |
|------|----|------|
| `minioj_sid` | 32 字节十六进制随机串（共 64 个字符） | `HttpOnly; SameSite=Lax; Max-Age=3600[; Secure]` |

- TTL 默认 604800 秒（7 天），由 `SESSION_TTL_SECONDS` 环境变量控制
- `SESSION_COOKIE_SECURE=true` 时追加 `Secure`（仅 HTTPS 发送）

### 5.2 错误响应

所有非 2xx 响应（除 204）均返回：
```json
{ "error": "<人类可读的消息>" }
```

| 状态码 | 含义 |
|--------|------|
| `400` | 客户端请求不合法（JSON、字段、范围） |
| `401` | 未登录或 Session 失效 |
| `404` | 资源不存在 |
| `500` | 服务端内部异常（DB、判题子进程等） |

### 5.3 Content-Type

所有响应均为 `application/json; charset=utf-8`。前端无需手动设置 `Content-Type` — 但 `POST` 请求需带 `Content-Type: application/json` 以确保 cpp-httplib 正确解析 body。

---

## 6. 端到端示例

**注册 → 登录 → 提交 → 注销**

```bash
# 1) 注册
curl -sX POST http://localhost/api/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"Passw0rd!"}' \
  -c cookies.txt
# → 201 {"id":2,"username":"alice","role":"user"}

# 2) 查看自己
curl -s http://localhost/api/auth/me -b cookies.txt
# → 200 {"id":2,"username":"alice","role":"user"}

# 3) 题单
curl -s http://localhost/api/problems
# → 200 [ { "id":1, "title":"...", ... } ]

# 4) 详情
curl -s http://localhost/api/problems/1
# → 200 { "id":1, "title":"...", "description_md":"...", "sample_testcases":[...] }

# 5) 提交
curl -sX POST http://localhost/api/submissions \
  -H 'Content-Type: application/json' \
  -d '{"problem_id":1,"lang":"cpp","code":"#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b;}\n"}'
# → 200 { "verdict":"AC", "time_ms":3, "memory_mb":2, "per_case":[...] }

# 6) 注销
curl -sX POST http://localhost/api/auth/logout -b cookies.txt
# → 200 {"status":"ok"}
```