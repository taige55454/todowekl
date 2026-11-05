#pragma once
#include <string>
#include <vector>

struct JsonValue {
    enum Type { Null, Bool, Num, Str, Arr, Obj };
    Type type = Null;
    bool b = false;
    double num = 0;
    std::wstring str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::wstring, JsonValue>> obj;
    
    const JsonValue* find(const std::wstring& key) const {
        for (auto& p : obj) {
            if (p.first == key) return &p.second;
        }
        return nullptr;
    }
};

JsonValue json_parse(const std::wstring& text);
std::wstring json_stringify(const JsonValue& val, bool pretty = false);
