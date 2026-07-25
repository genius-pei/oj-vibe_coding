FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        ca-certificates \
        default-libmysqlclient-dev \
        libjsoncpp-dev \
        libcrypt-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/
COPY third_party/ ./third_party/

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMINIOJ_BUILD_TESTS=OFF \
 && cmake --build build --target minioj-backend -j

ENV HTTP_HOST=0.0.0.0 \
    HTTP_PORT=8080 \
    LOG_LEVEL=info

EXPOSE 8080

CMD ["/app/build/minioj-backend"]
