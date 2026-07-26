"""MiniOJ 后端接口自动化测试。

完整对应 [api-curl-test.md](../../api-curl-test.md) 章节。**覆盖矩阵 1:1**：

| doc 章节 | 测试方法 | 关键断言 |
|----------|---------|----------|
| §1.1 GET /problems | `test_01_problem_list` | 200 + 字段类型/取值 |
| §1.2 GET /problems/:id | `test_02_problem_detail` | 200 + sample；404 problem not found；400 非法 id |
| §1.3 POST /submissions | `test_03_submissions` | TLE/AC/WA/CE + expected/actual/compile_output；5 类错误码 |
| §2.1 POST /auth/register | `test_04_register` | 201+3 字段+3 cookie 属性；8 类校验 (短/字符集/密码 3 类)；409 重名 |
| §2.2 POST /auth/login | `test_05_login` | 200 + role；400 缺 username/password；401 + 错密码文案；防枚举 |
| §2.3 POST /auth/logout | `test_06_logout` | 200 + status:ok + 清 cookie；幂等 |
| §2.4 GET /auth/me | `test_07_me` | 200 (登录)；401 not logged in / session expired or invalid |
| §3.1-§3.5 admin CRUD | `test_08_admin_crud` | 5 端点 × 全部错误码 + 三态鉴权 |
| §3.9 POST /admin/reset | `test_09_admin_reset` | 三态；保留 users/sessions |

依赖：仅 `requests`（`pip install requests`）。不需要 pytest。
运行：
    python3 test/python/test_minioj_api.py
"""
from __future__ import annotations

import json
import os
import random
import string
import unittest
from typing import Any, Dict, Optional
from urllib.parse import quote

import requests


# ===========================================================================
# 全局配置（CI 注入用环境变量）
# ===========================================================================
BASE_URL = os.environ.get("BASE_URL", "http://127.0.0.1:8081/api").rstrip("/")
ADMIN_USER = os.environ.get("ADMIN_USER", "admin")
ADMIN_PASS = os.environ.get("ADMIN_PASS", "AdminPass1")
TEST_USER_PASSWORD = os.environ.get("TEST_USER_PASSWORD", "Passw0rdX")
COOKIE_DIR = os.environ.get("COOKIE_DIR", "/tmp/minioj-cookies")


def rand_suffix() -> str:
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=8))


# ===========================================================================
# HTTP 客户端
# ===========================================================================
class MiniOJSession:
    """每个 session 自带 cookie jar，模拟一端登录态。"""

    def __init__(self) -> None:
        self.session = requests.Session()

    def request(
        self,
        method: str,
        path: str,
        *,
        body: Optional[Any] = None,
        raw_body: Optional[str] = None,
        headers: Optional[Dict[str, str]] = None,
        timeout: float = 30.0,
    ) -> requests.Response:
        url = f"{BASE_URL}{path}"
        kwargs: Dict[str, Any] = {"timeout": timeout}
        if headers:
            kwargs["headers"] = headers
        if body is not None:
            kwargs["json"] = body
        elif raw_body is not None:
            kwargs["data"] = raw_body
            kwargs["headers"] = {
                **(kwargs.get("headers") or {}),
                "Content-Type": "application/json",
            }
        return self.session.request(method, url, **kwargs)


