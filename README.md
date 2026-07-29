# MiniOJ — 仿 LeetCode 在线判题系统

> 面向**算法训练 + 教学训练**的轻量级 OJ，单实例即可承载 1–40 人并发。
> 规格文档：[SPEC.md](./SPEC.md) · 后端 API：[API.md](./API.md) · curl 用例：[api-curl-test.md](./api-curl-test.md)

---

## 一键启动

> 推荐路径：**裸机原生部署**（Ubuntu 22.04+ / Debian）。完整步骤见 [docs/DEPLOY_NATIVE.md](./docs/DEPLOY_NATIVE.md) 与 [SPEC §9.4](./SPEC.md#94-依赖与安装指令)。

```bash
# 1. 一行装齐依赖（apt 路径）
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
    g++ default-libmysqlclient-dev libjsoncpp-dev libssl-dev \
    default-mysql-client nginx

# 2. 准备 MySQL（系统服务）
sudo systemctl enable --now mysql
sudo mysql -uroot <<'SQL'
CREATE DATABASE minioj DEFAULT CHARACTER SET utf8mb4;
CREATE USER 'minioj'@'localhost' IDENTIFIED BY 'change_me';
GRANT ALL ON minioj.* TO 'minioj'@'localhost';
FLUSH PRIVILEGES;
SQL

# 3. 后端构建
cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j

# 4. 跑 schema（一次性）
sudo mysql -uminioj -pchange_me minioj < backend/sql/schema.sql

# 5. 灌种子题 + 创建 admin（密码随机写入 stdout）
./backend/build/minioj-seed --reset

# 6. 起后端（前台 / systemd 二选一）
HTTP_HOST=127.0.0.1 HTTP_PORT=8080 \
DB_HOST=127.0.0.1 DB_PORT=3306 DB_NAME=minioj \
DB_USER=minioj DB_PASSWORD=change_me \
./backend/build/minioj-backend

# 7. 配 nginx（反代 /api + 静态文件），见 docs/DEPLOY_NATIVE.md §4
sudo cp frontend/nginx.conf /etc/nginx/sites-available/minioj
sudo ln -sf /etc/nginx/sites-available/minioj /etc/nginx/sites-enabled/minioj
sudo cp -r frontend/public /var/www/minioj
sudo nginx -t && sudo systemctl reload nginx
```

访问 <http://localhost>：
- 首页 / 题单 / 注册 / 登录 / 提交代码：对所有用户开放
- 后台管理 <http://localhost/admin>：账号 `admin` + seed 阶段生成的随机密码

### 重置

```bash
# 只重置题库 + admin（保留 user / session）
./backend/build/minioj-seed --reset

# 或走 API
curl -X POST http://localhost/api/admin/reset -b /tmp/admin-cookie.txt
```

---

## 端到端冒烟

`api-smoke.sh` 用 `curl` 覆盖：公开题单 / 注册 / 登录 / 登出 / 提交 / admin 三态鉴权 / admin 一键重置。**21 条断言**全部通过即返回 `ALL ASSERTIONS PASSED`。

```bash
# 起 backend 在 :8080（含 seed 题目与 admin）后再跑
BASE_URL=http://127.0.0.1:8080/api \
ADMIN_USERNAME=admin ADMIN_PASSWORD=<from seed log> \
./api-smoke.sh
```

每个用例详情（请求 / 响应 / 字段约束）见 [api-curl-test.md](./api-curl-test.md)。

---

## 项目结构

```
.
├── SPEC.md                 # 冻结的需求规格
├── API.md                  # 后端接口文档
├── api-curl-test.md        # curl 接口自动化测试文档
├── api-smoke.sh            # 一键冒烟脚本
├── .env.example            # 环境变量样例
├── docs/
│   └── DEPLOY_NATIVE.md    # 裸机部署指南（系统 MySQL + nginx + 后端 systemd）
├── backend/                # C++17 后端（cpp-httplib + MySQL + jsoncpp）
│   ├── CMakeLists.txt
│   ├── include/            # 公共头
│   ├── src/                # 业务代码（http / db / judge / auth / util）
│   ├── sql/                # 建表 DDL
│   ├── seed/
│   │   ├── problems.json   # 内置 5 题
│   │   └── tags.json
│   ├── scripts/seed.cpp    # 独立 seed 进程（灌库 + 创建 admin）
│   └── tests/              # GTest 单元 / 集成测试（17 个二进制 +218 例）
└── frontend/               # 静态前端（Nginx + 原生 HTML/JS）
    ├── nginx.conf          # 反代 /api → backend:8080 + gzip + vendor 缓存
    └── public/             # 静态 HTML / CSS / JS / vendor
```

---

## 进度（按 SPEC §10）

| 阶段 | 状态 |
|------|------|
| Phase 0 脚手架 | ✅ |
| Phase 1 题单展示 | ✅ |
| Phase 2 用户账号 | ✅ |
| Phase 3 判题核心 | ✅ |
| Phase 3.5 判题单测（41 例） | ✅ |
| Phase 4 前端编辑器 + 结果 | ⚠️ 后端通，前端待做 |
| Phase 5 管理员后台（CRUD + role 中间件 + 一键重置） | ✅ |
| Phase 5.5 后端单测（test_admin_request / test_admin_auth / test_problem_dao / test_seed_loader） | ✅ |
| Phase 6 部署 / 冒烟 / 文档 | ✅ (裸机部署：通过 seed 进程 + api-smoke.sh 端到端验证；nginx 反代见 `docs/DEPLOY_NATIVE.md`) |
