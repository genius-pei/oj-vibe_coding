# MiniOJ 依赖清单

## 宿主机依赖

| 依赖 | 当前版本 | 用途 |
|---|---:|---|
| Docker Engine | 29.1.3 | 容器运行时 |
| Docker Compose | 2.40.3 | 服务编排 |
| g++ | 13.3.0 | C++17 后端与判题代码编译 |
| CMake | 3.28.3 | 后端构建配置 |
| Ninja | 1.11.1 | 后端构建工具 |
| GNU Make | 4.3 | 后端备用构建工具 |
| pkg-config | 1.8.1 | 开发库发现 |
| MySQL Client | 8.0.46 | 数据库客户端与连通检查 |
| MySQL Client Library | 21.2.46 | 后端 MySQL C API |
| OpenSSL | 3.0.13 | HTTPS 与加密能力 |
| Nginx | 1.24.0 | 静态前端与 API 反向代理 |
| libcrypt | 系统版本 | bcrypt 哈希底层实现 |

## 后端 Vendored 依赖

| 依赖 | 版本或实现 | 文件路径 | 用途 |
|---|---|---|---|
| cpp-httplib | 0.18.3 | `backend/third_party/httplib.h` | HTTP 服务 |
| bcrypt 封装 | 基于系统 `crypt(3)` 的 bcrypt `$2b$` | `backend/third_party/bcrypt.h`, `backend/third_party/bcrypt.cpp` | 密码生成与验证 |

使用 bcrypt 封装时，链接参数需要包含 `-lcrypt`，多线程程序应同时包含 `-pthread`。

## 后端系统依赖（pkg-config 链接，不 vendored）

| 依赖 | 当前版本 | pkg-config 名 | 用途 |
|---|---:|---|---|
| jsoncpp | 1.9.5 | `jsoncpp` | JSON 解析与序列化 |

## 前端 Vendored 依赖

| 依赖 | 版本 | 文件路径 | 用途 |
|---|---:|---|---|
| marked.js | 12.0.2 | `frontend/public/vendor/marked.min.js` | Markdown 渲染 |
| CodeMirror | 6.0.1 | `frontend/public/vendor/codemirror/editor.js` | 代码编辑器 |
| CodeMirror C++ | 6.0.3 | `frontend/public/vendor/codemirror/cpp.js` | C/C++ 语法支持 |
| CodeMirror One Dark | 6.1.2 | `frontend/public/vendor/codemirror/theme-one-dark.js` | 暗色编辑器主题 |
| process 浏览器兼容模块 | esm.sh bundle | `frontend/public/vendor/codemirror/process.js` | CodeMirror C++ bundle 的本地运行依赖 |

前端依赖均已保存到仓库，不需要从 CDN 动态加载。

## 容器运行依赖

| 组件 | 建议版本 | 用途 |
|---|---:|---|
| MySQL Server | 8.0.x | 题库、用户和 Session 持久化 |
| Nginx Alpine | 最新稳定版 | 前端容器和 API 反向代理 |
| Ubuntu/Debian 构建镜像 | 支持 g++ 9+、CMake 3.16+ | 后端镜像构建 |

## 验证状态

- cpp-httplib 与 jsoncpp 已通过 C++17 编译验证。
- bcrypt 已通过密码生成、正确密码验证和错误密码拒绝测试。
- Docker daemon 和 Docker Compose 可用。
- MySQL、OpenSSL、jsoncpp 开发头文件及 pkg-config 元数据可用。
