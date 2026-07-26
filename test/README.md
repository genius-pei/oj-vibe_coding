# test/

接口自动化测试目录。

```
test/
├── python/
│   └── test_minioj_api.py    # Python + requests + unittest 全量接口测试
└── README.md                  # 本文件
```

## `python/test_minioj_api.py`

基于 Python `unittest` + `requests`，**1:1 对应 `api-curl-test.md` 每个章节的"期望响应"表**：每条断言都同时校验 HTTP 状态码 **+ 响应 body 关键文案**（不是只验状态码）。

依赖（仅一个）：

```bash
pip install requests
```

### 运行

```bash
# 默认：BASE_URL=http://127.0.0.1:8081/api ADMIN_USER=admin ADMIN_PASS=AdminPass1
python3 test/python/test_minioj_api.py

# 自定义：
BASE_URL=http://localhost:8080/api \
ADMIN_USER=admin ADMIN_PASS=<seed 日志密码> \
python3 test/python/test_minioj_api.py
```

### 覆盖（与 api-curl-test.md 章节一一对应）

| doc 章节 | 测试方法 | 覆盖路由 + 关键文案 |
|----------|---------|-------|
| §1.1 GET /problems | `test_01_problem_list` | 题单字段、`difficulty=easy`、`difficulty=invalid` → 400、`tag=数组` 筛选 |
| §1.2 GET /problems/:id | `test_02_problem_detail` | description_md + sample_testcases、404 + `problem not found`、非法 id → 400 |
| §1.3 POST /submissions | `test_03_submissions` | AC/WA（含 expected/actual）/TLE/CE（含 compile_output）、**5 类错误码**（lang 非 cpp/c、code 空、code 缺、code > 256 KiB、invalid JSON）+ 404 不存在 |
| §2.1 POST /auth/register | `test_04_register` | 201 + 3 字段 + Cookie 3 属性、**8 类校验失败**（短用户名/非法字符/短密码/无字母密码）+ 完整文案、409 重名 |
| §2.2 POST /auth/login | `test_05_login` | 缺 username/password → 400、错密码 → 401、未知用户 → 401、**防枚举文案一致** |
| §2.3 POST /auth/logout | `test_06_logout` | 200 + `status:ok` + Set-Cookie `Max-Age=0`、幂等（未登录 → 200） |
| §2.4 GET /auth/me | `test_07_me` | 登录 200（不含 password_hash）、未登录 401 + `not logged in`、session 失效 401 + `session expired or invalid`、格式错 → 401 |
| §3.1-§3.5 admin CRUD | `test_08_admin_crud` | 5 端点 × 全部错误码（404/400/403/401/201/204）+ 三态鉴权 + PUT 整组替换验证 |
| §3.9 POST /admin/reset | `test_09_admin_reset` | 三态权限、200 + `problem bank reset to seed data` + `seed` 字段、题数回到 5、旧 tag 清空、users/sessions 不受影响 |

实测 ~132 条断言，**每条都同时校验 status + body 关键字段**（不是只验状态码）。

### CI 集成

```bash
# 在 docker compose up -d + seed 跑完后执行：
python3 test/python/test_minioj_api.py
# 末尾输出：
#   Ran 9 tests in 8.5s
#   OK
#   ========== 汇总 ==========
#     PASS: 132
#     FAIL: 0
#   ALL TESTS PASSED
```

`setUpClass` 在 backend 不可达时直接抛 `ConnectionRefusedError`，测试套件记为 `ERRORS=1` 退出非零，适合放进流水线。

### 与 `api-smoke.sh` 的关系

- `api-smoke.sh`（bash + curl，21 条断言）：最小依赖的轻量冒烟，可放进部署后 smoke 流程
- `test/python/test_minioj_api.py`（Python，132 条断言）：完整对应 `api-curl-test.md` 每张「期望响应」表，可用于发版前校验

两条互相独立，互不依赖，可分别选用。
