# MiniOJ — 部署指南

> **零容器依赖的裸机原生部署**。Ubuntu 22.04+ / Debian 12+ 一行装齐系统包，3 个服务（mysqld + 自编译 `minioj-backend` + nginx）即可对外提供 80 端口入口。
>
> **✅ 已实测（2026-07-29 122.51.84.172）**：本指南按序执行通过 — MySQL 8.0.46 + 自编译 `minioj-backend` + nginx 1.24.0 三者常驻，5 题 / 16 用例 / admin·admin123 全通，6 端页面 + 14 API + AC / WA / CE / TLE / RE 5 态判定实测全对。

---

## 1. 系统依赖

### 1.1 Ubuntu / Debian 一行装齐

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake ninja-build pkg-config \
    g++ \
    default-libmysqlclient-dev libjsoncpp-dev libssl-dev libcrypt-dev \
    default-mysql-client \
    nginx
```

> Ubuntu 20.04：把 `default-libmysqlclient-dev` / `default-mysql-client` 换成 `libmysqlclient-dev` / `mysql-client`。

### 1.2 依赖作用一览

| 包 | 作用 |
|---|---|
| `mysql-server`（由 `default-mysql-client` 拉入） | 题库 / 用户 / Session 持久化 |
| `libmysqlclient-dev` | MySQL C API 头 + 库（后端编译依赖） |
| `libjsoncpp-dev` | JSON 解析与序列化（pkg-config 名 `jsoncpp`） |
| `libcrypt-dev` | 密码哈希（glibc `crypt(3)` bcrypt `$2b$`） |
| `g++` | C++17 编译器（编译后端 + 跑判题子进程） |
| `cmake` + `ninja-build` + `pkg-config` | 构建工具链 |
| `nginx` | 前端静态服务 + `/api/*` 反代 |

> 第三方依赖仅 `cpp-httplib`（vendored 单头，见 `backend/third_party/httplib.h`），零网络依赖，离线可编译。

---

## 2. 准备 MySQL

```bash
sudo systemctl enable --now mysql
sudo mysql <<'SQL'
CREATE DATABASE IF NOT EXISTS minioj
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'minioj'@'localhost' IDENTIFIED BY 'minioj_dev_pw';
GRANT ALL PRIVILEGES ON minioj.* TO 'minioj'@'localhost';
FLUSH PRIVILEGES;
SQL
```

> 密码按需替换（示例用 `minioj_dev_pw`），下文所有命令都按此密码演示。

### 2.1 密码认证插件说明

MySQL 8.0 默认用 `caching_sha2_password`，本后端客户端（`libmysqlclient`）支持。若遇到 `Access denied`：

```sql
ALTER USER 'minioj'@'localhost' IDENTIFIED WITH mysql_native_password BY 'minioj_dev_pw';
FLUSH PRIVILEGES;
```

---

## 3. 编译后端

```bash
cd <repo-root>
cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j$(nproc)
```

产物（`backend/build/`）：

| 二进制 | 用途 |
|---|---|
| `minioj-backend` | HTTP 服务 + 判题（监听 `0.0.0.0:8080`） |
| `minioj-seed` | 灌种子数据 + 创建 admin 账号 |
| `minioj-reset-for-tests` | 自动化测试用：清库 + 复位 id=1 + admin/admin123 + 清 `webtest_*` 临时用户 |

---

## 4. 配置 `.env`

仓库根的 `.env` 是后端配置文件（`.env.example` 是模板）。最小可用配置：

```env
# ---- MySQL ----
DB_HOST=127.0.0.1
DB_PORT=3306
DB_NAME=minioj
DB_USER=minioj
DB_PASSWORD=minioj_dev_pw

# ---- HTTP ----
HTTP_HOST=127.0.0.1
HTTP_PORT=8080

# ---- Session ----
SESSION_TTL_SECONDS=604800
SESSION_COOKIE_SECURE=false     # 仅 HTTPS 部署时改 true

# ---- Logging ----
LOG_LEVEL=info
```

高级可选项（一般不需要改）：

```env
DB_POOL_SIZE=10
DB_CONNECT_TIMEOUT_SECONDS=5
DB_ACQUIRE_TIMEOUT_MS=2000
RATE_LIMIT_CAPACITY=60
RATE_LIMIT_REFILL_PER_SEC=1.0
CSRF_TRUSTED_ORIGINS="http://localhost http://127.0.0.1:8080"
```

---

## 5. 灌库 + 启动

### 5.1 灌种子数据

```bash
# 标准模式：admin 密码随机写入 stdout
./backend/build/minioj-seed

# 自动化测试模式：admin 固定为 admin123
./backend/build/minioj-reset-for-tests
# 输出末尾：READY=true
```

> `minioj-reset-for-tests` 等价于"清库 + 灌种子 + admin/admin123 + 清临时用户"，**重复跑可幂等复位**。CI / 接口自动化测试必用。
>
> 两个工具都从仓库根目录找 `backend/seed/problems.json`；如 CWD 不在仓库根，设 `MINIOJ_SEED_JSON=/abs/path/backend/seed/problems.json` 显式指定。

### 5.2 启动后端

#### A. 前台跑（看日志）

```bash
./backend/build/minioj-backend
```

看到 `listening on 0.0.0.0:8080` 即 OK。

#### B. systemd 管理（生产推荐）

新建 `/etc/systemd/system/minioj-backend.service`：

```ini
[Unit]
Description=MiniOJ Backend
After=mysql.service
Wants=mysql.service

[Service]
Type=simple
User=minioj
Group=minioj
WorkingDirectory=/opt/minioj
EnvironmentFile=/opt/minioj/.env
ExecStart=/opt/minioj/backend/build/minioj-backend
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

```bash
# 把项目部署到 /opt/minioj
sudo mkdir -p /opt/minioj
sudo cp -r . /opt/minioj/
sudo chown -R minioj:minioj /opt/minioj

sudo systemctl daemon-reload
sudo systemctl enable --now minioj-backend
sudo systemctl status minioj-backend
```

---

## 6. 前端 + nginx 反代

### 6.1 部署前端静态文件

```bash
sudo cp -r frontend/public /var/www/minioj
```

### 6.2 配 nginx site

`/etc/nginx/sites-available/minioj`（仓库 `frontend/nginx.conf` 是模板，**注意把 `proxy_pass` 改成 `127.0.0.1:8080`，把 `root` 改成 `/var/www/minioj`**，容器版的 `http://backend:8080` 与 `/usr/share/nginx/html` 在裸机不通）：

```nginx
server {
    listen 80;
    server_name _;
    root /var/www/minioj;
    index index.html;

    gzip on;
    gzip_types text/plain text/css application/json application/javascript text/javascript;

    location = /healthz {
        return 200 "ok\n";
        add_header Content-Type text/plain;
    }

    # 反代后端 API
    location /api/ {
        proxy_pass         http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header   Host              $host;
        proxy_set_header   X-Real-IP         $remote_addr;
        proxy_set_header   X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header   X-Forwarded-Proto $scheme;
        proxy_read_timeout 30s;
        proxy_send_timeout 30s;
        client_max_body_size 4m;
    }

    # vendor 长期缓存（CodeMirror / marked）
    location ~* ^/vendor/ {
        expires 7d;
        add_header Cache-Control "public, max-age=604800, immutable";
        try_files $uri =404;
    }

    # SPA 风格路由兜底
    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

启用：

```bash
sudo ln -sf /etc/nginx/sites-available/minioj /etc/nginx/sites-enabled/minioj
sudo nginx -t && sudo systemctl reload nginx
```

> 外网部署时**只需放行 80 端口**（腾讯云安全组默认仅放 22/3389，需手动添加）。后端 8080 与 MySQL 3306 都只监听 localhost，外部不可直连。

---

## 7. 验证清单

```bash
# 1. 后端 API
curl -s http://127.0.0.1:8080/api/problems | head -c 200

# 2. 通过 nginx 走 80
curl -s http://127.0.0.1/api/problems | head -c 200

# 3. 三个页面
curl -s -o /dev/null -w "/ → HTTP %{http_code}\n" http://127.0.0.1/
curl -s -o /dev/null -w "/problems.html → HTTP %{http_code}\n" http://127.0.0.1/problems.html
curl -s -o /dev/null -w "/problem.html?id=1 → HTTP %{http_code}\n" "http://127.0.0.1/problem.html?id=1"

# 4. admin 登录
curl -X POST http://127.0.0.1/api/auth/login \
    -H 'Content-Type: application/json' \
    -d '{"username":"admin","password":"admin123"}' \
    -c /tmp/admin.cookie
# 期望：200 + Set-Cookie: minioj_sid=...

# 5. 提交 AC 代码
curl -X POST http://127.0.0.1/api/submissions \
    -H 'Content-Type: application/json' \
    -d '{"problem_id":1,"lang":"cpp","code":"#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b;return 0;}"}'
# 期望：verdict=AC
```

浏览器访问 `http://<server-ip>/` 应看到落地页；用 admin/admin123 登录后可进 `/admin/index.html` 看到题库管理。

---

## 8. 故障排查

| 症状 | 排查 |
|---|---|
| `mysql_real_connect: Access denied` | `.env` 的 `DB_USER` / `DB_PASSWORD` 与 §2 创建的不一致；或运行 §2.1 切到 `mysql_native_password` |
| 提交代码全返 `CE: cannot find -lgcc` 或 `iostream: No such file` | 后端机器没装 `g++` 与 `libstdc++-13-dev`；`apt install -y g++ libstdc++-13-dev` 后重启 backend |
| `nginx 502 Bad Gateway` | backend 没起；`systemctl status minioj-backend` 看日志 |
| 跨源 POST 返 `403` | `Origin` 不在 `CSRF_TRUSTED_ORIGINS`；白名单追加后重启 |
| 频繁 `429 Too Many Requests` | 调高 `RATE_LIMIT_CAPACITY` / `RATE_LIMIT_REFILL_PER_SEC` |
| `/tmp` 满 + 残留 `minioj_pipeline_*` | 后端进程异常退出；`systemctl restart minioj-backend` + `rm -rf /tmp/minioj_pipeline_*` |
| `minioj-reset-for-tests: failed to open seed file` | CWD 不在仓库根；设 `MINIOJ_SEED_JSON=$PWD/backend/seed/problems.json` 显式指定 |
| 3306 端口占用 | `ss -tlnp \| grep :3306`；若有 docker 容器占着（`minioj-mysql` 旧容器），`docker stop minioj-mysql` 释放 |

---

## 9. 一键重置

```bash
# 方式 A：reset_for_tests 二进制（CI / 接口测试推荐，幂等）
./backend/build/minioj-reset-for-tests
# 输出：5 problem(s), 16 testcase(s) / admin/admin123 可用

# 方式 B：走 API（保留 user / session，只重置题库 + admin）
curl -X POST http://127.0.0.1/api/admin/reset -b /tmp/admin.cookie
```

---

## 10. 相关文档

- [README.md](./README.md) — 项目概览 + 一键启动
- [SPEC.md](./SPEC.md) — 完整需求规格（含 API / 数据模型 / 判题机制）
- [API.md](./API.md) — 后端接口文档
- [api-curl-test.md](./api-curl-test.md) — curl 接口断言用例
- [api-smoke.sh](./api-smoke.sh) — shell 端到端冒烟脚本（21 条断言）
- [web自动化测试文档.md](./web自动化测试文档.md) — Playwright + pytest 92 用例
