#include "handleHttpEvent.h"

#include "httpServer/HttpRequest.h"
#include "httpServer/HttpResponse.h"
#include "httpServer/HttpServer.h"
#include "../../base/JsonView.h"

#include <string>
#include <iostream>

#include <fstream>
#include <sstream>

static std::string readFile(const std::string& filePath) {
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
    JsonDoc root;
    root.root()["id"].set(grpcClient::api_error_id_to_string(id));
    root.root()["message"].set(message);
    resp_json = root.toString();
}

void handleHttpEvent(const TcpConnectionPtr& conn, const HttpRequest& req, const grpcClientPtr& client) {
    if(conn->disconnected()) return;

    auto opt_connection = req.getHeader("Connection");

    const std::string& connection = *opt_connection.value();
    bool close = connection == "close" || (req.version() == HttpRequest::Version::kHttp10 && connection != "keep-alive");

    HttpResponse res{close};

    auto sendResponse = [] (const TcpConnectionPtr& conn, const HttpResponse& res) {
        std::string resJson{};
        res.appendToBuffer(resJson);
        conn->send(resJson);

        if(res.closeConnection()) conn->shutdown();
    };

    auto sendHeadResponse = [] (const TcpConnectionPtr& conn, const HttpResponse& res, std::size_t body_size) {
        std::string resJson{};
        res.appendToHeadBuffer(resJson, body_size);

        conn->send(resJson);
    };

    if(req.path() == "/api/reg") {
        int ret{0};
        std::string errmsg{""};
        
        client->rpcRegisterAsync(req, ret, errmsg, [conn, close, sendResponse] (RegisterInfo info) {
            HttpResponse res{close};

            if(info.errcode < 0) {
                std::string resJson;
                std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(info.errcode);
                if(!opt.has_value()) return;
                encodeLoginJson(*opt, info.errmsg, resJson);

                res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
                res.setStatusMessage("Bad Request");
                res.setCloseConnection(true);
                res.setBody(resJson);

                sendResponse(conn, res);

            } else {
                res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                res.setStatusMessage("OK");
                res.setCloseConnection(true);
                res.setBody("{}");

                sendResponse(conn, res);
            }
        });

        if(ret < 0) {
            std::string resJson;
            std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(ret);
            if(!opt.has_value()) return;
            encodeLoginJson(*opt, errmsg, resJson);

            res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
            res.setStatusMessage("Bad Request");
            res.setCloseConnection(true);
            res.setBody(resJson);

            sendResponse(conn, res);
        }

    } else if(req.path() == "/api/login") {
        int ret{0};
        std::string errmsg{""};

        client->rpcLoginAsync(req, ret, errmsg, [conn, close, sendResponse] (LogicInfo info) {
            HttpResponse res{close};

            if(info.errcode < 0) {
                std::string resJson;
                std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(info.errcode);
                if(!opt.has_value()) return;
                encodeLoginJson(*opt, info.errmsg, resJson);

                res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
                res.setStatusMessage("Bad Request");
                res.setCloseConnection(true);
                res.setBody(resJson);

                sendResponse(conn, res);

            } else {
                res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                res.setStatusMessage("OK");
                res.setCloseConnection(true);
                res.setCookie(info.token);
                res.setBody("{}");

                sendResponse(conn, res);
            }
        });

        if(ret < 0) {
            std::string resJson;
            std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(ret);
            if(!opt.has_value()) return;
            encodeLoginJson(*opt, errmsg, resJson);

            res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
            res.setStatusMessage("Bad Request");
            res.setCloseConnection(true);
            res.setBody(resJson);

            sendResponse(conn, res);
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

            sendResponse(conn, res);

            return;
        }

        // 如果请求的是根路径，默认指向 index.html
        if (file_path == "/") {
            file_path = "/index.html";
        }

        std::string full_path = base_dir + file_path;
        std::string content = readFile(full_path);

        // std::string content = HttpServer::StaticFilesHash[file_path];

        if (!content.empty()) {
            // 找到了对应的静态文件 (如 /assets/index-xxx.js)
            res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
            res.setStatusMessage("OK");
            res.setCloseConnection(true);
            res.setBody(content);
            res.setContentType(GetMimeType(file_path));

            sendResponse(conn, res);

        } else {
            // 找不到文件时的处理逻辑 (SPA Fallback)
            // 如果是请求 /assets/ 下的静态资源找不到，直接返回 404
            if (file_path.find("/assets/") == 0) {
                res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
                res.setStatusMessage("Not Found");
                res.setCloseConnection(true);

                sendResponse(conn, res);

            } else {
                // 对于 React 单页应用，如果用户刷新了某个前端路由（如 /chat），
                // 后端找不到这个文件，应该统一返回 index.html，交由前端 React Router 处理
                std::string index_content = readFile(base_dir + "/index.html");
                // std::string index_content = HttpServer::StaticFilesHash["/"];
                if (!index_content.empty()) {
                    res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                    res.setStatusMessage("OK");
                    res.setCloseConnection(true);
                    res.setBody(index_content);
                    res.setContentType("text/html;charset=utf-8");

                    sendResponse(conn, res);

                } else {
                    res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
                    res.setStatusMessage("Not Found");
                    res.setCloseConnection(true);

                    sendResponse(conn, res);
                }
            }
        }
    }
}