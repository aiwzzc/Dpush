#!/bin/bash

# =========================
# 路径定位
# =========================
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)

BIN_DIR="$ROOT_DIR/build/bin"
LOG_DIR="$ROOT_DIR/logs"
PID_DIR="$ROOT_DIR/pids"

mkdir -p "$LOG_DIR"
mkdir -p "$PID_DIR"

# =========================
# 启动函数
# =========================
start_service() {
    name=$1
    exec_file="$BIN_DIR/$name"

    if [ ! -f "$exec_file" ]; then
        echo "❌ $name not found: $exec_file"
        exit 1
    fi

    echo "🚀 Starting $name ..."

    nohup "$exec_file" \
        > "$LOG_DIR/${name}.log" 2>&1 &
    # "$exec_file"

    pid=$!
    echo $pid > "$PID_DIR/${name}.pid"

    echo "✅ $name started (PID=$pid)"
}

# =========================
# 启动顺序（非常重要）
# =========================
start_service "Gateway"
sleep 1

start_service "AuthServer"
sleep 1

start_service "RoomServer"
sleep 1

start_service "LogicServer"
sleep 1

echo ""
echo "🎉 All services started successfully!"
echo "📁 logs: $LOG_DIR"
echo "📁 pids: $PID_DIR"