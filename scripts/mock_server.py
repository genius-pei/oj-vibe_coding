#!/usr/bin/env python3
"""MiniOJ 本地开发服务器：静态文件 + mock API（v1.1 含 admin）。
用于无 Docker / 无后端场景下的前端联调，所有写操作只在内存中。
"""
import http.server
import json
import os
import re
import sys
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "frontend" / "public"
SEED = Path(__file__).resolve().parent.parent / "backend" / "seed" / "problems.json"

PORT = int(os.environ.get("PORT", "8080"))
BIND = os.environ.get("BIND", "0.0.0.0")

# ── 加载种子题库到内存 ──────────────────────────
try:
    with open(SEED, encoding="utf-8") as f:
        SEED_PROBLEMS = json.load(f)
except Exception as e:
    print(f"[warn] failed to load seed: {e}", file=sys.stderr)
    SEED_PROBLEMS = []


def _seed_to_problems():
    """从 SEED_PROBLEMS 重新构造内存 problems 列表（reset / 启动共用）。"""
    out = []
    for i, p in enumerate(SEED_PROBLEMS, start=1):
        out.append({
            "id": i,
            "title": p["title"],
            "description_md": p.get("description_md", ""),
            "difficulty": p["difficulty"],
            "time_limit_ms": p.get("time_limit_ms", 1000),
            "memory_limit_mb": p.get("memory_limit_mb", 256),
            "tags": list(p.get("tags", [])),
            "testcases": [
                {
                    "input": tc.get("input", ""),
                    "expected_output": tc.get("expected_output", ""),
                    "is_sample": bool(tc.get("is_sample", False)),
                    "score": int(tc.get("score", 0)),
                }
                for tc in p.get("testcases", [])
            ],
        })
    return out


problems = _seed_to_problems()

USERS = {"admin": {"username": "admin", "role": "admin", "password": "admin123"}}
SESSIONS = {}  # sid -> username


# ── DTO 转换 ──────────────────────────────────────
def to_admin_detail(p):
    """AdminProblemDetail 格式：tags 为 [{id, name}]，testcases 含 id。"""
    return {
        "id": p["id"],
        "title": p["title"],
        "description_md": p.get("description_md", ""),
        "difficulty": p["difficulty"],
        "time_limit_ms": p["time_limit_ms"],
        "memory_limit_mb": p["memory_limit_mb"],
        "tags": [{"id": idx, "name": t} for idx, t in enumerate(p.get("tags", []))],
        "testcases": [
            {
                "id": idx + 1,
                "input": tc["input"],
                "expected_output": tc["expected_output"],
                "is_sample": tc["is_sample"],
                "score": tc["score"],
            }
            for idx, tc in enumerate(p.get("testcases", []))
        ],
    }


def to_public_summary(p):
    """ProblemSummary 格式（公开 list 用）：tags 为字符串数组。"""
    return {
        "id": p["id"],
        "title": p["title"],
        "difficulty": p["difficulty"],
        "time_limit_ms": p["time_limit_ms"],
        "memory_limit_mb": p["memory_limit_mb"],
        "tags": list(p.get("tags", [])),
    }


def to_public_detail(p):
    """公开题单详情（不含完整 testcases，只含 sample）。"""
    return {
        "id": p["id"],
        "title": p["title"],
        "description_md": p.get("description_md", ""),
        "difficulty": p["difficulty"],
        "time_limit_ms": p["time_limit_ms"],
        "memory_limit_mb": p["memory_limit_mb"],
        "tags": [{"id": idx, "name": t} for idx, t in enumerate(p.get("tags", []))],
        "sample_testcases": [
            {"id": idx, "input": tc["input"], "expected_output": tc["expected_output"]}
            for idx, tc in enumerate(p["testcases"]) if tc["is_sample"]
        ],
    }


# ── 公开 list ──────────────────────────────────────
def filter_problems(params):
    diff = params.get("difficulty", [""])[0]
    tag = params.get("tag", [""])[0]
    out = problems
    if diff:
        out = [p for p in out if p["difficulty"] == diff]
    if tag:
        out = [p for p in out if tag in p["tags"]]
    return [to_public_summary(p) for p in out]


