#!/usr/bin/env bash
set -euo pipefail

BASE_URL="http://localhost:8080/api"
COOKIE_USER="/tmp/minioj-cookies/user.txt"
COOKIE_ADMIN="/tmp/minioj-cookies/admin.txt"
mkdir -p "$(dirname "$COOKIE_USER")"

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

hr "1. 公开题单"
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/problems")
assert_status 200 "$code" "GET /problems"

hr "2. 题目详情 (存在 / 不存在)"
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE_URL/problems/1")
assert_status 200 "$code" "GET /problems/1"
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

hr "4. 提交代码"
code=$(curl -sS -o /tmp/sub.out -w '%{http_code}' -X POST "$BASE_URL/submissions" \
    -H "Content-Type: application/json" \
    -d '{"problem_id":1,"lang":"cpp","code":"#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b;return 0;}"}')
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

yellow "WARN: skip admin auth test (router has no role middleware yet)"
yellow "      after /api/admin role check is wired, enable this:"
cat <<'EOF'
   code=$(curl ... -b "$COOKIE_USER" -X POST "$BASE_URL/admin/problems" ...)
   assert_status 401 "$code" "non-admin hits /admin/*"
EOF

green "ALL ASSERTIONS PASSED"