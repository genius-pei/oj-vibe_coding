# syntax=docker/dockerfile:1.7

# ---- Build stage：包含 g++ / cmake / 客户端开发库 ----
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# 鸡生蛋：先装 ca-certificates（用默认源），再切清华源
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && sed -i 's|http://archive.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g; \
             s|http://security.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' \
            /etc/apt/sources.list.d/ubuntu.sources

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        default-libmysqlclient-dev \
        libjsoncpp-dev \
        libcrypt-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get clean

WORKDIR /app

COPY CMakeLists.txt ./
COPY include/  ./include/
COPY src/      ./src/
COPY third_party/ ./third_party/
COPY scripts/  ./scripts/

# -Os 替代 -O2：二进制更小，编译期内存峰值更低（对 2GB 宿主友好）
# -j1：2GB 机器编译并发硬限为 1，否则 ninja 默认按核数并行 → OOM
# MINIOJ_BUILD_TESTS=OFF：镜像里不带测试可执行，省时省内存
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS_RELEASE="-Os" \
        -DMINIOJ_BUILD_TESTS=OFF \
 && ninja -C build -j1 minioj-backend minioj-seed minioj-reset-for-tests \
 && strip --strip-all build/minioj-backend build/minioj-seed build/minioj-reset-for-tests \
 && rm -rf build/CMakeFiles build/cmake_install.cmake build/CMakeCache.txt \
 && rm -rf build/.*.cmake build/MinIOJ*.cmake 2>/dev/null || true

# ---- Runtime stage：仅运行时库，无编译器 ----
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    HTTP_HOST=0.0.0.0 \
    HTTP_PORT=8080 \
    LOG_LEVEL=info

# 鸡生蛋：先装 ca-certificates（用默认源），再切清华源（同 build stage）
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && sed -i 's|http://archive.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g; \
             s|http://security.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' \
            /etc/apt/sources.list.d/ubuntu.sources

# 只装运行时库（dev 包会自动把对应 lib 拉进来，最简可靠）
# g++ + libstdc++6：判题子进程需要 g++ 编译用户代码（execlp("g++")）
#   注释：以前只装 lib 系列 → execlp 失败 exit 127，stderr 为空 → pipeline 误判 CE
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        default-libmysqlclient-dev \
        libjsoncpp-dev \
        libcrypt-dev \
        g++ \
        libstdc++6 \
 && rm -rf /usr/include /usr/lib/*/pkgconfig /usr/share/man /usr/share/doc \
 && find /usr/lib -name '*.a' -delete 2>/dev/null || true \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get clean

WORKDIR /app

COPY --from=build /app/build/minioj-backend          /app/minioj-backend
COPY --from=build /app/build/minioj-seed             /app/minioj-seed
COPY --from=build /app/build/minioj-reset-for-tests  /app/minioj-reset-for-tests

EXPOSE 8080

CMD ["/app/minioj-backend"]