# ── 输入校验（POST / PUT 复用） ───────────────────
def validate_problem_input(data):
    """返回 (ok, error_message)。"""
    if not isinstance(data, dict):
        return False, "body must be a JSON object"
    title = data.get("title")
    if not isinstance(title, str) or not title.strip():
        return False, "title is required"
    if len(title) > 255:
        return False, "title must not exceed 255 characters"
    desc = data.get("description_md")
    if not isinstance(desc, str):
        return False, "description_md is required"
    if data.get("difficulty") not in ("easy", "medium", "hard"):
        return False, "difficulty must be one of easy|medium|hard"
    tl = data.get("time_limit_ms")
    if not isinstance(tl, int) or tl <= 0:
        return False, "time_limit_ms must be a positive integer"
    ml = data.get("memory_limit_mb")
    if not isinstance(ml, int) or ml <= 0:
        return False, "memory_limit_mb must be a positive integer"
    tags = data.get("tags", [])
    if not isinstance(tags, list):
        return False, "tags must be an array"
    for t in tags:
        if not isinstance(t, str) or not t.strip():
            return False, "tag entries must be non-empty strings"
    cases = data.get("testcases")
    if not isinstance(cases, list) or not cases:
        return False, "testcases must contain at least one entry"
    if len(cases) > 1000:
        return False, "testcases must not exceed 1000 entries"
    for i, tc in enumerate(cases):
        if not isinstance(tc, dict):
            return False, f"testcase #{i + 1} must be a JSON object"
        if not isinstance(tc.get("input"), str):
            return False, f"testcase #{i + 1} input is required"
        if not isinstance(tc.get("expected_output"), str):
            return False, f"testcase #{i + 1} expected_output is required"
        if "is_sample" in tc and not isinstance(tc["is_sample"], bool):
            return False, f"testcase #{i + 1} is_sample must be a boolean"
        if "score" in tc and not isinstance(tc["score"], int):
            return False, f"testcase #{i + 1} score must be an integer"
    return True, ""


def problem_from_input(data, new_id):
    return {
        "id": new_id,
        "title": data["title"].strip(),
        "description_md": data["description_md"],
        "difficulty": data["difficulty"],
        "time_limit_ms": data["time_limit_ms"],
        "memory_limit_mb": data["memory_limit_mb"],
        "tags": [t.strip() for t in data["tags"] if t and t.strip()],
        "testcases": [
            {
                "input": tc["input"],
                "expected_output": tc["expected_output"],
                "is_sample": bool(tc.get("is_sample", False)),
                "score": int(tc.get("score", 0)),
            }
            for tc in data["testcases"]
        ],
    }


def parse_json_body(raw):
    if not raw:
        return None, "request body is empty"
    try:
        return json.loads(raw.decode("utf-8")), ""
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        return None, f"invalid JSON body: {e}"


