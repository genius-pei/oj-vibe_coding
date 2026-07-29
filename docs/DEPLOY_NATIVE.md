# MiniOJ — 原生部署指南

> 适用：Ubuntu 22.04+ / Debian 12+。本指南覆盖本地开发与单机部署，**走系统包 + 自编译后端 + nginx 反代**，无需任何容器运行时。

---

## 1. 系统依赖

| 包 | 用途 | 安装命令（Ubuntu/Debian） |
|---|---|---|
| `mysql-server` | MySQL 8.0 数据库 | `sudo apt install -y mysql-server` |
| `libmysqlclient-dev` | MySQL C API 头 + 库（后端编译依赖） | `sudo apt install -y libmysqlclient-dev` |
| `libjsoncpp-dev` | JSON 序列化 | `sudo apt install -y libjsoncpp-dev` |
| `libcrypt-dev` | 密码哈希 crypt(3) | `sudo apt install -y libcrypt-dev` |
| `g++` | C++ 编译器（编译后端 + 判题子进程） | `sudo apt install -y g++` |
| `libstdc++6` | C++ 运行时（编译产物执行需要） | 通常随 g++ 一起装 |
| `cmake ninja-build pkg-config` | 构建工具链 | `sudo apt install -y cmake ninja-build pkg-config` |
| `nginx`（推荐）或 `python3` | 前端静态服务 + `/api/*` 反代 | `sudo apt install -y nginx` |

> Fedora / RHEL 用 `dnf install` 等价包名：`mariadb-server mariadb-devel jsoncpp-devel libcrypt-devel gcc-c++ cmake ninja-build pkgconfig nginx`。

---

## 2. 启动 MySQL

```bash
# 1) 启用 + 启动
sudo systemctl enable --now mysql

# 2) 建库建用户（密码按需替换）
sudo mysql <<'SQL'
CREATE DATABASE IF NOT EXISTS minioj CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'minioj'@'localhost' IDENTIFIED BY 'minioj_local_pw';
GRANT ALL PRIVILEGES ON minioj.* TO 'minioj'@'localhost';
FLUSH PRIVILEGES;
SQL
```

---

## 3. 编译后端

```bash
cd backend
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build -j$(nproc)
```

产物：
- `build/minioj-backend` — HTTP 服务（监听 `0.0.0.0:8080`）
- `build/minioj-seed` — seed 灌库工具
- `build/minioj-reset-for-tests` — 测试基线复位工具（自动化测试用）

---

## 4. 配置 `.env`

后端从仓库根的 `.env` 读配置（`.env.example` 是模板）。原生环境最小可用配置：

```env
# ---- MySQL ----
DB_HOST=127.0.0.1
DB_PORT=3306
DB_NAME=minioj
DB_USER=minioj
DB_PASSWORD=minioj_local_pw

# ---- HTTP ----
HTTP_HOST=0.0.0.0
HTTP_PORT=8080

# ---- Session ----
SESSION_TTL_SECONDS=604800
SESSION_COOKIE_SECURE=false     # 仅 HTTPS 部署时改 true

# ---- Logging ----
LOG_LEVEL=info

# ---- Rate limit（可选覆盖）----
RATE_LIMIT_CAPACITY=60
RATE_LIMIT_REFILL_PER_SEC=1.0

# ---- CSRF trusted origins（可选；多值空格分隔）----
# CSRF_TRUSTED_ORIGINS="http://localhost http://127.0.0.1:8080"
```

---

## 5. 灌库 + 启动

```bash
# 1) 灌 seed 数据（5 道内置题 + admin/admin123 账号）
./backend/build/minioj-seed

# 2) 启动后端（前台跑，看日志）
./backend/build/minioj-backend
# 日志看到 "listening on 0.0.0.0:8080" 即 OK
```

生产环境建议用 `systemd` 管理：

```ini
# /etc/systemd/system/minioj-backend.service
[Unit]
Description=MiniOJ Backend
After=mysql.service

[Service]
Type=simple
User=minioj
WorkingDirectory=/opt/minioj
ExecStart=/opt/minioj/backend/build/minioj-backend
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now minioj-backend
```

---

## 6. 前端 + nginx 反代

把 `frontend/public/` 部署到 `/var/www/minioj/`，然后用 nginx 反代 `/api/*` 到 backend：

```nginx
# /etc/nginx/sites-available/minioj
server {
    listen 80;
    server_name _;
    root /var/www/minioj;
    index index.html;

    gzip on;
    gzip_types text/plain text/css application/json application/javascript text/javascript;

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

    # vendor 长期缓存
    location ~* ^/vendor/ {
        expires 7d;
        add_header Cache-Control "public, max-age=604800, immutable";
        try_files $uri =404;
    }

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/minioj /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

浏览器开 `http://<server-ip>` → 应能看到落地页。

---

## 7. 验证清单

- [ ] `curl http://127.0.0.1/api/problems` 返回 200 + JSON 数组（5 道题）
- [ ] `curl -X POST http://127.0.0.1/api/auth/login -d '{"username":"admin","password":"admin123"}' -H 'content-type: application/json'` 返回 200 + Set-Cookie
- [ ] 浏览器登录 admin/admin123 → `/admin/index.html` 看到题库管理
- [ ] 任选一道题提交 C++ AC 代码 → 收到 `{"verdict":"AC",...}`
- [ ] 浏览器 DevTools Network 看 `X-RateLimit-Remaining` 响应头（限流生效）
- [ ] DevTools Console 无 favicon 404 / 跨源 403 报错

---

## 8. 故障排查

| 症状 | 排查 |
|---|---|
| `mysql_real_connect: Access denied` | `.env` 的 `DB_USER`/`DB_PASSWORD` 与 §2 创建的不一致；`sudo mysql` 重设 |
| 提交代码全返 CE | 后端机器没装 `g++` — 见 §1；或 `which g++` 检查 |
| `/tmp` 满 | 判题 work dir 残留 — 后端进程异常退出。重启 + `rm -rf /tmp/minioj_pipeline_*` |
| `nginx 502 Bad Gateway` | backend 没起；`sudo systemctl status minioj-backend` |
| 跨源 POST 返 403 | `Origin` 不在 `CSRF_TRUSTED_ORIGINS`；加入白名单后重启 |
| 频繁 429 | 调高 `RATE_LIMIT_CAPACITY` / `RATE_LIMIT_REFILL_PER_SEC` |