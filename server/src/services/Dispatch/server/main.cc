#include "DispatchServer.h"

int main() {

    dispatchServer server{"127.0.0.1:2379"};

    server.start();

    return 0;
}