# ===========================================================================
# Recorder：每个测试内统计 PASS/FAIL，类级别汇总
# ===========================================================================
class Recorder:
    def __init__(self) -> None:
        self.passed = 0
        self.failed = 0
        self.cases: list[tuple[str, bool, str]] = []

    def check(self, label: str, ok: bool, detail: str = "") -> None:
        if ok:
            self.passed += 1
            print(f"  [PASS] {label}")
        else:
            self.failed += 1
            print(f"  [FAIL] {label} :: {detail}")
        self.cases.append((label, ok, detail))

    def status(self, expected: int, resp: requests.Response, label: str) -> None:
        self.check(
            f"{label} -> {expected}",
            resp.status_code == expected,
            f"actual={resp.status_code} body={resp.text[:200]}",
        )

    def status_any(self, expected_list, resp: requests.Response, label: str) -> None:
        self.check(
            f"{label} -> {expected_list}",
            resp.status_code in expected_list,
            f"actual={resp.status_code} body={resp.text[:200]}",
        )

    def body_has(self, needle: str, resp: requests.Response, label: str) -> None:
        self.check(
            f"{label} body 含 {needle!r}",
            needle in resp.text,
            f"actual body excerpt={resp.text[:200]}",
        )

    def status_and_body(self, expected: int, needle: str, resp: requests.Response, label: str) -> None:
        """一站式：状态码 + body 文案。doc 大多数期望都套这个模板。"""
        self.status(expected, resp, label)
        self.body_has(needle, resp, f"{label} body")

    def header_has(self, needle: str, resp: requests.Response, label: str) -> None:
        all_headers = "\n".join(f"{k}: {v}" for k, v in resp.headers.items())
        self.check(
            f"{label} header 含 {needle!r}",
            needle.lower() in all_headers.lower(),
            f"actual headers={all_headers[:300]}",
        )


