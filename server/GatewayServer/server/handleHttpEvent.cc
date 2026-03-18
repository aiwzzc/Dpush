#include "handleHttpEvent.h"

#include "httpServer/HttpRequest.h"
#include "httpServer/HttpResponse.h"

#include <string>
#include <iostream>

#include <fstream>
#include <sstream>
#include <jsoncpp/json/json.h>
 
// 读取文件内容
std::string readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// 根据文件后缀名推断 MIME 类型
std::string GetMimeType(const std::string& path) {
    if (path.find(".html") != std::string::npos) return "text/html;charset=utf-8";
    if (path.find(".js") != std::string::npos) return "application/javascript;charset=utf-8";
    if (path.find(".css") != std::string::npos) return "text/css;charset=utf-8";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (path.find(".svg") != std::string::npos) return "image/svg+xml";
    if (path.find(".json") != std::string::npos) return "application/json;charset=utf-8";
    return "application/octet-stream";
}

void encodeLoginJson(grpcClient::api_error_id id, const std::string& message, std::string& resp_json) {
    Json::Value root;
    root["id"] = grpcClient::api_error_id_to_string(id);
    root["message"] = message;
    Json::FastWriter writer;
    resp_json = writer.write(root);
}
#include <uuid/uuid.h>
static std::string generateUUID() {
    uuid_t uuid;
    uuid_generate_time_safe(uuid);  //调用uuid的接口
 
    char uuidStr[40] = {0};
    uuid_unparse(uuid, uuidStr);     //调用uuid的接口
 
    return std::string(uuidStr);
}

void handleHttpEvent(const TcpConnectionPtr& conn, const HttpRequest& req, const grpcClientPtr& client) {
    if(conn->disconnected()) return;

    const std::string& connection = req.getHeader("Connection");
    bool close = connection == "close" || (req.version() == HttpRequest::Version::kHttp10 && connection != "keep-alive");

    HttpResponse res{close};

    auto sendResponse = [&conn] (const HttpResponse& res) {
        std::string resJson{};
        res.appendToBuffer(resJson);
        conn->send(resJson);

        if(res.closeConnection()) conn->shutdown();
    };

    if(req.path() == "/api/reg") {
        int ret{0};
        std::string errmsg{""};
        client->rpcRegister(req, res, ret, errmsg);

        if(ret < 0) {
            std::string resJson;
            std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(ret);
            if(!opt.has_value()) return;
            encodeLoginJson(*opt, errmsg, resJson);

            res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
            res.setStatusMessage("Bad Request");
            res.setCloseConnection(true);
            res.setBody(resJson);

            sendResponse(res);

        } else {
            res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
            res.setStatusMessage("OK");
            res.setCloseConnection(true);
            res.setBody("{}");

            sendResponse(res);
        }

    } else if(req.path() == "/api/login") {
        int ret{0};
        std::string errmsg{""};
        client->rpcLogin(req, res, ret, errmsg);

        if(ret < 0) {
            std::string resJson;
            std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(ret);
            if(!opt.has_value()) return;
            encodeLoginJson(*opt, errmsg, resJson);

            res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
            res.setStatusMessage("Bad Request");
            res.setCloseConnection(true);
            res.setBody(resJson);

            sendResponse(res);

        } else {
            res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
            res.setStatusMessage("OK");
            res.setCloseConnection(true);
            res.setCookie(generateUUID());
            res.setBody("{}");

            sendResponse(res);
        }

    } else {
        // 注意：这里需要指向您前端项目执行 npm run build 后生成的 dist 目录
        std::string base_dir = "/home/zzc/linux_test/DistributedPush/client/web/dist";
        std::string file_path = req.path();

        // 防止路径穿越攻击 (例如请求 /../../../etc/passwd)
        if (file_path.find("..") != std::string::npos) {
            res.setStatusCode(HttpResponse::HttpStatusCode::k403Forbidden);
            res.setStatusMessage("Forbidden");
            res.setCloseConnection(true);

            sendResponse(res);

            return;
        }

        // 如果请求的是根路径，默认指向 index.html
        if (file_path == "/") {
            file_path = "/index.html";
        }

        std::string full_path = base_dir + file_path;
        std::string content = readFile(full_path);

        if (!content.empty()) {
            // 找到了对应的静态文件 (如 /assets/index-xxx.js)
            res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
            res.setStatusMessage("OK");
            res.setCloseConnection(true);
            res.setBody(content);
            res.setContentType(GetMimeType(file_path));

            sendResponse(res);

        } else {
            // 找不到文件时的处理逻辑 (SPA Fallback)
            // 如果是请求 /assets/ 下的静态资源找不到，直接返回 404
            if (file_path.find("/assets/") == 0) {
                res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
                res.setStatusMessage("Not Found");
                res.setCloseConnection(true);

                sendResponse(res);

            } else {
                // 对于 React 单页应用，如果用户刷新了某个前端路由（如 /chat），
                // 后端找不到这个文件，应该统一返回 index.html，交由前端 React Router 处理
                std::string index_content = readFile(base_dir + "/index.html");
                if (!index_content.empty()) {
                    res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                    res.setStatusMessage("OK");
                    res.setCloseConnection(true);
                    res.setBody(index_content);
                    res.setContentType("text/html;charset=utf-8");

                    sendResponse(res);

                } else {
                    res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
                    res.setStatusMessage("Not Found");
                    res.setCloseConnection(true);

                    sendResponse(res);
                }
            }
        }
    }
}