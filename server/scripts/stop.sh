#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)

PID_DIR="$ROOT_DIR/pids"

stop_service() {
    name=$1
    pid_file="$PID_DIR/${name}.pid"

    if [ -f "$pid_file" ]; then
        pid=$(cat "$pid_file")
        echo "🛑 Stopping $name (PID=$pid)"
        kill $pid
        rm -f "$pid_file"
    else
        echo "⚠️ $name not running"
    fi
}

stop_service "Gateway"
stop_service "RoomServer"
stop_service "LogicServer"
stop_service "AuthServer"

echo "✅ All services stopped."