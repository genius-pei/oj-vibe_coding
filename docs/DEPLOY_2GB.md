# 2GB 内存宿主机部署指南

> 本指南配套仓库内的 `docker-compose.yml`（已对 MySQL 做低内存调优、给所有服务加 `mem_limit`）。
> 项目自身的代码 / API / 接口 100% 不变，只在以下两层做了适配：
>
> 1. **镜像层**（`backend/Dockerfile`）：多阶段构建、`-Os` 优化、`-j1` 编译、`strip`
> 2. **编排层**（`docker-compose.yml`）：MySQL `performance_schema=OFF`、内存上限、日志上限
>
> 本文档处理**宿主机层**（需 SSH 到机器一次性执行），与上面两层是同一套方案的最后一环。

---

## 1. 加 swap（最关键的续命手段）

2GB 物理内存 + 4GB swap = 6GB 可用地址空间，OOM killer 触发概率降到几乎为零。

```bash
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab

# 低内存下推荐 swappiness=10，宁可多用 swap 也不要频繁触发 OOM
sudo sysctl -w vm.swappiness=10
echo 'vm.swappiness=10' | sudo tee -a /etc/sysctl.d/99-minioj.conf
```

验证：
```bash
free -h
# Swap 应显示 4.0G
```

---

## 2. 内核参数（一次性）

```bash
sudo tee /etc/sysctl.d/99-minioj.conf <<'EOF'
vm.swappiness=10
vm.dirty_ratio=10
vm.dirty_background_ratio=5
vm.vfs_cache_pressure=50
vm.overcommit_memory=1
EOF

sudo sysctl -p
```

---

## 3. Docker daemon 调优

2GB 机器上 dockerd 自身默认配置有两个常见坑：日志无上限会撑爆磁盘、`storage-driver` 在低内存下偶发问题。

```bash
sudo tee /etc/docker/daemon.json <<'EOF'
{
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "10m",
    "max-file": "3"
  },
  "storage-driver": "overlay2",
  "default-ulimits": {
    "nofile": {
      "Name": "nofile",
      "Soft": 1024,
      "Hard": 2048
    }
  }
}
EOF

sudo systemctl restart docker
docker info | grep -A1 "Registry Mirrors\|Storage Driver\|Logging Driver"
```

清理历史日志：
```bash
docker system prune -af --volumes 2>/dev/null
```

---

## 4. 砍掉不必要的常驻服务（释放 ~150MB）

```bash
sudo systemctl disable --now \
    snapd snapd.socket snapd.seeded.service \
    multipathd multipathd.socket \
    ModemManager \
    systemd-resolved \
    avahi-daemon avahi-daemon.socket \
    cups cups-browsed \
    whoopsie

sudo systemctl daemon-reload
```

不影响 docker / ssh / 业务。

---

## 5. 启动 MiniOJ

```bash
# 在仓库根目录
docker compose up -d

# 首次：灌种子题 + 创建管理员（密码会输出到 seed 容器日志）
docker compose --profile seed run --rm seed
docker compose logs seed | grep -i admin    # 找到生成的随机密码

# 验证
docker compose ps
docker stats --no-stream    # 看实时内存
free -h                     # 看系统内存（含 swap）
```

预期内存占用（4 服务全跑 + 加 swap 后）：

| 组件 | 调优前 | 调优后 |
|---|---|---|
| 系统 + dockerd | ~600M | ~400M |
| mysql:8.0 容器 | ~600M | **~250M** |
| backend 容器 | ~80M | ~30M（多阶段镜像 + -Os） |
| frontend 容器 | ~15M | ~10M |
| **常驻合计** | **~1.3G** | **~700M** |
| 留给编译 / shell | ~700M | **~1.3G** |

---

## 6. 后续日常维护

```bash
# 看每个容器实时的内存上限 vs 当前使用
docker stats

# 定期清理 docker 缓存
docker system prune -af

# 重建 backend（改了 C++ 代码后）
docker compose build backend
docker compose up -d backend

# 重置题库到种子状态
docker compose --profile seed run --rm seed --reset
```

---

## 7. 如果还是 OOM

按这个顺序降级：

1. MySQL `innodb-buffer-pool-size` → 64M（docker-compose.yml 里改）
2. backend `mem_limit` → 128m
3. 加 swap 到 8G（`swapoff /swapfile && fallocate -l 8G /swapfile && mkswap /swapfile && swapon /swapfile`）
4. 终极方案：放弃 docker 路径，走 SPEC § 9.4.3 裸机部署（再省 250MB）