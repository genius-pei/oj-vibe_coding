# MiniOJ — 仿 LeetCode 在线判题系统

> 面向**算法训练 + 教学训练**的轻量级 OJ，单实例即可承载 1–40 人并发。
> 规格文档：[SPEC.md](./SPEC.md) · 后端 API：[API.md](./API.md) · curl 用例：[api-curl-test.md](./api-curl-test.md)

---

## 一键启动

### 方式 A：Docker Compose（推荐）

```bash
# 1. 准备环境变量（首次）
cp .env.example .env
# 按需修改 .env：MYSQL_ROOT_PASSWORD / DB_PASSWORD 等

# 2. 启动 mysql + backend + frontend
docker compose up -d

# 3. 首次启动：初始化题库与管理员账号（admin 密码会打到日志）
docker compose --profile seed run --rm seed
docker compose --profile seed logs seed | tail -3
# 看到形如：
#   ADMIN_USERNAME=admin
#   ADMIN_PASSWORD=XXXXXXXX
# 即成功
```

访问 <http://localhost>：

- 首页 / 题单 / 注册 / 登录 / 提交代码：对所有用户开放
- 后台管理 <http://localhost/admin>：账号 `admin` + seed 阶段生成的随机密码

容器化路径下，后端实际在 `backend:8080`，Nginx 反代 `/api/*` 到该端口。
调试可以直接打后端绕开反代：`curl http://localhost:8080/api/problems`（以 `docker-compose.yml` 暴露端口为准）。

### 重置与销毁

```bash
# 清空所有数据回到初始状态
docker compose down -v
docker compose up -d
docker compose --profile seed run --rm seed
```

只重置题库（保留 user/session）：

```bash
# 走 API：
curl -X POST http://localhost/api/admin/reset -b /tmp/admin-cookie.txt
# 走 seed 进程：
docker compose --profile seed run --rm seed --reset
```

---

## 方式 B：裸机 / 本地开发

> 详细依赖与镜像源见 [SPEC §9.4](./SPEC.md#94-依赖与安装指令)。

```bash
# Ubuntu 22.04+ 一行装齐（apt 路径）
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
    g++ default-libmysqlclient-dev libjsoncpp-dev libssl-dev \
    default-mysql-client

# 后端构建
cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j

# 跑单元 / 集成测试（需要 MySQL：DB_HOST/PORT/NAME/USER/PASSWORD 环境变量）
export DB_HOST=127.0.0.1 DB_PORT=3306 DB_NAME=minioj \
       DB_USER=minioj DB_PASSWORD=your_pw
cd backend/build && ctest -j4 --output-on-failure

# 启动后端
HTTP_HOST=127.0.0.1 HTTP_PORT=8080 ./backend/build/minioj-backend

# 首次 / 重置题库 + 创建 admin（密码随机写入 stdout）
cd /path/to/minioj          # seed 进程从这里取 backend/seed/problems.json
./backend/build/minioj-seed --reset
# 或者用一个固定密码：
./backend/build/minioj-seed --reset --admin-password=Passw0rd!
```

> seed 进程若 CWD 不在仓库根，可通过 `MINIOJ_SEED_JSON=/abs/path/backend/seed/problems.json` 显式指定。

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
├── docker-compose.yml      # 一键编排：mysql / backend / frontend / seed
├── .env.example            # 环境变量样例
├── backend/                # C++17 后端（cpp-httplib + MySQL + jsoncpp）
│   ├── CMakeLists.txt
│   ├── Dockerfile
│   ├── include/            # 公共头
│   ├── src/                # 业务代码（http / db / judge / auth / util）
│   ├── sql/                # 建表 DDL
│   ├── seed/
│   │   ├── problems.json   # 内置 5 题
│   │   └── tags.json
│   ├── scripts/seed.cpp    # 独立 seed 进程（灌库 + 创建 admin）
│   └── tests/              # GTest 单元 / 集成测试（17 个二进制 +218 例）
└── frontend/               # 静态前端（Nginx + 原生 HTML/JS）
    ├── Dockerfile
    ├── nginx.conf
    └── public/
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
| Phase 6 部署 / 冒烟 / 文档 | ✅ (docker 部署未在本机跑，已通过裸机 seed 进程与 api-smoke.sh 端到端验证) |
