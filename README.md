# MiniOJ

> 面向**算法训练 + 教学训练**的轻量级在线判题系统，仿 LeetCode 核心体验。
> 单实例支撑 1–40 人并发；C/C++ 代码秒级评测；支持 AC / WA / TLE / CE / MLE / RE 6 态判定。
>
> 规格：[SPEC.md](./SPEC.md) · API：[API.md](./API.md) · 部署：[DEPLOY.md](./DEPLOY.md) · 测试：[web自动化测试文档.md](./web自动化测试文档.md)

---

## 一句话启动

```bash
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
    g++ default-libmysqlclient-dev libjsoncpp-dev libssl-dev libcrypt-dev \
    default-mysql-client nginx
sudo systemctl enable --now mysql
sudo mysql -uroot <<'SQL'
CREATE DATABASE minioj CHARACTER SET utf8mb4;
CREATE USER 'minioj'@'localhost' IDENTIFIED BY 'minioj_dev_pw';
GRANT ALL ON minioj.* TO 'minioj'@'localhost';
FLUSH PRIVILEGES;
SQL
cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j$(nproc)
./backend/build/minioj-reset-for-tests        # 灌 5 题 + admin/admin123
./backend/build/minioj-backend &              # 起后端 :8080
sudo cp -r frontend/public /var/www/minioj
sudo cp frontend/nginx.conf /etc/nginx/sites-available/minioj
sudo sed -i 's|root /usr/share/nginx/html;|root /var/www/minioj|; s|http://backend:8080|http://127.0.0.1:8080|' \
    /etc/nginx/sites-available/minioj
sudo ln -sf /etc/nginx/sites-available/minioj /etc/nginx/sites-enabled/minioj
sudo systemctl enable --now nginx
```

访问 **http://122.51.84.172/**：落地页 → 题单 → 提交代码。
后台：**http://122.51.84.172/admin/**（账号 `admin` / `admin123`）。

完整步骤 + systemd / 故障排查见 **[DEPLOY.md](./DEPLOY.md)**。

---

## 项目结构

```
minioj/
├── SPEC.md                      # 冻结的需求规格（含 API / 数据模型 / 判题机制）
├── README.md                    # 本文件
├── DEPLOY.md                    # 裸机原生部署指南（apt + MySQL + nginx + systemd）
├── API.md                       # 后端 HTTP 接口文档
├── api-smoke.sh                 # shell 端到端冒烟（21 条断言）
├── api-curl-test.md             # curl 接口断言文档（21 端点）
├── web自动化测试文档.md          # Playwright + pytest UI 自动化（92 用例）
├── dependence.md                # 系统 / Vendored 依赖清单
│
├── backend/                     # C++17 后端（cpp-httplib + MySQL + jsoncpp）
│   ├── CMakeLists.txt
│   ├── third_party/             # 单头 vendored（cpp-httplib）
│   ├── include/                 # 公共头（common / config / types）
│   ├── src/
│   │   ├── main.cpp             # 入口：读配置 → 起 HTTP → 注册路由
│   │   ├── http/                # 路由 / 中间件 / 三个 handler 文件
│   │   ├── db/                  # MySQL 连接池 + DAO + seed_loader + migrate
│   │   ├── judge/               # worker_pool / compiler / runner / diff / pipeline
│   │   ├── auth/                # session / password (bcrypt) / validator
│   │   └── util/                # config / logger / signal (SIGTERM)
│   ├── sql/
│   │   └── schema.sql           # 1NF 建表 DDL（problems / testcases / tags / problem_tags / users / sessions）
│   ├── seed/
│   │   ├── problems.json        # 内置 5 道题
│   │   └── tags.json
│   ├── scripts/                 # seed.cpp / reset_for_tests.cpp
│   └── tests/                   # GTest 单元 + 集成测试（17 个二进制，~110 例）
│
├── frontend/                    # 静态前端
│   ├── nginx.conf               # nginx site 模板（裸机部署时把 proxy_pass 改 127.0.0.1:8080）
│   ├── public/                  # 7 个 HTML + 6 个 CSS + 10 个 JS + vendor（marked / CodeMirror / Ace）
│   └── tests/                   # 前端 validation.test.mjs（node:test 跑）
│
├── docs/                        # 内部设计文档（ui-ux-pro-max 设计令牌 / mock server 等）
├── scripts/                     # mock_server.py 等开发辅助
├── test/                        # 接口测试套件
├── design-system/minioj/        # ui-ux-pro-max skill 持久化的设计令牌
├── .env.example                 # 环境变量样例
└── .gitignore
```

---

## 核心能力
- **判题**：fork + exec + rlimit 沙箱（CPU 500ms / MEM 256MB / 编译 3s / 子进程 ≤8 信号量 / FIFO 队列）
- **题库**：1NF 规范化的 6 张表（problems / testcases / tags / problem_tags / users / sessions）
- **用户**：bcrypt 密码 + Session + Cookie 鉴权，role 中间件拦截 `/api/admin/*`
- **安全**：CSRF 跨源拦截 + 速率限制（令牌桶 60 req/min/IP）+ CORS
- **运维**：systemd 优雅关闭（SIGTERM drain worker pool）+ 日志分级 + 健康检查 `/healthz`

完整接口定义见 [API.md](./API.md)；判题流程 / 资源限制 / 状态机见 [SPEC.md §8](./SPEC.md#8-判题机制)。

---

## 端到端冒烟

```bash
# 21 条 curl 断言（公开题单 / 注册 / 登录 / 登出 / 提交 / admin 三态鉴权 / reset）
./api-smoke.sh

# 92 条 Playwright + pytest 用例（详见 web自动化测试文档.md）
cd test && python3 -m pytest
```

