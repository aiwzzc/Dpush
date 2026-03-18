#include "AuthServiceImpl.h"

struct QueryAwaiter {

    MySQLConnPool* pool_;
    std::string sql_;
    SQLOperation::SQLType type_;
    SQLResult result_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->pool_->query(this->sql_, this->type_, [this, handle] (SQLResult res) {
            if(type_ == SQLOperation::SQLType::QUERY) this->result_ = res;

            handle.resume();
        });
    }

    SQLResult await_resume() { return this->result_; }
};

struct UpdateAwaiter {

    MySQLConnPool* pool_;
    std::string sql_;
    SQLOperation::SQLType type_;
    std::string status_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->pool_->query(this->sql_, this->type_, [this, handle] (SQLResult res) {
            if(!res.empty() && !res[0].empty()) this->status_.assign(res[0][0]);

            handle.resume();
        });
    }

    std::string await_resume() { return this->status_; }
};

struct redisVerifyAwaiter {

    sw::redis::Redis* redis_;
    std::string_view token_;
    sw::redis::OptionalString db_email_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([this, handle]() {
            this->db_email_ = this->redis_->get(this->token_);
            handle.resume();
        }).detach(); 
    }

    sw::redis::OptionalString await_resume() { return this->db_email_; }

};

QueryAwaiter async_query_for_coro(MySQLConnPool* pool, const std::string& sql, SQLOperation::SQLType type) {
    return QueryAwaiter{pool, sql, type, {}};
}

UpdateAwaiter async_update_for_coro(MySQLConnPool* pool, const std::string& sql, SQLOperation::SQLType type) {
    return UpdateAwaiter{pool, sql, type, ""};
}

redisVerifyAwaiter async_redisVerify_for_coro(sw::redis::Redis* redis, std::string_view token) {
    return redisVerifyAwaiter{redis, token};
}

