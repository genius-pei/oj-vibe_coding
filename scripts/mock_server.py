#!/usr/bin/env python3
"""MiniOJ 本地开发服务器：静态文件 + mock API。
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

problems = []  # [{id, title, difficulty, time_limit_ms, memory_limit_mb, tags, description_md, samples}]
for i, p in enumerate(SEED_PROBLEMS, start=1):
    samples = [t for t in p.get("testcases", []) if t.get("is_sample")]
    problems.append({
        "id": i,
        "title": p["title"],
        "difficulty": p["difficulty"],
        "time_limit_ms": p.get("time_limit_ms", 1000),
        "memory_limit_mb": p.get("memory_limit_mb", 256),
        "tags": p.get("tags", []),
        "description_md": p.get("description_md", ""),
        "samples": samples,
    })

USERS = {"admin": {"username": "admin", "role": "admin", "password": "admin"}}
SESSIONS = {}  # sid -> username


def filter_problems(params):
    diff = params.get("difficulty", [""])[0]
    tag = params.get("tag", [""])[0]
    out = problems
    if diff:
        out = [p for p in out if p["difficulty"] == diff]
    if tag:
        out = [p for p in out if tag in p["tags"]]
    return [
        {
            "id": p["id"],
            "title": p["title"],
            "difficulty": p["difficulty"],
            "time_limit_ms": p["time_limit_ms"],
            "memory_limit_mb": p["memory_limit_mb"],
            "tags": p["tags"],
        }
        for p in out
    ]


def detail_problem(pid):
    for p in problems:
        if p["id"] == pid:
            return {
                "id": p["id"],
                "title": p["title"],
                "description_md": p["description_md"],
                "difficulty": p["difficulty"],
                "time_limit_ms": p["time_limit_ms"],
                "memory_limit_mb": p["memory_limit_mb"],
                "tags": [{"id": idx, "name": t} for idx, t in enumerate(p["tags"])],
                "sample_testcases": [
                    {"id": idx, "input": s["input"], "expected_output": s["expected_output"]}
                    for idx, s in enumerate(p["samples"])
                ],
            }
    return None


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

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        # /api/problems/:id
        m = re.match(r"^/api/problems/(\d+)$", path)
        if m:
            pid = int(m.group(1))
            detail = detail_problem(pid)
            if detail is None:
                return self._send_json(404, {"error": "problem not found"})
            return self._send_json(200, detail)

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
            return self._send_json(200, {"username": user, "role": USERS[user]["role"], "id": 1})

        # 静态文件兜底
        return super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length else b"{}"
        try:
            body = json.loads(raw or b"{}")
        except json.JSONDecodeError:
            body = {}

        # /api/auth/register
        if path == "/api/auth/register":
            username = (body.get("username") or "").strip()
            password = body.get("password") or ""
            if not re.match(r"^[A-Za-z0-9_]{3,20}$", username):
                return self._send_json(400, {"error": "invalid username"})
            if len(password) < 8 or len(password) > 64 or not re.search(r"[A-Za-z]", password) or not re.search(r"\d", password):
                return self._send_json(400, {"error": "invalid password"})
            if username in USERS:
                return self._send_json(409, {"error": "username exists"})
            USERS[username] = {"username": username, "role": "user", "password": password}
            sid = f"sid-{len(SESSIONS)+1}"
            SESSIONS[sid] = username
            self.send_response(201)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Set-Cookie", f"minioj_sid={sid}; Path=/; HttpOnly")
            body = json.dumps({"id": 1, "username": username, "role": "user"}, ensure_ascii=False).encode("utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        # /api/auth/login
        if path == "/api/auth/login":
            username = (body.get("username") or "").strip()
            password = body.get("password") or ""
            user = USERS.get(username)
            if not user or user["password"] != password:
                return self._send_json(401, {"error": "用户名或密码错误"})
            sid = f"sid-{len(SESSIONS)+1}"
            SESSIONS[sid] = username
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Set-Cookie", f"minioj_sid={sid}; Path=/; HttpOnly")
            body = json.dumps({"id": 1, "username": username, "role": user["role"]}, ensure_ascii=False).encode("utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        # /api/auth/logout
        if path == "/api/auth/logout":
            return self._send_json(200, {"status": "ok"})

        # /api/submissions（mock：永远 AC）
        if path == "/api/submissions":
            return self._send_json(200, {
                "verdict": "AC",
                "time_ms": 5,
                "memory_mb": 3,
                "compile_output": "",
                "per_case": [],
            })

        return self._send_json(404, {"error": "not found"})

    def do_DELETE(self):
        return self._send_json(200, {"status": "ok"})

    def _cookie_sid(self):
        cookie = self.headers.get("Cookie", "")
        for part in cookie.split(";"):
            k, _, v = part.strip().partition("=")
            if k == "minioj_sid":
                return v
        return None


if __name__ == "__main__":
    print(f"[mock] serving {ROOT} on http://{BIND}:{PORT}")
    print(f"[mock] loaded {len(problems)} seed problems")
    print(f"[mock] users: {list(USERS.keys())} (password = username)")
    http.server.ThreadingHTTPServer((BIND, PORT), Handler).serve_forever()