# ── Handler ──────────────────────────────────────
class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def log_message(self, fmt, *args):
        sys.stderr.write("[mock] " + (fmt % args) + "\n")

    def _send_json(self, status, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def _send_empty(self, status):
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", "0")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()

    def _cookie_sid(self):
        cookie = self.headers.get("Cookie", "")
        for part in cookie.split(";"):
            k, _, v = part.strip().partition("=")
            if k == "minioj_sid":
                return v
        return None

    def _require_admin(self):
        sid = self._cookie_sid()
        user = SESSIONS.get(sid)
        if not user:
            self._send_json(401, {"error": "not logged in"})
            return False
        if USERS.get(user, {}).get("role") != "admin":
            self._send_json(403, {"error": "admin only"})
            return False
        return True

    def _read_body(self):
        length = int(self.headers.get("Content-Length", "0"))
        return self.rfile.read(length) if length else b"{}"

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        # /api/admin/problems/:id
        m = re.match(r"^/api/admin/problems/(\d+)$", path)
        if m:
            if not self._require_admin():
                return
            pid = int(m.group(1))
            p = next((x for x in problems if x["id"] == pid), None)
            if not p:
                return self._send_json(404, {"error": "problem not found"})
            return self._send_json(200, to_admin_detail(p))

        # /api/admin/problems
        if path == "/api/admin/problems":
            if not self._require_admin():
                return
            return self._send_json(200, [to_admin_detail(p) for p in problems])

        # /api/problems/:id
        m = re.match(r"^/api/problems/(\d+)$", path)
        if m:
            pid = int(m.group(1))
            p = next((x for x in problems if x["id"] == pid), None)
            if not p:
                return self._send_json(404, {"error": "problem not found"})
            return self._send_json(200, to_public_detail(p))

        # /api/problems
        if path == "/api/problems":
            params = urllib.parse.parse_qs(parsed.query)
            return self._send_json(200, filter_problems(params))

        # /api/auth/me
        if path == "/api/auth/me":
            sid = self._cookie_sid()
            user = SESSIONS.get(sid)
            if not user:
                return self._send_json(401, {"error": "not logged in"})
            u = USERS[user]
            return self._send_json(200, {
                "id": 1, "username": user, "role": u["role"]
            })

        # 静态文件兜底
        return super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        raw = self._read_body()

        # /api/admin/problems
        if path == "/api/admin/problems":
            if not self._require_admin():
                return
            data, err = parse_json_body(raw)
            if data is None:
                return self._send_json(400, {"error": err})
            ok, msg = validate_problem_input(data)
            if not ok:
                return self._send_json(400, {"error": msg})
            new_id = max([p["id"] for p in problems], default=0) + 1
            problems.append(problem_from_input(data, new_id))
            return self._send_json(201, {"id": new_id, "message": "problem created"})

        # /api/admin/reset
        if path == "/api/admin/reset":
            if not self._require_admin():
                return
            problems.clear()
            problems.extend(_seed_to_problems())
            return self._send_json(200, {
                "message": "problem bank reset to seed data",
                "seed": str(SEED),
                "count": len(problems),
            })

        # /api/auth/register
        if path == "/api/auth/register":
            data, err = parse_json_body(raw)
            if data is None:
                return self._send_json(400, {"error": err})
            username = (data.get("username") or "").strip()
            password = data.get("password") or ""
            if not re.match(r"^[A-Za-z0-9_]{3,20}$", username):
                return self._send_json(400, {"error": "invalid username"})
            if (len(password) < 8 or len(password) > 64
                    or not re.search(r"[A-Za-z]", password)
                    or not re.search(r"\d", password)):
                return self._send_json(400, {"error": "invalid password"})
            if username in USERS:
                return self._send_json(409, {"error": "username exists"})
            USERS[username] = {"username": username, "role": "user", "password": password}
            sid = f"sid-{len(SESSIONS) + 1}"
            SESSIONS[sid] = username
            self.send_response(201)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Set-Cookie", f"minioj_sid={sid}; Path=/; HttpOnly")
            self.send_header("Access-Control-Allow-Origin", "*")
            body = json.dumps(
                {"id": 1, "username": username, "role": "user"},
                ensure_ascii=False,
            ).encode("utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        # /api/auth/login
        if path == "/api/auth/login":
            data, err = parse_json_body(raw)
            if data is None:
                return self._send_json(400, {"error": err})
            username = (data.get("username") or "").strip()
            password = data.get("password") or ""
            user = USERS.get(username)
            if not user or user["password"] != password:
                return self._send_json(401, {"error": "用户名或密码错误"})
            sid = f"sid-{len(SESSIONS) + 1}"
            SESSIONS[sid] = username
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Set-Cookie", f"minioj_sid={sid}; Path=/; HttpOnly")
            self.send_header("Access-Control-Allow-Origin", "*")
            body = json.dumps(
                {"id": 1, "username": username, "role": user["role"]},
                ensure_ascii=False,
            ).encode("utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        # /api/auth/logout
        if path == "/api/auth/logout":
            sid = self._cookie_sid()
            if sid:
                SESSIONS.pop(sid, None)
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Set-Cookie", "minioj_sid=; Path=/; Max-Age=0; HttpOnly")
            self.send_header("Access-Control-Allow-Origin", "*")
            body = json.dumps({"status": "ok"}, ensure_ascii=False).encode("utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        # /api/submissions（mock：固定 4 AC + 1 WA，让前端能渲染完整 case list）
        # 真实后端会基于 testcases 跑评测；mock 简化但保证 per_case 数组非空。
        if path == "/api/submissions":
            per_case = [
                {"index": i + 1, "verdict": "AC", "time_ms": 2 + i, "memory_mb": 2 + (i % 2)}
                for i in range(4)
            ]
            per_case.append({
                "index": 5, "verdict": "WA", "time_ms": 6, "memory_mb": 3,
                "expected": "3\n", "actual": "5\n"
            })
            return self._send_json(200, {
                "verdict": "WA",
                "time_ms": 12,
                "memory_mb": 3,
                "compile_output": "",
                "per_case": per_case,
            })

        return self._send_json(404, {"error": "not found"})

    def do_PUT(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        m = re.match(r"^/api/admin/problems/(\d+)$", path)
        if m:
            if not self._require_admin():
                return
            pid = int(m.group(1))
            raw = self._read_body()
            data, err = parse_json_body(raw)
            if data is None:
                return self._send_json(400, {"error": err})
            ok, msg = validate_problem_input(data)
            if not ok:
                return self._send_json(400, {"error": msg})
            idx = next((i for i, x in enumerate(problems) if x["id"] == pid), None)
            if idx is None:
                return self._send_json(404, {"error": "problem not found"})
            # 保留 id，整组替换
            new_p = problem_from_input(data, pid)
            problems[idx] = new_p
            return self._send_empty(204)

        return self._send_json(404, {"error": "not found"})

    def do_DELETE(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        m = re.match(r"^/api/admin/problems/(\d+)$", path)
        if m:
            if not self._require_admin():
                return
            pid = int(m.group(1))
            idx = next((i for i, x in enumerate(problems) if x["id"] == pid), None)
            if idx is None:
                return self._send_json(404, {"error": "problem not found"})
            problems.pop(idx)
            return self._send_empty(204)

        return self._send_json(404, {"error": "not found"})


if __name__ == "__main__":
    print(f"[mock] serving {ROOT} on http://{BIND}:{PORT}")
    print(f"[mock] loaded {len(problems)} seed problems")
    print(f"[mock] users: {list(USERS.keys())} (admin/admin123, others register)")
    print(f"[mock] admin API: /api/admin/{{problems,problems/:id,reset}}")
    http.server.ThreadingHTTPServer((BIND, PORT), Handler).serve_forever()