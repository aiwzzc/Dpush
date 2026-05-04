#include "heartbeatManager.h"
#include "chat_generated.h"

Entry::Entry(const WsSessionPtr& conn) : conn_(conn) {}

Entry::~Entry() {
    auto conn = this->conn_.lock();
    if(conn) {
        conn->forceClose();
    }
}

thread_local std::unique_ptr<heartbeatManager> t_heartbeatManager_ptr = nullptr;

heartbeatManager::heartbeatManager() : wheel_(100), current_index_(0) {};

void heartbeatManager::onTimerTick() {
    this->current_index_ = (this->current_index_ + 1) % this->wheel_.size();

    this->wheel_[this->current_index_] = Bucket{};
}

void heartbeatManager::onMessagePing(const WsSessionPtr& webconn, int64_t ts,
    const std::function<void(const std::string&)>& callback) {
    auto* context = webconn->getMutableContext();
    EntryPtr entry;

    if(context == nullptr) {
        entry = std::make_shared<Entry>(webconn);
        webconn->setContext(std::weak_ptr<Entry>(entry));
        context = webconn->getMutableContext();
    }

    if(context->type() == typeid(std::weak_ptr<Entry>)) {
        std::weak_ptr<Entry> weak_entry = std::any_cast<std::weak_ptr<Entry>>(*context);
        entry = weak_entry.lock();
    }

    if(!entry) {
        entry = std::make_shared<Entry>(webconn);
        webconn->setContext(std::weak_ptr<Entry>(entry));
    }

    int newest_idx = (this->current_index_ + wheel_.size() - 1) % this->wheel_.size();
    this->wheel_[newest_idx].insert(entry);

    thread_local flatbuffers::FlatBufferBuilder builder(64);
    builder.Clear();

    ChatApp::PongPayloadBuilder PongBuilder(builder);
    PongBuilder.add_ts(ts);
    auto PongOffset = PongBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_PongPayload);
    rootMsgBuilder.add_payload(PongOffset.Union());
    auto rootMsg = rootMsgBuilder.Finish();

    builder.Finish(rootMsg);
    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    std::size_t size = builder.GetSize();

    if(callback) callback(std::string(data, size));
}