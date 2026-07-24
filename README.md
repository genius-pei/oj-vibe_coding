# MiniOJ — 仿 LeetCode 在线判题系统

> 面向**求职展示 + 教学训练**的轻量级 OJ，单实例即可承载 1–40 人并发。
> 规格文档：[SPEC.md](./SPEC.md)

---

## 一键启动

```bash
# 1. 准备环境变量
cp .env.example .env
# 编辑 .env，至少修改 MYSQL_ROOT_PASSWORD / DB_PASSWORD

# 2. 启动 mysql + backend + frontend
docker compose up -d

# 3. 首次启动：初始化题库与管理员账号
docker compose --profile seed run --rm seed
# 管理员密码会输出到 seed 容器日志，请记录：
docker compose --profile seed logs seed
```

启动完成后访问 <http://localhost>：
- 题单 / 题目详情 / 注册 / 登录：均对所有用户开放
- 管理员后台：<http://localhost/admin>，账号 `admin` + seed 阶段生成的随机密码

---

## 重置与销毁

```bash
# 清空所有数据回到初始状态
docker compose down -v
docker compose up -d
docker compose --profile seed run --rm seed
```

---

## 项目结构

```
.
├── SPEC.md                 # 冻结的需求规格
├── docker-compose.yml      # 一键编排：mysql / backend / frontend / seed
├── .env.example            # 环境变量样例
├── backend/                # C++17 后端（cpp-httplib + MySQL）
│   ├── CMakeLists.txt
│   ├── Dockerfile
│   ├── include/            # 公共头
│   ├── src/                # 业务代码
│   ├── sql/                # 建表 DDL
│   ├── seed/               # 初始题库 JSON
│   └── tests/              # GTest 单元测试
└── frontend/               # 静态前端（Nginx + 原生 HTML/JS）
    ├── Dockerfile
    ├── nginx.conf
    └── public/
```

---

## 本地裸机开发（可选）

详见 [SPEC.md §9.4](./SPEC.md)：

```bash
# Ubuntu 22.04+ 一行装齐
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
    g++ default-libmysqlclient-dev libssl-dev default-mysql-client nginx

# 后端构建
cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build backend/build -j

# 跑测试
./backend/build/minioj_test_logger
./backend/build/minioj_test_config
```

---

## 进度

按 [SPEC §10](./SPEC.md) 分阶段推进，当前 **Phase 0（脚手架）** 已完成。