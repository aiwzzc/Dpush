// #include "handleHttpEvent.h"

// #include "HttpRequest.h"
// #include "HttpResponse.h"
// #include "HttpServer.h"
// #include "yyjson/JsonView.h"

// #include <string>
// #include <iostream>

// #include <fstream>
// #include <sstream>

// static std::string readFile(const std::string& filePath) {
//     std::ifstream file(filePath);
//     if (!file) {
//         return "";
//     }
//     std::stringstream buffer;
//     buffer << file.rdbuf();
//     return buffer.str();
// }

// // 根据文件后缀名推断 MIME 类型
// std::string GetMimeType(const std::string& path) {
//     if (path.find(".html") != std::string::npos) return "text/html;charset=utf-8";
//     if (path.find(".js") != std::string::npos) return "application/javascript;charset=utf-8";
//     if (path.find(".css") != std::string::npos) return "text/css;charset=utf-8";
//     if (path.find(".png") != std::string::npos) return "image/png";
//     if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
//     if (path.find(".svg") != std::string::npos) return "image/svg+xml";
//     if (path.find(".json") != std::string::npos) return "application/json;charset=utf-8";
//     return "application/octet-stream";
// }

// void encodeErrorLoginJson(grpcClient::api_error_id id, const std::string& message, std::string& resp_json) {
//     JsonDoc root;
//     root.root()["id"].set(grpcClient::api_error_id_to_string(id));
//     root.root()["message"].set(message);
//     resp_json = root.toString();
// }

// void encodeLoginJsonBody(LogicInfo& info, std::string& resp_json) {
//     JsonDoc root;
//     root.root()["userinfo"]["userid"].set(info.userid);
//     root.root()["userinfo"]["username"].set(info.username);
//     resp_json = root.toString();
// }

// void encodeCreateSessionJsonBody(int32_t userid, int64_t roomid, std::string& resp_json) {
//     JsonDoc root;
//     root.root()["userid"].set(userid);
//     root.root()["roomid"].set(std::to_string(roomid));
//     resp_json = root.toString();
// }

// void encodeErrorCreateSessionJsonBody(const std::string& errmsg, std::string& resp_json) {
//     JsonDoc root;
//     root.root()["errormsg"].set(errmsg);
//     resp_json = root.toString();
// }

// void sendResponse(const TcpConnectionPtr& conn, const HttpResponse& res) {
//     std::string resJson{};
//     res.appendToBuffer(resJson);
//     conn->send(resJson);

//     if(res.closeConnection()) conn->shutdown();
// }

// HttpEventRegister::HttpEventRegister() {
//     registerEvent({"/api/reg", HttpEventHandlers::register_api});
//     registerEvent({"/api/login", HttpEventHandlers::login_api});
//     registerEvent({"/api/joinsession", HttpEventHandlers::joinSession_api});
//     registerEvent({"/api/createsession", HttpEventHandlers::createSession_api});
// }

// HttpEventRegister& HttpEventRegister::getInstance() {
//     static HttpEventRegister instance;

//     return instance;
// }

// void HttpEventRegister::registerEvent(HttpEventDef def)
// { HttpEventRegister::event_table_[def.name_] = def; }

// HttpEventDef* HttpEventRegister::loop_up(const std::string& name) {
//     auto it = this->event_table_.find(name);

//     return it == this->event_table_.end() ? nullptr : &it->second;
// }

// HttpEventHandlers& HttpEventHandlers::getInstance() {
//     static HttpEventHandlers instance;
//     return instance;
// }

// void HttpEventHandlers::register_api(HttpEventContext& ctx) {
//     int ret{0};
//     std::string errmsg{""};
    
//     ctx.rpc_->rpcRegisterAsync(ctx.req_, ret, errmsg, [ctx = std::move(ctx)] (RegisterInfo info) {
//         HttpResponse res{ctx.close_};

//         if(info.errcode < 0) {
//             std::string resJson;
//             std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(info.errcode);
//             if(!opt.has_value()) return;
//             encodeErrorLoginJson(*opt, info.errmsg, resJson);

