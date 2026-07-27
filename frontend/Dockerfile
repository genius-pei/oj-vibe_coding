FROM nginx:1.27-alpine

# 删掉默认配置
RUN rm -f /etc/nginx/conf.d/default.conf

COPY nginx.conf /etc/nginx/conf.d/default.conf

# 静态资源
COPY public/ /usr/share/nginx/html/

# 暴露端口（compose 里映射到宿主机 80）
EXPOSE 80

# nginx:alpine 镜像默认 1 worker、4MB limit_zone，足够 1-40 人并发
CMD ["nginx", "-g", "daemon off;"]