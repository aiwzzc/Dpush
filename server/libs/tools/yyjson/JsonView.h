#pragma once

#include "yyjson.h"
#include <string>
#include <memory>

class JsonValue;

class JsonDoc {
public:
    JsonDoc() {
        doc_ = yyjson_mut_doc_new(NULL);
        root_ = yyjson_mut_obj(doc_);           
        yyjson_mut_doc_set_root(doc_, root_);
    }

    ~JsonDoc() {
        if (doc_) yyjson_mut_doc_free(doc_);
    }

    bool parse(const char* json, size_t len) {
        if (doc_) {
            yyjson_mut_doc_free(doc_);
            doc_ = nullptr;
            root_ = nullptr;
        }

        yyjson_doc* rdoc = yyjson_read(json, len, 0);
        if (!rdoc) return false;

        doc_ = yyjson_mut_doc_new(NULL);
        root_ = copyValue(yyjson_doc_get_root(rdoc));

        yyjson_mut_doc_set_root(doc_, root_);

        yyjson_doc_free(rdoc);
        return true;
    }

    JsonValue root();

    std::string toString() const {
        if (!doc_) return "{}";
        size_t len;
        char* buf = yyjson_mut_write(doc_, 0, &len);
        std::string s(buf, len);
        free(buf);
        return s;
    }

    yyjson_mut_doc* doc() { return doc_; }

private:
    // 🔥 关键：递归 copy
    yyjson_mut_val* copyValue(yyjson_val* val) {
        if (yyjson_is_null(val)) return yyjson_mut_null(doc_);
        if (yyjson_is_bool(val)) return yyjson_mut_bool(doc_, yyjson_get_bool(val));
        if (yyjson_is_int(val)) return yyjson_mut_int(doc_, yyjson_get_int(val));
        if (yyjson_is_real(val)) return yyjson_mut_real(doc_, yyjson_get_real(val));
        if (yyjson_is_str(val)) return yyjson_mut_str(doc_, yyjson_get_str(val));

        if (yyjson_is_arr(val)) {
            yyjson_mut_val* arr = yyjson_mut_arr(doc_);
            size_t idx, max;
            yyjson_val* elem;
            yyjson_arr_foreach(val, idx, max, elem) {
                yyjson_mut_arr_add_val(arr, copyValue(elem));
            }
            return arr;
        }

        if (yyjson_is_obj(val)) {
            yyjson_mut_val* obj = yyjson_mut_obj(doc_);
            yyjson_val *key, *value;
            size_t idx, max;
            yyjson_obj_foreach(val, idx, max, key, value) {
                yyjson_mut_obj_add_val(
                    doc_,
                    obj,
                    yyjson_get_str(key),
                    copyValue(value)
                );
            }
            return obj;
        }

        return yyjson_mut_null(doc_);
    }

private:
    yyjson_mut_doc* doc_;
    yyjson_mut_val* root_;
};

class JsonValue {
public:
    JsonValue(yyjson_mut_val* val = nullptr, yyjson_mut_doc* doc = nullptr)
        : val_(val), doc_(doc) {}

    // 链式访问对象 key
    JsonValue operator[](const char* key) {
        if (!val_) return JsonValue(nullptr, doc_);

        // 🔥 如果当前不是 object，直接失败（也可以改成自动转 object）
        if (!yyjson_mut_is_obj(val_)) return JsonValue(nullptr, doc_);

        yyjson_mut_val* child = yyjson_mut_obj_get(val_, key);

        if (!child) {
            child = yyjson_mut_obj(doc_);
            yyjson_mut_obj_add_val(doc_, val_, key, child);
        }

        return JsonValue(child, doc_);
    }

    // 判断是否有 key
    bool isMember(const char* key) const {
        if (!val_ || !yyjson_mut_is_obj(val_)) return false;
        return yyjson_mut_obj_get(val_, key) != nullptr;
    }

    // 读操作
    int asInt(int default_val = 0) const {
        return (val_ && yyjson_mut_is_int(val_)) ? (int)yyjson_mut_get_int(val_) : default_val;
    }
    std::string asString(const std::string& default_val = "") const {
        return (val_ && yyjson_mut_is_str(val_)) ? std::string(yyjson_mut_get_str(val_)) : default_val;
    }
    bool asBool(bool default_val = false) const {
        return (val_ && yyjson_mut_is_bool(val_)) ? yyjson_mut_get_bool(val_) : default_val;
    }
    bool isNull() const { return !val_ || yyjson_mut_is_null(val_); }

    bool isInt() const { return val_ && yyjson_mut_is_int(val_); }
    bool isString() const { return val_ && yyjson_mut_is_str(val_); }
    bool isBool() const { return val_ && yyjson_mut_is_bool(val_); }

    // 写操作
    void set(int v) { if (val_) yyjson_mut_set_int(val_, v); }
    void set(const char* str) { if (val_) yyjson_mut_set_str(val_, str); }
    void set(const std::string& str) { set(str.c_str()); }
    void set(bool b) { if (val_) yyjson_mut_set_bool(val_, b); }
    void setNull() { if (val_) yyjson_mut_set_null(val_); }

    // 数组操作
    void append(int v) { addValue(yyjson_mut_int(doc_, v)); }
    void append(const char* str) { addValue(yyjson_mut_str(doc_, str)); }
    void append(const std::string& str) { append(str.c_str()); }
    void append(bool b) { addValue(yyjson_mut_bool(doc_, b)); }
    void appendNull() { addValue(yyjson_mut_null(doc_)); }

private:
    void addValue(yyjson_mut_val* v) {
        if (val_ && yyjson_mut_is_arr(val_)) {
            yyjson_mut_arr_add_val(val_, v);
        }
    }

public:
    yyjson_mut_val* raw() const { return val_; }

private:
    yyjson_mut_val* val_;
    yyjson_mut_doc* doc_;
};

// JsonDoc root() 实现
inline JsonValue JsonDoc::root() { return JsonValue(root_, doc_); }