//             res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
//             res.setStatusMessage("Bad Request");
//             res.setCloseConnection(true);
//             res.setBody(resJson);

//             sendResponse(ctx.conn_, res);

//         } else {
//             res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
//             res.setStatusMessage("OK");
//             res.setCloseConnection(true);
//             res.setBody("{}");

//             sendResponse(ctx.conn_, res);
//         }
//     });

//     if(ret < 0) {
//         std::string resJson;
//         std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(ret);
//         if(!opt.has_value()) return;
//         encodeErrorLoginJson(*opt, errmsg, resJson);

//         ctx.res_.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
//         ctx.res_.setStatusMessage("Bad Request");
//         ctx.res_.setCloseConnection(true);
//         ctx.res_.setBody(resJson);

//         sendResponse(ctx.conn_, ctx.res_);
//     }
// }

// void HttpEventHandlers::login_api(HttpEventContext& ctx) {
//     int ret{0};
//     std::string errmsg{""};

//     ctx.rpc_->rpcLoginAsync(ctx.req_, ret, errmsg, [ctx = std::move(ctx)] (LogicInfo info) {
//         HttpResponse res{ctx.close_};

//         if(info.errcode < 0) {
//             std::string resJson;
//             std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(info.errcode);
//             if(!opt.has_value()) return;
//             encodeErrorLoginJson(*opt, info.errmsg, resJson);

//             res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
//             res.setStatusMessage("Bad Request");
//             res.setCloseConnection(true);
//             res.setBody(resJson);

//             sendResponse(ctx.conn_, res);

//         } else {
//             std::string resJson;
//             encodeLoginJsonBody(info, resJson);
//             res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
//             res.setStatusMessage("OK");
//             res.setCloseConnection(true);
//             res.setCookie(info.token);
//             res.setBody(resJson);

//             sendResponse(ctx.conn_, res);
//         }
//     });

//     if(ret < 0) {
//         std::string resJson;
//         std::optional<grpcClient::api_error_id> opt = grpcClient::to_api_error_id(ret);
//         if(!opt.has_value()) return;
//         encodeErrorLoginJson(*opt, errmsg, resJson);

//         ctx.res_.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
//         ctx.res_.setStatusMessage("Bad Request");
//         ctx.res_.setCloseConnection(true);
//         ctx.res_.setBody(resJson);

//         sendResponse(ctx.conn_, ctx.res_);
//     }
// }

// void HttpEventHandlers::joinSession_api(HttpEventContext& ctx) {
//     JsonDoc root;

//     if(!root.parse(ctx.req_.body().c_str(), ctx.req_.body().size())) {

//     }

//     if(!root.root().isMember("userid") || !root.root()["userid"].isInt() ||
//        !root.root().isMember("roomname") || !root.root()["roomname"].isString()) {

//     }

//     int userid = root.root()["userid"].asInt();
//     std::string roomname = root.root()["roomname"].asString();

//     ctx.rpc_->rpcJoinSessionAsync(userid, roomname, [userid, ctx = std::move(ctx)] (int code, const std::string& err_msg, int64_t roomid) {
//         HttpResponse res{ctx.close_};

//         if(code < 0) {
//             std::string resJson;
//             res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
//             res.setStatusMessage("Bad Request");
//             res.setCloseConnection(true);
//             encodeErrorCreateSessionJsonBody(err_msg, resJson);
//             res.setBody(resJson); // modify send err msg

//             sendResponse(ctx.conn_, res);

//         } else {
//             std::string resJson;
//             res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
//             res.setStatusMessage("OK");
//             res.setCloseConnection(true);
//             encodeCreateSessionJsonBody(userid, roomid, resJson);
//             res.setBody(resJson);

//             sendResponse(ctx.conn_, res);
//         }
//     });
// }

// void HttpEventHandlers::createSession_api(HttpEventContext& ctx) {
//     JsonDoc root;

//     if(!root.parse(ctx.req_.body().c_str(), ctx.req_.body().size())) {

//     }

//     if(!root.root().isMember("userid") || !root.root()["userid"].isInt() ||
//        !root.root().isMember("roomname") || !root.root()["roomname"].isString()) {

