#include "MySQLConn.h"
#include "BlockQueue.h"
#include "SQLOperation.h"
#include "MySQLWorker.h"
#include "MySQLConnPool.h"
#include <vector>

#include <mysql-cppconn/jdbc/mysql_driver.h>
#include <mysql-cppconn/jdbc/mysql_connection.h>
#include <mysql-cppconn/jdbc/cppconn/statement.h>
#include <mysql-cppconn/jdbc/cppconn/resultset.h>
#include <mysql-cppconn/jdbc/cppconn/exception.h>

static std::vector<std::string_view> split(std::string_view msg, char de) {
    std::vector<std::string_view> tokens;
    int pos = 0, next = 0;

    while((next = msg.find(de, pos)) != std::string::npos) {
        tokens.push_back(msg.substr(pos, next - pos));
        pos = next + 1;
    }

    if(pos < msg.size()) tokens.push_back(msg.substr(pos));

    return tokens;
}

MySQLConnInfo::MySQLConnInfo(const std::string& info, const std::string& db) {
    auto tokens = split(info, ';');
    if(tokens.size() != 3) return;
    
    url.assign(tokens[0]);
    user.assign(tokens[1]);
    password.assign(tokens[2]);
    database.assign(db);
}

MySQLConnInfo::~MySQLConnInfo() = default;

MySQLConn::MySQLConn(MySQLConnPool* manager, const std::string& info, const std::string& db, BlockQueue<SQLOperation*>& ask_queue) : 
manager_(manager), info_(info, db) {
    this->worker_ = new MySQLWorker(this, ask_queue);
    this->worker_->start();
}

MySQLConn::~MySQLConn() {
    if(this->worker_) {
        this->worker_->stop();

        delete this->worker_;
        this->worker_ = nullptr;
    }

    this->conn_->close();
}

MySQLConnPool* MySQLConn::get_manager() { return this->manager_; }

int MySQLConn::open() {
    int err = 0;

    try {
        this->driver_ = sql::mysql::get_driver_instance();

        this->conn_ = std::unique_ptr<sql::Connection>(this->driver_->connect(this->info_.url, this->info_.user, this->info_.password));

        this->conn_->setSchema(this->info_.database);

    } catch(sql::SQLException &e) {
        err = e.getErrorCode();
        std::cerr << "MySQL error: " << e.what() << std::endl;
    }

    return err;
}

void MySQLConn::close() {
    if(this->conn_) this->conn_->close();
    
}

SQLResult MySQLConn::query(const std::string& sql) {

    SQLResult result_container;

    try {

        std::unique_ptr<sql::Statement> stmt(this->conn_->createStatement());

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));

        sql::ResultSetMetaData* data = res->getMetaData();
        int column_count = data->getColumnCount();

        while(res->next()) {
            SQLRow row_result;

            for(int i = 1; i <= column_count; ++i) {
                if(res->isNull(i)) row_result.push_back("NULL");
                else row_result.push_back(res->getString(i));
            }

            result_container.push_back(std::move(row_result));
        }

    } catch(sql::SQLException& e) {
        std::cerr << "MySQL error: " << e.what() << std::endl;
    }

    return result_container;
}

int MySQLConn::update(const std::string& sql) {
    try {

        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());

        int affected_rows = stmt->executeUpdate(sql);

        return affected_rows;

    } catch(sql::SQLException& e) {
        std::cerr << "MySQL error code: " << e.getErrorCode() 
                  << ", State: " << e.getSQLState() 
                  << ", msg: " << e.what() << std::endl;

        if(e.getErrorCode() == 1062) {
            std::string err_msg = e.what();

            if(err_msg.find("username") != std::string::npos) {
                return -1001;

            } else if(err_msg.find("email") != std::string::npos) {
                return -1002;

            } else {
                return -1003; // 位置字段重复
            }
        }
    }

    return -1;
}