template <typename... Args>
std::string FormatString(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

std::string generateUUID() {
    uuid_t uuid;
    uuid_generate_time_safe(uuid);  //调用uuid的接口
 
    char uuidStr[40] = {0};
    uuid_unparse(uuid, uuidStr);     //调用uuid的接口
 
    return std::string(uuidStr);
}

// std::string RandomString(const int len) {
//     /*初始化*/
//     std::string str; /*声明用来保存随机字符串的str*/
//     char c;     /*声明字符c，用来保存随机生成的字符*/
//     int idx;    /*用来循环的变量*/
//     /*循环向字符串中添加随机生成的字符*/
//     for (idx = 0; idx < len; idx++) {
//         /*rand()%26是取余，余数为0~25加上'a',就是字母a~z,详见asc码表*/
//         c = 'a' + rand() % 26;
//         str.push_back(c); /*push_back()是string类尾插函数。这里插入随机字符c*/
//     }
//     return str; /*返回生成的随机字符串*/
// }

std::string RandomString(const int len) {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution('a', 'z');
    std::string str;
    for (int idx = 0; idx < len; idx++) {
        str.push_back(static_cast<char>(distribution(generator)));
    }
    return str;
}

void ApiSetCookie(sw::redis::Redis& redis, std::string email, std::string& cookie) {
    cookie.assign(generateUUID());
    redis.setex(cookie, 86400, email);
}

Task<int> verifyUserPassword(MySQLConnPool* pool, const std::string& email, 
    const std::string& password, int32_t& userid, std::string& username) {
    std::string strSql = FormatString("select id, username, password, salt from users where email = '%s'", email.c_str());

    SQLResult res = co_await async_query_for_coro(pool, strSql, SQLOperation::SQLType::QUERY);

    if(res.empty()) co_return -2;

    for(const auto& rows : res) {
        userid = std::stoi(rows[0]);
        username.assign(rows[1]);
        const std::string db_password = rows[2];
        const std::string db_salt = rows[3];

        MD5 md5(password + db_salt);
        const std::string client_password = md5.toString();

        if(db_password != client_password) co_return -3;
    }

    co_return 0;
}

AuthService::AuthService(MySQLConnPool* pool, sw::redis::Redis* redis) : mysql_pool_(pool), redis_pool_(redis) {}
AuthService::~AuthService() = default;

grpc::ServerUnaryReactor* AuthService::Login(grpc::CallbackServerContext* context, const auth::LoginRequest* request, 
    auth::LoginResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DoLoginAsync(request, response, reactor);

    return reactor;
}

DetachedTask AuthService::DoLoginAsync(const auth::LoginRequest* request, auth::LoginResponse* response, 
    grpc::ServerUnaryReactor* reactor) {
    
    const std::string& email = request->email();
    const std::string& password = request->password();
    int32_t userid{-1};
    std::string username;

    int ret = co_await verifyUserPassword(this->mysql_pool_, email, password, userid, username);

    if(ret < 0) {
        if(ret == -2) {
            response->set_code(-4);
            response->set_error_msg("邮箱不存在");

        } else if(ret == -3) {
            response->set_code(-5);
            response->set_error_msg("密码错误");
        }

        // reactor->Finish(grpc::Status::);

    } else {
        std::string token;
        ApiSetCookie(*this->redis_pool_, email, token);
        response->set_token(token);
        response->set_userid(userid);
        response->set_username(username);
    }

    reactor->Finish(grpc::Status::OK);
}

grpc::ServerUnaryReactor* AuthService::Register(grpc::CallbackServerContext* context, const auth::RegisterRequest* request, 
    auth::RegisterResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DoRegisterAsync(request, response, reactor);

    return reactor;
}

DetachedTask AuthService::DoRegisterAsync(const auth::RegisterRequest* request, auth::RegisterResponse* response, 
    grpc::ServerUnaryReactor* reactor) {

    const std::string& username = request->username();
    const std::string& email = request->email();
    const std::string& password = request->password();
    response->set_code(1);

    // 随机数生成盐值
    std::string Salt = RandomString(16);
    MD5 md5(password + Salt);
    std::string passwordHash = md5.toString();

    std::string sql =
    "insert into users(username,email,password,salt) values('" +
    username + "','" +
    email + "','" +
    passwordHash + "','" +
    Salt + "')";

    std::string res = co_await async_update_for_coro(this->mysql_pool_, sql, SQLOperation::SQLType::UPDATE);

    if(res == "-1001") {
        response->set_code(-2);
        response->set_error_msg("用户名已存在");

    } else if(res == "-1002") {
        response->set_code(-3);
        response->set_error_msg("邮箱已存在");

    } else if(res == "-1003") {
        response->set_code(-1);
        response->set_error_msg("未知错误");
    }

    reactor->Finish(grpc::Status::OK);
}

grpc::ServerUnaryReactor* AuthService::Verify(grpc::CallbackServerContext* context, const auth::VerifyTokenRequest* request, 
    auth::VerifyTokenResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DoVerifyAsync(request, response, reactor);

    return reactor;

}

DetachedTask AuthService::DoVerifyAsync(const auth::VerifyTokenRequest* request, auth::VerifyTokenResponse* response, 
    grpc::ServerUnaryReactor* reactor) {

    // sw::redis::OptionalString db_email = this->redis_pool_->get(request->token());
    sw::redis::OptionalString db_email = co_await async_redisVerify_for_coro(this->redis_pool_, request->token());
    if(!db_email) {
        response->set_code(-1);
        response->set_error_msg("token error");

        reactor->Finish(grpc::Status::OK);
        co_return;
    }

    std::string strSql = FormatString("select id, username from users where email='%s'", db_email.value().c_str());
    
    SQLResult res = co_await async_query_for_coro(this->mysql_pool_, strSql, SQLOperation::SQLType::QUERY);

    if(res.empty() || res[0].empty()) {
        response->set_code(-2);
        response->set_error_msg("email error");

        reactor->Finish(grpc::Status::OK);
        co_return;
    }

    response->set_userid(std::stoi(res[0][0]));
    response->set_username(res[0][1]);

    reactor->Finish(grpc::Status::OK);
}