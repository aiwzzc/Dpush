#include "JsonView.h"
#include <string>
#include <iostream>

void encodeLoginJsonBody(std::string& resp_json) {
    JsonDoc root;
    root.root()["userinfo"]["userid"].set(1);
    root.root()["userinfo"]["username"].set("king");
    resp_json = root.toString();
}

int main() {

    std::string res;
    encodeLoginJsonBody(res);

    std::cout << res << std::endl;

    return 0;
}