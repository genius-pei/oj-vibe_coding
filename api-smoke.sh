#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:8080/api}"
COOKIE_USER="/tmp/minioj-cookies/user.txt"
COOKIE_ADMIN="/tmp/minioj-cookies/admin.txt"
mkdir -p "$(dirname "$COOKIE_USER")"
rm -f "$COOKIE_USER" "$COOKIE_ADMIN"

red()    { printf '\033[31m%s\033[0m\n' "$*"; }
green()  { printf '\033[32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[33m%s\033[0m\n' "$*"; }
hr()     { printf '\n=== %s ===\n' "$*"; }

assert_status() {
    local expected="$1" actual="$2" desc="$3"
    if [[ "$actual" == "$expected" ]]; then
        green "  [OK] $desc -> $actual"
    else
        red   "  [FAIL] $desc -> expected=$expected actual=$actual"
        exit 1
    fi
}

assert_body_has() {
    local needle="$1" body="$2" desc="$3"
    if [[ "$body" == *"$needle"* ]]; then
        green "  [OK] $desc"
    else
        red   "  [FAIL] $desc -> body did not contain '$needle'; body=$body"
        exit 1
    fi
}

hr "1. 公开题单"
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/problems")
assert_status 200 "$code" "GET /problems"

hr "2. 题目详情 (存在 / 不存在)"
# 动态取一道内置题 id（seed 数据可能 id>1）
FIRST_PROBLEM_ID=$(curl -sS "$BASE_URL/problems" | grep -oE '"id":[0-9]+' | head -1 | grep -oE '[0-9]+')
if [[ -z "$FIRST_PROBLEM_ID" ]]; then
    red "  [FAIL] 题单为空，无法继续测试"
    exit 1
fi
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/problems/$FIRST_PROBLEM_ID")
assert_status 200 "$code" "GET /problems/$FIRST_PROBLEM_ID"
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/problems/999999")
assert_status 404 "$code" "GET /problems/999999"

hr "3. 注册 / 登录 / 登出"
suffix="$RANDOM"
USERNAME="alice_${suffix}"
PASSWORD="Passw0rd!"
code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/register" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")
assert_status 201 "$code" "POST /auth/register (new)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/register" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")
assert_status 409 "$code" "POST /auth/register (duplicate)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/register" \
    -H "Content-Type: application/json" \
    -d '{"username":"ab","password":"Passw0rd!"}')
assert_status 400 "$code" "POST /auth/register (short username)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/login" \
    -H "Content-Type: application/json" \
    -c "$COOKIE_USER" \
    -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")
assert_status 200 "$code" "POST /auth/login"

code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USERNAME\",\"password\":\"WRONGpass1\"}")
assert_status 401 "$code" "POST /auth/login (wrong password)"

code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/auth/me" -b "$COOKIE_USER")
assert_status 200 "$code" "GET /auth/me (logged in)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/logout" -b "$COOKIE_USER")
assert_status 200 "$code" "POST /auth/logout"

code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/auth/me" -b "$COOKIE_USER")
assert_status 401 "$code" "GET /auth/me (after logout)"

# 重新登录一次拿到 cookie 供后续 admin 测试
code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/login" \
    -H "Content-Type: application/json" \
    -c "$COOKIE_USER" \
    -d "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD\"}")
assert_status 200 "$code" "POST /auth/login (re-login)"

hr "4. 提交代码"
code=$(curl -sS -o /tmp/sub.out -w '%{http_code}' -X POST "$BASE_URL/submissions" \
    -H "Content-Type: application/json" \
    -d "{\"problem_id\":$FIRST_PROBLEM_ID,\"lang\":\"cpp\",\"code\":\"#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b;return 0;}\"}")
assert_status 200 "$code" "POST /submissions"
yellow "  verdict=$(jq -r .verdict /tmp/sub.out 2>/dev/null || cat /tmp/sub.out)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/submissions" \
    -H "Content-Type: application/json" \
    -d '{"problem_id":1,"lang":"python","code":"print(1)"}')
assert_status 400 "$code" "POST /submissions (bad lang)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/submissions" \
    -H "Content-Type: application/json" \
    -d '{"problem_id":999999,"lang":"cpp","code":"int main(){return 0;}"}')
assert_status 404 "$code" "POST /submissions (no such problem)"

hr "5. admin 鉴权（role 中间件三态）"
# 普通用户访问 /admin/problems
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/admin/problems" -b "$COOKIE_USER")
assert_status 403 "$code" "GET /admin/problems (普通用户 -> 403)"

# 未登录访问
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/admin/problems")
assert_status 401 "$code" "GET /admin/problems (未登录 -> 401)"

# 无效 Cookie
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/admin/problems" \
    -H "Cookie: minioj_sid=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
assert_status 401 "$code" "GET /admin/problems (无效 session -> 401)"

# 普通用户 POST 创建题目
code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/admin/problems" \
    -H "Content-Type: application/json" \
    -b "$COOKIE_USER" \
    -d '{"title":"hack","description_md":"x","difficulty":"easy","time_limit_ms":1,"memory_limit_mb":1,"testcases":[{"input":"a","expected_output":"b"}]}')
assert_status 403 "$code" "POST /admin/problems (普通用户 -> 403)"

# 尝试 admin 登录——admin 必须存在才能继续
# 这一步假定 ./backend/build/minioj-seed --reset 已执行（或 minioj-reset-for-tests 跑过），
# admin 密码由 seed 进程输出到 stdout；若未捕获，本测试段会跳过。
if [[ -z "${ADMIN_USERNAME:-}" || -z "${ADMIN_PASSWORD:-}" ]]; then
    yellow "[WARN] ADMIN_USERNAME/ADMIN_PASSWORD 未设置，跳过 admin 鉴权 / 重置段。"
    yellow "       启用方法：在 seed 容器日志中取随机密码，然后在调用此脚本前 export。"
else
    code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST "$BASE_URL/auth/login" \
        -H "Content-Type: application/json" \
        -c "$COOKIE_ADMIN" \
        -d "{\"username\":\"$ADMIN_USERNAME\",\"password\":\"$ADMIN_PASSWORD\"}")
    assert_status 200 "$code" "POST /auth/login (admin)"

    code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/admin/problems" -b "$COOKIE_ADMIN")
    assert_status 200 "$code" "GET /admin/problems (admin -> 200)"

    hr "6. admin 一键重置"
    code=$(curl -sS -o /tmp/reset.out -w '%{http_code}' -X POST "$BASE_URL/admin/reset" \
        -b "$COOKIE_ADMIN")
    assert_status 200 "$code" "POST /admin/reset (admin)"
    assert_body_has 'problem bank reset' "$(cat /tmp/reset.out)" "reset 返回 message"
fi

green "ALL ASSERTIONS PASSED"