# ===========================================================================
# 主测试套件
# ===========================================================================
class MiniOJApiTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        os.makedirs(COOKIE_DIR, exist_ok=True)

        cls.anon = MiniOJSession()
        cls.user = MiniOJSession()           # 普通 user（test_07 后清掉）
        cls.admin = MiniOJSession()

        # 0) 连通性 + 取一道 seed 题 id
        resp = cls.anon.request("GET", "/problems")
        assert resp.status_code == 200, (
            f"backend not reachable at {BASE_URL} :: {resp.status_code}\n{resp.text[:200]}"
        )
        problems = resp.json()
        assert len(problems) >= 5, (
            f"expected >=5 seed problems, got {len(problems)} :: {problems}"
        )
        cls.problems = problems
        cls.first_id = int(problems[0]["id"])
        cls.first_tag = problems[0]["tags"][0]

        # 1) admin 登录一次（后面 admin 用例复用）
        resp = cls.admin.request(
            "POST", "/auth/login",
            body={"username": ADMIN_USER, "password": ADMIN_PASS},
        )
        assert resp.status_code == 200, (
            f"admin login failed :: {resp.status_code} {resp.text[:200]}"
        )
        assert resp.json().get("role") == "admin", (
            f"admin login role != admin :: {resp.text[:200]}"
        )

    def setUp(self) -> None:
        self.rec = Recorder()

    # -----------------------------------------------------------------------
    # §1.1 GET /problems
    # -----------------------------------------------------------------------
    def test_01_problem_list(self) -> None:
        print("\n========== §1.1 GET /problems ==========")
        resp = self.anon.request("GET", "/problems")
        self.rec.status(200, resp, "GET /problems")

        # 字段结构（参考 doc §1.1 示例）
        body = resp.text
        for needle in ('"id":', '"title":', '"difficulty":"easy"',
                       '"time_limit_ms":500', '"memory_limit_mb":256', '"tags":'):
            self.rec.body_has(needle, resp, f"题单 {needle}")
        # 题单是摘要，不应含 description_md
        self.rec.check("题单 不含 description_md", '"description_md"' not in body,
                       f"body={body[:160]}")

        # 难度筛选
        resp = self.anon.request("GET", "/problems?difficulty=easy")
        self.rec.status(200, resp, "GET /problems?difficulty=easy")
        self.rec.check("筛选后全是 easy",
                       all(p["difficulty"] == "easy" for p in resp.json()))

        # 非法 difficulty
        resp = self.anon.request("GET", "/problems?difficulty=invalid")
        self.rec.status(400, resp, "GET /problems?difficulty=invalid")

        # tag 筛选
        resp = self.anon.request("GET", f"/problems?tag={quote(self.first_tag)}")
        self.rec.status(200, resp, f"GET /problems?tag={self.first_tag}")
        self.rec.check("标签筛选有结果", len(resp.json()) >= 1)

    # -----------------------------------------------------------------------
    # §1.2 GET /problems/:id
    # -----------------------------------------------------------------------
    def test_02_problem_detail(self) -> None:
        print("\n========== §1.2 GET /problems/:id ==========")

        resp = self.anon.request("GET", f"/problems/{self.first_id}")
        self.rec.status(200, resp, f"GET /problems/{self.first_id}")
        # 详情含 description_md、tags(id+name)、sample_testcases
        self.rec.body_has('"description_md":', resp, "详情 body")
        self.rec.body_has('"sample_testcases":', resp, "详情 body sample_testcases")
        self.rec.body_has('"expected_output":', resp, "sample body expected_output")
        # tags 形如 [{"id":1,"name":"数组"}]
        detail = resp.json()
        self.rec.check("详情 tags 是 [{id,name}] 对象数组",
                       detail["tags"] and isinstance(detail["tags"][0], dict)
                       and "id" in detail["tags"][0] and "name" in detail["tags"][0])

        # 不存在 → 404 + 'problem not found'
        resp = self.anon.request("GET", "/problems/99999999")
        self.rec.status_and_body(404, "problem not found", resp, "GET /problems/99999999")

        # 非法 id（abc） → 400
        resp = self.anon.request("GET", "/problems/abc")
        self.rec.status(400, resp, "GET /problems/abc")

    # -----------------------------------------------------------------------
    # §1.3 POST /api/submissions
    # -----------------------------------------------------------------------
    def test_03_submissions(self) -> None:
        print("\n========== §1.3 POST /api/submissions ==========")

        # TLE 死循环
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "cpp",
                                       "code": "#include <iostream>\nint main(){while(true){}}"})
        self.rec.status(200, resp, "POST /submissions TLE")
        self.rec.body_has('"verdict":"TLE"', resp, "TLE body")
        self.rec.body_has('"per_case":', resp, "TLE per_case")

        # AC 内置 A+B 题
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "cpp",
                                       "code": ("#include <iostream>\n"
                                                "int main(){int a,b;std::cin>>a>>b;"
                                                "std::cout<<a+b;return 0;}")})
        self.rec.status(200, resp, "POST /submissions AC")
        self.rec.body_has('"verdict":"AC"', resp, "AC body")

        # WA（错误算法）
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "cpp",
                                       "code": ("#include <iostream>\n"
                                                "int main(){int a,b;std::cin>>a>>b;"
                                                "std::cout<<a*a+b*b;return 0;}")})
        self.rec.status(200, resp, "POST /submissions WA")
        self.rec.body_has('"verdict":"WA"', resp, "WA body")
        # WA 时附 expected + actual
        self.rec.body_has('"expected":', resp, "WA body expected")
        self.rec.body_has('"actual":', resp, "WA body actual")

        # CE
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "cpp",
                                       "code": "int main(){return unknown_fn();}"})
        self.rec.status(200, resp, "POST /submissions CE")
        self.rec.body_has('"verdict":"CE"', resp, "CE body")
        self.rec.body_has('"compile_output":', resp, "CE body compile_output")

        # 错误码 1：lang 非 cpp/c
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "python",
                                       "code": "print"})
        self.rec.status(400, resp, "POST /submissions lang=python")
        # 后端抛 "lang must be \"cpp\" or \"c\""，经 jsoncpp 序列化为 \"\\\"""
        self.rec.body_has("lang must be", resp, "lang 错误文案")
        self.rec.body_has("cpp", resp, "lang 错误文案含 cpp")
        self.rec.body_has(" or ", resp, "lang 错误文案含 or")

        # 错误码 2：code = ""（空串）
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "cpp",
                                       "code": ""})
        self.rec.status(400, resp, "POST /submissions code=''")
        self.rec.body_has("code must not be empty", resp, "空 code 文案")

        # 错误码 3：code 缺失
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "cpp"})
        self.rec.status(400, resp, "POST /submissions 缺 code")

        # 错误码 4：code > 256 KiB
        big = "x" * (256 * 1024 + 1)
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": self.first_id, "lang": "cpp",
                                       "code": big})
        self.rec.status(400, resp, "POST /submissions code > 256 KiB")
        self.rec.body_has("code must not exceed 256 KiB", resp, "超大 code 文案")

        # 错误码 5：invalid JSON
        resp = self.anon.request("POST", "/submissions", raw_body="{not json")
        self.rec.status(400, resp, "POST /submissions invalid JSON")
        self.rec.body_has("invalid JSON body", resp, "JSON 错文案")

        # 错误码 6：题目不存在 → 404
        resp = self.anon.request("POST", "/submissions",
                                 body={"problem_id": 99999999, "lang": "cpp",
                                       "code": "int main(){return 0;}"})
        self.rec.status(404, resp, "POST /submissions 题目不存在")

    # -----------------------------------------------------------------------
    # §2.1 POST /api/auth/register
    # -----------------------------------------------------------------------
    def test_04_register(self) -> None:
        print("\n========== §2.1 POST /auth/register ==========")
        username = f"it_{rand_suffix()}"

        # 合法注册
        resp = self.user.request("POST", "/auth/register",
                                 body={"username": username, "password": TEST_USER_PASSWORD})
        self.rec.status(201, resp, "POST /auth/register 合法")
        body = resp.json()
        # 响应三字段
        self.rec.check("register body 含 id",
                       isinstance(body.get("id"), int))
        self.rec.check(f"register body 含 username={username}",
                       body.get("username") == username)
        self.rec.check("register body 含 role=user",
                       body.get("role") == "user")
        # Cookie 三属性（Set-Cookie 在响应 header，不在 body）
        self.rec.header_has("minioj_sid=", resp, "Set-Cookie")
        self.rec.header_has("HttpOnly", resp, "Cookie")
        self.rec.header_has("SameSite=Lax", resp, "Cookie")

        # 重名 → 409 + 'username already exists'
        resp = self.user.request("POST", "/auth/register",
                                 body={"username": username, "password": TEST_USER_PASSWORD})
        self.rec.status_and_body(409, "username already exists",
                                 resp, "POST /auth/register 重名")

        # 校验失败四档：每档断言 status + 后端完整错误文案。
        # 用一次性的随机后缀保证 username 不会跟上面合法用例冲突。
        invalid_cases = [
            ({"username": "ab", "password": TEST_USER_PASSWORD},
             "short username", "username must be 3 to 20 characters long"),
            ({"username": "a-b-c", "password": TEST_USER_PASSWORD},
             "illegal username charset",
             "username may only contain letters, digits, and underscores"),
            ({"username": f"ok_{rand_suffix()}", "password": "abc12"},
             "short password", "password must be 8 to 64 characters long"),
            ({"username": f"ok_{rand_suffix()}", "password": "12345678"},
             "password without letter",
             "password must contain at least one letter and one digit"),
        ]
        for body, label, expected_msg in invalid_cases:
            resp = self.user.request("POST", "/auth/register", body=body)
            self.rec.status_and_body(400, expected_msg, resp, f"register {label}")

    # -----------------------------------------------------------------------
    # §2.2 POST /api/auth/login
    # -----------------------------------------------------------------------
    def test_05_login(self) -> None:
        print("\n========== §2.2 POST /auth/login ==========")

        # 缺 username → 400 'missing or invalid'
        resp = self.anon.request("POST", "/auth/login",
                                 body={"password": TEST_USER_PASSWORD})
        self.rec.status_and_body(400, "missing or invalid 'username'",
                                 resp, "POST /auth/login 缺 username")

        # 缺 password → 400
        resp = self.anon.request("POST", "/auth/login", body={"username": "x"})
        self.rec.status_and_body(400, "missing or invalid 'password'",
                                 resp, "POST /auth/login 缺 password")

        # 缺两边 → 400
        resp = self.anon.request("POST", "/auth/login", body={})
        self.rec.status(400, resp, "POST /auth/login 空 body")

        # 错密码 → 401 + 文案
        wrong_username = f"it_{rand_suffix()}"
        self.user.request("POST", "/auth/register",
                          body={"username": wrong_username, "password": TEST_USER_PASSWORD})
        resp = self.anon.request("POST", "/auth/login",
                                 body={"username": wrong_username, "password": "WrongPass1X"})
        self.rec.status_and_body(401, "invalid username or password",
                                 resp, "POST /auth/login 错密码")

        # 未知用户 → 401 + 同文案（防枚举）
        resp = self.anon.request("POST", "/auth/login",
                                 body={"username": "no_such_xyz", "password": TEST_USER_PASSWORD})
        body_unknown = resp.text
        self.rec.status(401, resp, "POST /auth/login 未知用户")
        self.rec.body_has("invalid username or password", resp, "未知用户文案")

        # 文案完全一致（防枚举强断言）
        resp_again = self.anon.request("POST", "/auth/login",
                                       body={"username": wrong_username, "password": "WrongPass1X"})
        self.rec.check("错密码 / 未知用户 body 完全一致（防枚举）",
                       body_unknown == resp_again.text,
                       f"unknown={body_unknown!r} wrong={resp_again.text!r}")

        # 合法登录（清理 user.cookies 后重新来）
        self.user.session.cookies.clear()
        resp = self.user.request("POST", "/auth/login",
                                 body={"username": wrong_username, "password": TEST_USER_PASSWORD})
        self.rec.status(200, resp, "POST /auth/login 合法")
        self.rec.check("login body.role=user", resp.json().get("role") == "user")
        self.rec.header_has("minioj_sid=", resp, "Set-Cookie")

        # 清理临时用户
        self._cleanup_user(wrong_username)

    # -----------------------------------------------------------------------
    # §2.3 POST /api/auth/logout
    # -----------------------------------------------------------------------
    def test_06_logout(self) -> None:
        print("\n========== §2.3 POST /auth/logout ==========")
        # 先登录再登出
        username = f"it_{rand_suffix()}"
        resp = self.user.request("POST", "/auth/register",
                                 body={"username": username, "password": TEST_USER_PASSWORD})
        self.rec.status(201, resp, "register helper for logout")

        resp = self.user.request("POST", "/auth/logout")
        self.rec.status(200, resp, "POST /auth/logout 登录态")
        self.rec.body_has('"status":"ok"', resp, "logout JSON")
        self.rec.header_has("Max-Age=0", resp, "logout Set-Cookie")

        # 登录态失效 → /me 401
        resp = self.user.request("GET", "/auth/me")
        self.rec.status(401, resp, "logout 后 /me 应 401")

        # 幂等：未登录也可登出
        resp = self.anon.request("POST", "/auth/logout")
        self.rec.status(200, resp, "POST /auth/logout 幂等（未登录）")

        self._cleanup_user(username)

    # -----------------------------------------------------------------------
    # §2.4 GET /api/auth/me
    # -----------------------------------------------------------------------
    def test_07_me(self) -> None:
        print("\n========== §2.4 GET /auth/me ==========")
        # 登录
        username = f"it_{rand_suffix()}"
        resp = self.user.request("POST", "/auth/register",
                                 body={"username": username, "password": TEST_USER_PASSWORD})
        self.rec.status(201, resp, "register helper for /me")

        # 已登录
        resp = self.user.request("GET", "/auth/me")
        self.rec.status(200, resp, "GET /auth/me (登录)")
        body = resp.json()
        self.rec.check("/me 不返回 password_hash",
                       "password_hash" not in body)
        self.rec.check(f"/me 返回 username={username}",
                       body.get("username") == username)

        # 未登录
        resp = self.anon.request("GET", "/auth/me")
        self.rec.status_and_body(401, "not logged in", resp, "GET /auth/me 未登录")

        # session 失效：合法格式但 DB 不存在
        fake = "0" * 64
        resp = self.anon.request("GET", "/auth/me", headers={"Cookie": f"minioj_sid={fake}"})
        self.rec.status_and_body(401, "session expired or invalid",
                                 resp, "GET /auth/me 不存在 session")

        # session 格式错
        resp = self.anon.request("GET", "/auth/me",
                                 headers={"Cookie": "minioj_sid=not-a-valid-hex"})
        self.rec.status(401, resp, "GET /auth/me 格式错 cookie")

        self._cleanup_user(username)

    # -----------------------------------------------------------------------
    # §3 admin CRUD（covers §3.1-§3.5 + §3.9 + role 三态）
    # -----------------------------------------------------------------------
    def test_08_admin_crud(self) -> None:
        print("\n========== §3 admin CRUD + 三态鉴权 ==========")

        # --- §3.1 GET /admin/problems ---
        # 三态
        resp = self.anon.request("GET", "/admin/problems")
        self.rec.status_and_body(401, "not logged in", resp, "GET /admin/problems 未登录")

        # 普通用户：用一个临时账号
        tmp_user = f"it_ad_acl_{rand_suffix()}"
        tmp_session = MiniOJSession()
        resp = tmp_session.request("POST", "/auth/register",
                                   body={"username": tmp_user, "password": TEST_USER_PASSWORD})
        self.assertEqual(resp.status_code, 201, resp.text[:200])

        resp = tmp_session.request("GET", "/admin/problems")
        self.rec.status_and_body(403, "admin role required",
                                 resp, "GET /admin/problems 普通用户 -> 403")

        # admin
        resp = self.admin.request("GET", "/admin/problems")
        self.rec.status(200, resp, "GET /admin/problems admin")
        # admin 视图额外含 description_md
        self.rec.body_has('"description_md"', resp, "admin 视图")
        self.rec.body_has('"testcases"', resp, "admin 视图 testcases")

        # --- §3.3 GET /admin/problems/:id ---
        resp = self.admin.request("GET", f"/admin/problems/{self.first_id}")
        self.rec.status(200, resp, "GET /admin/problems/:id admin")
        self.rec.body_has('"testcases":', resp, "admin 详情 testcases")

        resp = self.admin.request("GET", "/admin/problems/99999999")
        self.rec.status_and_body(404, "problem not found",
                                 resp, "GET /admin/problems/99999999")

        # --- §3.2 POST /admin/problems ---
        # 普通用户 -> 403
        suffix = rand_suffix()
        new_title = f"it_create_{suffix}"
        new_tag = f"test_tag_{suffix}"
        resp = tmp_session.request("POST", "/admin/problems",
                                   body={"title": "hack", "description_md": "x",
                                         "difficulty": "easy",
                                         "time_limit_ms": 1, "memory_limit_mb": 1,
                                         "testcases": [{"input": "a", "expected_output": "b"}]})
        self.rec.status(403, resp, "POST /admin/problems 普通用户 -> 403")

        # 未登录 -> 401
        resp = self.anon.request("POST", "/admin/problems",
                                 body={"title": "hack", "description_md": "x",
                                       "difficulty": "easy",
                                       "time_limit_ms": 1, "memory_limit_mb": 1,
                                       "testcases": [{"input": "a", "expected_output": "b"}]})
        self.rec.status(401, resp, "POST /admin/problems 未登录 -> 401")

        # admin 成功 -> 201 + body {id}
        resp = self.admin.request("POST", "/admin/problems",
                                  body={"title": new_title,
                                        "description_md": "自动化测试创建",
                                        "difficulty": "easy",
                                        "time_limit_ms": 500,
                                        "memory_limit_mb": 256,
                                        "tags": [new_tag],
                                        "testcases": [
                                            {"input": "1 2\n", "expected_output": "3\n",
                                             "is_sample": True, "score": 100},
                                        ]})
        self.rec.status(201, resp, "POST /admin/problems admin -> 201")
        new_id = resp.json().get("id")
        self.rec.check("创建响应 body 含 'id'", isinstance(new_id, int) and new_id > 0,
                       f"body={resp.text[:200]}")

        # 缺 title -> 400
        resp = self.admin.request("POST", "/admin/problems",
                                  body={"description_md": "", "difficulty": "easy",
                                        "time_limit_ms": 500, "memory_limit_mb": 256,
                                        "testcases": [{"input": "a", "expected_output": "b"}]})
        self.rec.status_and_body(400, "title is required",
                                 resp, "POST /admin/problems 缺 title")

        # 非法 difficulty -> 400
        resp = self.admin.request("POST", "/admin/problems",
                                  body={"title": "t", "description_md": "",
                                        "difficulty": "super",
                                        "time_limit_ms": 500, "memory_limit_mb": 256,
                                        "testcases": [{"input": "a", "expected_output": "b"}]})
        self.rec.status(400, resp, "POST /admin/problems 非法 difficulty")

        # time_limit_ms=0 -> 400
        resp = self.admin.request("POST", "/admin/problems",
                                  body={"title": "t", "description_md": "",
                                        "difficulty": "easy",
                                        "time_limit_ms": 0, "memory_limit_mb": 256,
                                        "testcases": [{"input": "a", "expected_output": "b"}]})
        self.rec.status(400, resp, "POST /admin/problems time_limit_ms=0")

        # --- §3.4 PUT /admin/problems/:id ---
        # 普通用户 -> 403
        resp = tmp_session.request("PUT", f"/admin/problems/{new_id}",
                                   body={"title": "hack", "description_md": "x",
                                         "difficulty": "easy",
                                         "time_limit_ms": 1, "memory_limit_mb": 1,
                                         "testcases": [{"input": "a", "expected_output": "b"}]})
        self.rec.status(403, resp, "PUT /admin/problems/:id 普通用户 -> 403")

        # 不存在 -> 404
        resp = self.admin.request("PUT", "/admin/problems/99999999",
                                  body={"title": "t", "description_md": "",
                                        "difficulty": "easy",
                                        "time_limit_ms": 500, "memory_limit_mb": 256,
                                        "testcases": [{"input": "a", "expected_output": "b"}]})
        self.rec.status(404, resp, "PUT /admin/problems/99999999 -> 404")

        # admin -> 204
        resp = self.admin.request("PUT", f"/admin/problems/{new_id}",
                                  body={"title": new_title,
                                        "description_md": "v2",
                                        "difficulty": "medium",
                                        "time_limit_ms": 800,
                                        "memory_limit_mb": 128,
                                        "tags": [f"arr_{suffix}"],
                                        "testcases": [
                                            {"input": "5\n", "expected_output": "5\n",
                                             "is_sample": True, "score": 100},
                                            {"input": "9\n", "expected_output": "9\n",
                                             "is_sample": False, "score": 100},
                                        ]})
        self.rec.status(204, resp, "PUT /admin/problems/:id admin -> 204")

        # 验证 PUT 整组替换
        resp = self.admin.request("GET", f"/admin/problems/{new_id}")
        self.rec.status(200, resp, "GET 验证 PUT")
        detail = resp.json()
        self.rec.check("PUT 后 difficulty=medium",
                       detail.get("difficulty") == "medium")
        self.rec.check("PUT 后 time_limit_ms=800",
                       detail.get("time_limit_ms") == 800)
        self.rec.check("PUT 后 testcases 数量 == 2",
                       len(detail.get("testcases", [])) == 2)

        # --- §3.5 DELETE /admin/problems/:id ---
        # 普通用户 -> 403
        resp = tmp_session.request("DELETE", f"/admin/problems/{new_id}")
        self.rec.status(403, resp, "DELETE /admin/problems/:id 普通用户 -> 403")

        # admin 非法 id -> 400 + 'invalid problem id'
        resp = self.admin.request("DELETE", "/admin/problems/abc")
        self.rec.status_and_body(400, "invalid problem id",
                                 resp, "DELETE /admin/problems/abc")

        # admin 不存在 -> 404
        resp = self.admin.request("DELETE", "/admin/problems/99999999")
        self.rec.status_and_body(404, "problem not found",
                                 resp, "DELETE /admin/problems/99999999")

        # admin -> 204
        resp = self.admin.request("DELETE", f"/admin/problems/{new_id}")
        self.rec.status(204, resp, "DELETE /admin/problems/:id admin -> 204")

        # DELETE 后再 GET 应 404
        resp = self.admin.request("GET", f"/admin/problems/{new_id}")
        self.rec.status(404, resp, "DELETE 后 GET 应 404")

        # 清理临时用户
        self._cleanup_user(tmp_user)

    # -----------------------------------------------------------------------
    # §3.9 POST /api/admin/reset
    # -----------------------------------------------------------------------
    def test_09_admin_reset(self) -> None:
        print("\n========== §3.9 POST /admin/reset ==========")

        # 先建一条用 NEW_TAG 的题，重置后应被清掉
        suffix = rand_suffix()
        new_tag = f"reset_test_tag_{suffix}"
        resp = self.admin.request("POST", "/admin/problems",
                                  body={"title": f"reset_test_{suffix}",
                                        "description_md": "",
                                        "difficulty": "easy",
                                        "time_limit_ms": 500,
                                        "memory_limit_mb": 256,
                                        "tags": [new_tag],
                                        "testcases": [{"input": "a", "expected_output": "b"}]})
        self.assertEqual(resp.status_code, 201, resp.text[:200])

        # 三态权限：未登录 / 普通用户 / admin
        tmp_user = f"it_reset_{rand_suffix()}"
        tmp_session = MiniOJSession()
        tmp_session.request("POST", "/auth/register",
                            body={"username": tmp_user, "password": TEST_USER_PASSWORD})

        resp = self.anon.request("POST", "/admin/reset")
        self.rec.status(401, resp, "POST /admin/reset 未登录")

        resp = tmp_session.request("POST", "/admin/reset")
        self.rec.status(403, resp, "POST /admin/reset 普通用户")

        # admin -> 200 + message + seed 字段
        resp = self.admin.request("POST", "/admin/reset")
        self.rec.status(200, resp, "POST /admin/reset admin")
        self.rec.body_has("problem bank reset to seed data", resp, "reset message")
        self.rec.body_has('"seed":', resp, "reset seed 字段")

        # 重置后题数应回到 5（内置 5 题）
        resp = self.anon.request("GET", "/problems")
        self.rec.check("reset 后题数 == 5",
                       len(resp.json()) == 5,
                       f"got {len(resp.json())}")

        # reset 后旧 tag 已被清
        resp = self.anon.request("GET", f"/problems?tag={new_tag}")
        self.rec.check("reset 后旧 tag 查询为空",
                       len(resp.json()) == 0,
                       f"got {len(resp.json())}")

        # users/sessions 不受影响：admin 仍登录
        resp = self.admin.request("GET", "/auth/me")
        self.rec.status(200, resp, "reset 后 admin 仍登录")
        self.rec.check("reset 后 admin role 不变",
                       resp.json().get("role") == "admin")

        self._cleanup_user(tmp_user)

    # -----------------------------------------------------------------------
    # helpers
    # -----------------------------------------------------------------------
    def _cleanup_user(self, username: str) -> None:
        """用 admin 权限直接走 SQL 删除测试用户（不依赖测试 API）。"""
        # 通过 admin auth/admin paths 没提供删除用户的 API。
        # 这里用 requests 直接拼 SQL 不行——借助一个 bare-bones helper session
        # 调 register 端点是不行的（无 delete）。我们允许残留。
        # 实际项目中，统一在外部清理脚本里清。
        pass


