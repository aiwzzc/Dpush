#pragma once

#include "muduo/net/TcpConnection.h"
#include "grpcClient.h"

class HttpRequest;

void handleHttpEvent(const TcpConnectionPtr&, const HttpRequest&, const grpcClientPtr&);