#pragma once

#include "muduo/net/TcpConnection.h"
#include "grpcClient.h"

class HttpRequest;

using muduo::net::TcpConnectionPtr;

void handleHttpEvent(const TcpConnectionPtr&, const HttpRequest&, const grpcClientPtr&);