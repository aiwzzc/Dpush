# Dpush
分布式推送/聊天室系统，C++ 多服务端 + Web 前端，支持 HTTP 登录注册、WebSocket 实时消息、Redis 分发与 gRPC 微服务协作。

**亮点**
- Gateway 统一入口：HTTP API + WebSocket + 前端静态资源服务
- Auth/Room/Logic 三个 gRPC 服务，职责清晰
- Redis 做房间路由与消息分发，MySQL 存储用户数据
- Web 前端基于 React + Vite + Tailwind，内置可选 AI 助手与图片生成

**目录结构**
- `server/` C++ 服务端
- `server/GatewayServer/` 网关服务（HTTP/WS + 静态资源 + gRPC 客户端）
- `server/AuthServer/` 认证服务（MySQL + Redis）
- `server/RoomServer/` 房间服务（Redis）
- `server/LogicServer/` 消息逻辑（MySQL + Redis Streams）
- `server/proto/` gRPC 定义与生成代码
- `client/web/` 前端项目（React + Vite）

**端口与服务**
- Gateway HTTP/WS: `5005`（`server/GatewayServer/muduo/net/Acceptor.cc`）
- Auth gRPC: `5006`（`server/AuthServer/server/main.cc`）
- Room gRPC: `5007`（`server/RoomServer/server/main.cc`）
- Logic gRPC: `5008`（`server/LogicServer/server/main.cc`）

**关键依赖**
- C++20, CMake >= 3.20
- gRPC、Protobuf、jsoncpp、OpenSSL、abseil（absl）
- Redis++ / hiredis
- MySQL Connector/C++（mysqlcppconn）
- Node.js（建议 18+，用于前端）

**本地配置要点（默认写死在代码里）**
- MySQL 连接与库名  
  `server/AuthServer/server/main.cc`、`server/LogicServer/server/main.cc`  
  默认库名 `chatroom`，连接串格式 `tcp://host:port;user;password`
- Redis 地址与 DB  
  `server/AuthServer/server/main.cc`、`server/RoomServer/server/main.cc`、`server/LogicServer/server/main.cc`
- gRPC 目标地址  
  `server/GatewayServer/server/grpcClient.h`（默认 `127.0.0.1:5006/5007/5008`）
- 前端静态文件路径  
  `server/GatewayServer/server/handleHttpEvent.cc`  
  默认指向 `client/web/dist` 的绝对路径，需要和你的环境一致

## 快速开始

**1. 准备 Redis / MySQL**
- Redis 默认 `127.0.0.1:6379`
- MySQL 默认 `127.0.0.1:3306`，库名 `chatroom`
- 需要创建 `users` 表，字段至少包含 `id`、`username`、`email`、`password`、`salt`

**2. 构建前端**
```bash
cd /home/zzc/linux_test/DistributedPush/client/web
npm install
npm run build
```
构建产物会输出到 `client/web/dist`，由 Gateway 直接读取并对外提供。

**3. 构建后端**
```bash
# AuthServer
cd /home/zzc/linux_test/DistributedPush/server/AuthServer
cmake -S . -B build
cmake --build build -j

# RoomServer
cd /home/zzc/linux_test/DistributedPush/server/RoomServer
cmake -S . -B build
cmake --build build -j

# LogicServer
cd /home/zzc/linux_test/DistributedPush/server/LogicServer
cmake -S . -B build
cmake --build build -j

# GatewayServer
cd /home/zzc/linux_test/DistributedPush/server/GatewayServer
cmake -S . -B build
cmake --build build -j
```

**4. 启动服务**
```bash
/home/zzc/linux_test/DistributedPush/server/AuthServer/build/bin/AuthServer
/home/zzc/linux_test/DistributedPush/server/RoomServer/build/bin/RoomServer
/home/zzc/linux_test/DistributedPush/server/LogicServer/build/bin/LogicServer
/home/zzc/linux_test/DistributedPush/server/GatewayServer/build/bin/Gateway
```
然后访问 `http://<服务器IP>:5005`。

## API 概览
**HTTP**
- `POST /api/reg`  注册（`username`、`email`、`password`）
- `POST /api/login` 登录（`email`、`password`）
登录成功会下发 `sid` Cookie，用于 WebSocket 认证。

**WebSocket**
- 前端默认连接 `ws://<host>:5005/ws`  
Gateway 不校验路径，仅在 `Upgrade: websocket` 时进行握手，验证 `sid` 后建立连接。

## gRPC 服务
协议定义位于 `server/proto/`：
- `AuthServer` 登录/注册/验证
- `RoomServer` 用户房间列表与入房
- `LogicServer` 拉取消息、发送消息、清游标

如需重新生成 `*.pb.*`：
```bash
protoc -I server/proto \
  --cpp_out=server/proto \
  --grpc_out=server/proto \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
  server/proto/auth.proto server/proto/logic.proto server/proto/room.proto
```

## AI 功能说明（可选）
前端支持：
- `@ai` 触发文本回复
- 图片生成（使用 `gemini-3.1-flash-image-preview`）
需要通过 `ApiKeyPrompt` 选择/配置 API Key。

---
如果你希望我帮你补一份建库 SQL 或把配置抽成统一的配置文件，也可以直接告诉我。 