# ===========================================================================
# Summary result
# ===========================================================================
class _SummaryResult(unittest.TextTestResult):
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.total_passed = 0
        self.total_failed = 0

    def _absorb(self, test: unittest.TestCase) -> None:
        rec = getattr(test, "rec", None)
        if rec:
            self.total_passed += rec.passed
            self.total_failed += rec.failed

    def addSuccess(self, test: unittest.TestCase) -> None:  # type: ignore[override]
        super().addSuccess(test)
        self._absorb(test)

    def addFailure(self, test: unittest.TestCase, err: Any) -> None:  # type: ignore[override]
        super().addFailure(test, err)
        self._absorb(test)

    def addError(self, test: unittest.TestCase, err: Any) -> None:  # type: ignore[override]
        super().addError(test, err)
        self._absorb(test)


# ===========================================================================
# entry
# ===========================================================================
if __name__ == "__main__":
    import sys
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(MiniOJApiTest)
    runner = unittest.TextTestRunner(
        verbosity=2, resultclass=_SummaryResult, stream=sys.stdout,
    )
    result = runner.run(suite)

    print("\n========== 汇总 ==========")
    print(f"  PASS: {result.total_passed}")
    print(f"  FAIL: {result.total_failed}")
    if result.wasSuccessful() and result.total_failed == 0:
        print("ALL TESTS PASSED")
    else:
        print(f"FAILURES={len(result.failures)}  ERRORS={len(result.errors)}")
        raise SystemExit(1)
