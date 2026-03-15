#pragma once

#include "muduo/net/TcpConnection.h"
#include "grpcClient.h"
#include "producer.h"

class HttpRequest;
class GatewayPubSubManager;

void handleUpgradeEvent(const TcpConnectionPtr&, const HttpRequest&, const grpcClientPtr&, const kafkaProducerPtr&);