//     }

//     int userid = root.root()["userid"].asInt();
//     std::string roomname = root.root()["roomname"].asString();

//     ctx.rpc_->rpcCreateSessionAsync(userid, roomname, [userid, ctx = std::move(ctx)] (int32_t code, const std::string& errmsg, int64_t roomid) {
//         HttpResponse res{ctx.close_};

//         if(code < 0) {
//             std::string resJson;
//             res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
//             res.setStatusMessage("Bad Request");
//             res.setCloseConnection(true);
//             encodeErrorCreateSessionJsonBody(errmsg, resJson);
//             res.setBody(resJson);

//             sendResponse(ctx.conn_, res);

//         } else {
//             std::string resJson;
//             res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
//             res.setStatusMessage("OK");
//             res.setCloseConnection(true);
//             encodeCreateSessionJsonBody(userid, roomid, resJson);
//             res.setBody(resJson);

//             sendResponse(ctx.conn_, res);
//         }
//     });
// }

// void HttpEventHandlers::handleHttpEvent(const TcpConnectionPtr& conn, const HttpRequest& req, 
//     const grpcClientPtr& client) {
//     if(conn->disconnected()) return;

//     HttpEventRegister& register_ = HttpEventRegister::getInstance();

//     auto opt_connection = req.getHeader("Connection");

//     const std::string& connection = *opt_connection.value();
//     bool close = connection == "close" || (req.version() == HttpRequest::Version::kHttp10 && connection != "keep-alive");

//     HttpResponse res{close};

//     HttpEventContext ctx{conn, client, req, close, res};

//     HttpEventDef* def = register_.loop_up(req.path());
//     if(def) def->execute_(ctx);

//     else {
//         // 注意：这里需要指向您前端项目执行 npm run build 后生成的 dist 目录
//         std::string base_dir = "/home/zzc/linux_test/DistributedPush/client/web/dist";
//         std::string file_path = req.path();

//         // 防止路径穿越攻击 (例如请求 /../../../etc/passwd)
//         if (file_path.find("..") != std::string::npos) {
//             res.setStatusCode(HttpResponse::HttpStatusCode::k403Forbidden);
//             res.setStatusMessage("Forbidden");
//             res.setCloseConnection(true);

//             sendResponse(conn, res);

//             return;
//         }

//         // 如果请求的是根路径，默认指向 index.html
//         if (file_path == "/") {
//             file_path = "/index.html";
//         }

//         std::string full_path = base_dir + file_path;
//         std::string content = readFile(full_path);

//         // std::string content = HttpServer::StaticFilesHash[file_path];

//         if (!content.empty()) {
//             // 找到了对应的静态文件 (如 /assets/index-xxx.js)
//             res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
//             res.setStatusMessage("OK");
//             res.setCloseConnection(true);
//             res.setBody(content);
//             res.setContentType(GetMimeType(file_path));

//             sendResponse(conn, res);

//         } else {
//             // 找不到文件时的处理逻辑 (SPA Fallback)
//             // 如果是请求 /assets/ 下的静态资源找不到，直接返回 404
//             if (file_path.find("/assets/") == 0) {
//                 res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
//                 res.setStatusMessage("Not Found");
//                 res.setCloseConnection(true);

//                 sendResponse(conn, res);

//             } else {
//                 // 对于 React 单页应用，如果用户刷新了某个前端路由（如 /chat），
//                 // 后端找不到这个文件，应该统一返回 index.html，交由前端 React Router 处理
//                 std::string index_content = readFile(base_dir + "/index.html");
//                 // std::string index_content = HttpServer::StaticFilesHash["/"];
//                 if (!index_content.empty()) {
//                     res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
//                     res.setStatusMessage("OK");
//                     res.setCloseConnection(true);
//                     res.setBody(index_content);
//                     res.setContentType("text/html;charset=utf-8");

//                     sendResponse(conn, res);

//                 } else {
//                     res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
//                     res.setStatusMessage("Not Found");
//                     res.setCloseConnection(true);

//                     sendResponse(conn, res);
//                 }
//             }
//         }
//     }
// }