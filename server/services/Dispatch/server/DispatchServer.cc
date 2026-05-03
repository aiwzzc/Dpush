#include "DispatchServer.h"
#include "yyjson/JsonView.h"

dispatchServer::dispatchServer(const std::string& etcd_url) :
etcd_url_(etcd_url) {
    this->etcd_client_ = std::make_shared<etcd::Client>(this->etcd_url_);
    this->server_ = std::make_unique<httplib::Server>();

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;
    connection_options.db = 1;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 3;

    this->redisPool_ = std::make_unique<sw::redis::Redis>(connection_options, pool_options);
}

dispatchServer::~dispatchServer() {
    this->running_ = false;
    if(this->sync_worker_.joinable()) this->sync_worker_.join();
}

void dispatchServer::BackendSyncTask() {
    while(this->running_) {
        std::unordered_map<std::string, std::string> gateway_loads;
        this->redisPool_->hgetall("gateway:load", std::inserter(gateway_loads, gateway_loads.begin()));

        std::vector<GatewayInfo> temp_list;
        JsonDoc root;

        for(auto it = gateway_loads.begin(); it != gateway_loads.end(); ++it) {
            std::string load_json = it->second;

            if(root.parse(load_json.data(), load_json.size())) {
                int score = root.root()["conn"].asInt();
                temp_list.emplace_back(score, it->first);
            }
        }

        std::sort(temp_list.begin(), temp_list.end(), 
        [] (const GatewayInfo& a, const GatewayInfo& b) {
            return a.score < b.score;
        });

        {
            std::unique_lock<std::shared_mutex> lock(this->cache_mutex_);
            this->cached_gateways_ = std::move(temp_list);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

void dispatchServer::etcdWatcherCallback(const etcd::Response& response) {
    for(const auto& event : response.events()) {
        std::string key = event.kv().key();

        std::string ip_port = key.substr(this->watch_prefix_.length());

        if(event.event_type() == etcd::Event::EventType::PUT) {
            {
                std::unique_lock<std::shared_mutex> lock(this->etcd_conns_mutex_);
                this->etcd_conns_.insert(ip_port);
            }

        } else if(event.event_type() == etcd::Event::EventType::DELETE_) {
            {
                std::unique_lock<std::shared_mutex> lock(this->etcd_conns_mutex_);
                this->etcd_conns_.erase(ip_port);
            }
        }
    }
}

void dispatchServer::httpEventCallback(const httplib::Request& req, httplib::Response& res) {
    std::vector<std::string> valid_urls;

    {
        std::shared_lock<std::shared_mutex> lock(this->cache_mutex_);

        if (this->cached_gateways_.empty()) {
            res.status = 503;
            res.set_content(R"({"error": "No gateways available"})", "application/json");
            return;
        }

        int count = std::min<std::size_t>(2, this->cached_gateways_.size());

        int index{0};
        while(valid_urls.size() < count && index < this->cached_gateways_.size()) {
            const std::string& gateway_url = this->cached_gateways_[index++].gateway_url;
            
            {
                std::shared_lock<std::shared_mutex> conns_lock(this->etcd_conns_mutex_);
                if(this->etcd_conns_.contains(gateway_url)) {
                    valid_urls.push_back(gateway_url);
                }
            }
        }
    }

    std::string res_json = R"({"code": 0, "urls": [)";
    for (size_t i = 0; i < valid_urls.size(); ++i) {
        res_json += R"("ws://)" + valid_urls[i] + R"(")";
        if (i < valid_urls.size() - 1) {
            res_json += ",";
        }
    }
    res_json += "]}";

    res.set_content(res_json, "application/json");
}

void dispatchServer::start() {
    auto response = this->etcd_client_->ls(this->watch_prefix_).get();

    for(int i = 0; i < response.keys().size(); ++i) {
        const std::string& ip_port = response.value(i).as_string();

        {
            std::unique_lock<std::shared_mutex> lock(this->etcd_conns_mutex_);
            auto it = this->etcd_conns_.find(ip_port);
            if(it == this->etcd_conns_.end()) this->etcd_conns_.insert(ip_port);
        }
    }

    this->etcd_watcher_ = std::make_unique<etcd::Watcher>(
        this->etcd_url_,
        this->watch_prefix_,
        [this] (const etcd::Response& response) {
            this->etcdWatcherCallback(response);
        },
        true
    );

    this->sync_worker_ = std::thread([this] () {
        this->BackendSyncTask();
    });

    this->server_->Get("/get_gateway", [this] (const httplib::Request& req, httplib::Response& res) {
        this->httpEventCallback(req, res);
    });

    this->server_->listen("192.168.183.130", 5000);
}