#include "json.h"
#include <sstream>
#include <cwchar>

static size_t skipWhitespace(const std::wstring& s, size_t i) {
    while (i < s.size() && (s[i] == L' ' || s[i] == L'\t' || s[i] == L'\n' || s[i] == L'\r')) i++;
    return i;
}

static JsonValue parseValue(const std::wstring& s, size_t& i);

static std::wstring parseString(const std::wstring& s, size_t& i) {
    std::wstring result;
    i++;
    while (i < s.size() && s[i] != L'"') {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            i++;
            wchar_t ch = s[i];
            if (ch == L'n') result += L'\n';
            else if (ch == L't') result += L'\t';
            else if (ch == L'r') result += L'\r';
            else if (ch == L'\\') result += L'\\';
            else if (ch == L'"') result += L'"';
            else if (ch == L'u' && i + 4 < s.size()) {
                wchar_t hex[5] = {s[i+1], s[i+2], s[i+3], s[i+4], 0};
                result += (wchar_t)wcstol(hex, nullptr, 16);
                i += 4;
            }
        } else {
            result += s[i];
        }
        i++;
    }
    if (i < s.size()) i++;
    return result;
}

static JsonValue parseValue(const std::wstring& s, size_t& i) {
    i = skipWhitespace(s, i);
    JsonValue val;
    
    if (i >= s.size()) return val;
    
    if (s[i] == L'"') {
        val.type = JsonValue::Str;
        val.str = parseString(s, i);
    } else if (s[i] == L'{') {
        val.type = JsonValue::Obj;
        i++;
        i = skipWhitespace(s, i);
        
        while (i < s.size() && s[i] != L'}') {
            std::wstring key = parseString(s, i);
            i = skipWhitespace(s, i);
            if (i < s.size() && s[i] == L':') i++;
            JsonValue v = parseValue(s, i);
            val.obj.emplace_back(key, v);
            
            i = skipWhitespace(s, i);
            if (i < s.size() && s[i] == L',') i++;
            i = skipWhitespace(s, i);
        }
        if (i < s.size()) i++;
    } else if (s[i] == L'[') {
        val.type = JsonValue::Arr;
        i++;
        i = skipWhitespace(s, i);
        
        while (i < s.size() && s[i] != L']') {
            val.arr.push_back(parseValue(s, i));
            i = skipWhitespace(s, i);
            if (i < s.size() && s[i] == L',') i++;
            i = skipWhitespace(s, i);
        }
        if (i < s.size()) i++;
    } else if (s.substr(i, 4) == L"true") {
        val.type = JsonValue::Bool;
        val.b = true;
        i += 4;
    } else if (s.substr(i, 5) == L"false") {
        val.type = JsonValue::Bool;
        val.b = false;
        i += 5;
    } else if (s.substr(i, 4) == L"null") {
        val.type = JsonValue::Null;
        i += 4;
    } else if (std::iswdigit(s[i]) || s[i] == L'-') {
        size_t start = i;
        if (s[i] == L'-') i++;
        while (i < s.size() && (std::iswdigit(s[i]) || s[i] == L'.')) i++;
        val.type = JsonValue::Num;
        val.num = wcstod(s.substr(start, i - start).c_str(), nullptr);
    }
    
    return val;
}

JsonValue json_parse(const std::wstring& text) {
    size_t i = 0;
    return parseValue(text, i);
}

static void stringifyValue(std::wostringstream& ss, const JsonValue& val, int indent, bool pretty);

static void stringifyString(std::wostringstream& ss, const std::wstring& s) {
    ss << L'"';
    for (wchar_t c : s) {
        if (c == L'"') ss << L"\\\"";
        else if (c == L'\\') ss << L"\\\\";
        else if (c == L'\n') ss << L"\\n";
        else if (c == L'\r') ss << L"\\r";
        else if (c == L'\t') ss << L"\\t";
        else ss << c;
    }
    ss << L'"';
}

static void stringifyValue(std::wostringstream& ss, const JsonValue& val, int indent, bool pretty) {
    std::wstring indentStr = pretty ? std::wstring(indent * 2, L' ') : L"";
    std::wstring nextIndentStr = pretty ? std::wstring((indent + 1) * 2, L' ') : L"";
    
    switch (val.type) {
        case JsonValue::Null:
            ss << L"null";
            break;
        case JsonValue::Bool:
            ss << (val.b ? L"true" : L"false");
            break;
        case JsonValue::Num:
            if (val.num == (long long)val.num) {
                ss << (long long)val.num;
            } else {
                ss << val.num;
            }
            break;
        case JsonValue::Str:
            stringifyString(ss, val.str);
            break;
        case JsonValue::Arr:
            ss << L'[';
            if (pretty && !val.arr.empty()) ss << L'\n';
            for (size_t i = 0; i < val.arr.size(); ++i) {
                if (pretty) ss << nextIndentStr;
                stringifyValue(ss, val.arr[i], indent + 1, pretty);
                if (i + 1 < val.arr.size()) ss << L',';
                if (pretty) ss << L'\n';
            }
            if (pretty && !val.arr.empty()) ss << indentStr;
            ss << L']';
            break;
        case JsonValue::Obj:
            ss << L'{';
            if (pretty && !val.obj.empty()) ss << L'\n';
            for (size_t i = 0; i < val.obj.size(); ++i) {
                if (pretty) ss << nextIndentStr;
                stringifyString(ss, val.obj[i].first);
                ss << L':';
                if (pretty) ss << L' ';
                stringifyValue(ss, val.obj[i].second, indent + 1, pretty);
                if (i + 1 < val.obj.size()) ss << L',';
                if (pretty) ss << L'\n';
            }
            if (pretty && !val.obj.empty()) ss << indentStr;
            ss << L'}';
            break;
    }
}

std::wstring json_stringify(const JsonValue& val, bool pretty) {
    std::wostringstream ss;
    stringifyValue(ss, val, 0, pretty);
    return ss.str();
}
