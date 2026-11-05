#include "storage.h"
#include "json.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <cwctype>

static std::wstring get_exe_dir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s = path;
    size_t pos = s.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? s.substr(0, pos) : L".";
}

static std::wstring read_file(const std::wstring& filename) {
    std::wstring fullPath = get_exe_dir() + L"\\" + filename;
    std::wifstream f(fullPath, std::ios::binary);
    if (!f.is_open()) return L"";
    
    f.seekg(0, std::ios::end);
    size_t size = (size_t)f.tellg();
    if (size == 0) return L"";
    
    f.seekg(0);
    std::wstring result(size / sizeof(wchar_t), L'\0');
    f.read(&result[0], size / sizeof(wchar_t));
    
    if (result.size() > 0 && result[0] == 0xFEFF) {
        result = result.substr(1);
    }
    
    return result;
}

static void write_file(const std::wstring& filename, const std::wstring& content) {
    std::wstring fullPath = get_exe_dir() + L"\\" + filename;
    std::wofstream f(fullPath, std::ios::binary);
    if (!f.is_open()) return;
    
    f.write(L"\xFEFF", 1);
    f.write(content.c_str(), content.size());
}

static TodoItem parse_todo(const JsonValue& obj) {
    TodoItem todo;
    if (auto p = obj.find(L"uuid")) todo.uuid = p->str;
    if (auto p = obj.find(L"plan_uuid")) todo.plan_uuid = p->str;
    if (auto p = obj.find(L"cycle_date")) todo.cycle_date = p->str;
    if (auto p = obj.find(L"text")) todo.text = p->str;
    if (auto p = obj.find(L"done")) todo.done = p->b;
    if (auto p = obj.find(L"created")) todo.created = p->str;
    if (auto p = obj.find(L"priority")) todo.priority = (int)p->num;
    if (auto p = obj.find(L"due_date")) todo.due_date = p->str;
    if (auto p = obj.find(L"due_time")) todo.due_time = p->str;
    if (auto p = obj.find(L"reminder_time")) todo.reminder_time = p->str;
    if (auto p = obj.find(L"repeat_rule")) todo.repeat_rule = p->str;
    if (auto p = obj.find(L"repeat_days")) {
        for (auto& d : p->arr) todo.repeat_days.push_back((int)d.num);
    }
    if (auto p = obj.find(L"hide_until")) todo.hide_until = p->str;
    if (auto p = obj.find(L"original_due")) todo.original_due = p->str;
    if (auto p = obj.find(L"notified")) todo.notified = p->b;
    return todo;
}

static JsonValue serialize_todo(const TodoItem& todo) {
    JsonValue obj;
    obj.type = JsonValue::Obj;
    obj.obj.push_back({L"uuid", {JsonValue::Str, false, 0, todo.uuid}});
    obj.obj.push_back({L"plan_uuid", {JsonValue::Str, false, 0, todo.plan_uuid}});
    obj.obj.push_back({L"cycle_date", {JsonValue::Str, false, 0, todo.cycle_date}});
    obj.obj.push_back({L"text", {JsonValue::Str, false, 0, todo.text}});
    obj.obj.push_back({L"done", {JsonValue::Bool, todo.done}});
    obj.obj.push_back({L"created", {JsonValue::Str, false, 0, todo.created}});
    obj.obj.push_back({L"priority", {JsonValue::Num, false, (double)todo.priority}});
    obj.obj.push_back({L"due_date", {JsonValue::Str, false, 0, todo.due_date}});
    obj.obj.push_back({L"due_time", {JsonValue::Str, false, 0, todo.due_time}});
    obj.obj.push_back({L"reminder_time", {JsonValue::Str, false, 0, todo.reminder_time}});
    obj.obj.push_back({L"repeat_rule", {JsonValue::Str, false, 0, todo.repeat_rule}});
    
    JsonValue days_arr;
    days_arr.type = JsonValue::Arr;
    for (int d : todo.repeat_days) {
        JsonValue v;
        v.type = JsonValue::Num;
        v.num = d;
        days_arr.arr.push_back(v);
    }
    obj.obj.push_back({L"repeat_days", days_arr});
    
    obj.obj.push_back({L"hide_until", {JsonValue::Str, false, 0, todo.hide_until}});
    obj.obj.push_back({L"original_due", {JsonValue::Str, false, 0, todo.original_due}});
    obj.obj.push_back({L"notified", {JsonValue::Bool, todo.notified}});
    
    return obj;
}

std::vector<Project> load_projects() {
    std::vector<Project> result;
    std::wstring content = read_file(L"todos.json");
    if (content.empty()) return result;
    
    JsonValue root = json_parse(content);
    if (root.type != JsonValue::Arr) return result;
    
    for (auto& proj_val : root.arr) {
        Project proj;
        if (auto p = proj_val.find(L"name")) proj.name = p->str;
        if (auto p = proj_val.find(L"todos")) {
            for (auto& todo_val : p->arr) {
                proj.todos.push_back(parse_todo(todo_val));
            }
        }
        result.push_back(proj);
    }
    
    return result;
}

void save_projects(const std::vector<Project>& projects) {
    JsonValue root;
    root.type = JsonValue::Arr;
    
    for (auto& proj : projects) {
        JsonValue proj_obj;
        proj_obj.type = JsonValue::Obj;
        proj_obj.obj.push_back({L"name", {JsonValue::Str, false, 0, proj.name}});
        
        JsonValue todos_arr;
        todos_arr.type = JsonValue::Arr;
        for (auto& todo : proj.todos) {
            todos_arr.arr.push_back(serialize_todo(todo));
        }
        proj_obj.obj.push_back({L"todos", todos_arr});
        
        root.arr.push_back(proj_obj);
    }
    
    write_file(L"todos.json", json_stringify(root, true));
}

std::vector<Plan> load_plans() {
    std::vector<Plan> result;
    std::wstring content = read_file(L"plans.json");
    if (content.empty()) return result;
    
    JsonValue root = json_parse(content);
    if (root.type != JsonValue::Arr) return result;
    
    for (auto& plan_val : root.arr) {
        Plan plan;
        if (auto p = plan_val.find(L"plan_uuid")) plan.plan_uuid = p->str;
        if (auto p = plan_val.find(L"text")) plan.text = p->str;
        if (auto p = plan_val.find(L"priority")) plan.priority = (int)p->num;
        if (auto p = plan_val.find(L"due_time")) plan.due_time = p->str;
        if (auto p = plan_val.find(L"repeat_rule")) plan.repeat_rule = p->str;
        if (auto p = plan_val.find(L"repeat_days")) {
            for (auto& d : p->arr) plan.repeat_days.push_back((int)d.num);
        }
        if (auto p = plan_val.find(L"base_date")) plan.base_date = p->str;
        result.push_back(plan);
    }
    
    return result;
}

void save_plans(const std::vector<Plan>& plans) {
    JsonValue root;
    root.type = JsonValue::Arr;
    
    for (auto& plan : plans) {
        JsonValue obj;
        obj.type = JsonValue::Obj;
        obj.obj.push_back({L"plan_uuid", {JsonValue::Str, false, 0, plan.plan_uuid}});
        obj.obj.push_back({L"text", {JsonValue::Str, false, 0, plan.text}});
        obj.obj.push_back({L"priority", {JsonValue::Num, false, (double)plan.priority}});
        obj.obj.push_back({L"due_time", {JsonValue::Str, false, 0, plan.due_time}});
        obj.obj.push_back({L"repeat_rule", {JsonValue::Str, false, 0, plan.repeat_rule}});
        
        JsonValue days_arr;
        days_arr.type = JsonValue::Arr;
        for (int d : plan.repeat_days) {
            JsonValue v;
            v.type = JsonValue::Num;
            v.num = d;
            days_arr.arr.push_back(v);
        }
        obj.obj.push_back({L"repeat_days", days_arr});
        
        obj.obj.push_back({L"base_date", {JsonValue::Str, false, 0, plan.base_date}});
        
        root.arr.push_back(obj);
    }
    
    write_file(L"plans.json", json_stringify(root, true));
}

std::vector<HistoryItem> load_history() {
    std::vector<HistoryItem> result;
    std::wstring content = read_file(L"history.json");
    if (content.empty()) return result;
    
    JsonValue root = json_parse(content);
    if (root.type != JsonValue::Arr) return result;
    
    for (auto& hist_val : root.arr) {
        HistoryItem h;
        if (auto p = hist_val.find(L"plan_uuid")) h.plan_uuid = p->str;
        if (auto p = hist_val.find(L"cycle_date")) h.cycle_date = p->str;
        if (auto p = hist_val.find(L"project")) h.project = p->str;
        if (auto p = hist_val.find(L"text")) h.text = p->str;
        if (auto p = hist_val.find(L"created")) h.created = p->str;
        if (auto p = hist_val.find(L"completed")) h.completed = p->str;
        if (auto p = hist_val.find(L"due_date")) h.due_date = p->str;
        result.push_back(h);
    }
    
    return result;
}

void save_history(const std::vector<HistoryItem>& history) {
    JsonValue root;
    root.type = JsonValue::Arr;
    
    for (auto& h : history) {
        JsonValue obj;
        obj.type = JsonValue::Obj;
        obj.obj.push_back({L"plan_uuid", {JsonValue::Str, false, 0, h.plan_uuid}});
        obj.obj.push_back({L"cycle_date", {JsonValue::Str, false, 0, h.cycle_date}});
        obj.obj.push_back({L"project", {JsonValue::Str, false, 0, h.project}});
        obj.obj.push_back({L"text", {JsonValue::Str, false, 0, h.text}});
        obj.obj.push_back({L"created", {JsonValue::Str, false, 0, h.created}});
        obj.obj.push_back({L"completed", {JsonValue::Str, false, 0, h.completed}});
        obj.obj.push_back({L"due_date", {JsonValue::Str, false, 0, h.due_date}});
        root.arr.push_back(obj);
    }
    
    write_file(L"history.json", json_stringify(root, true));
}

std::vector<ForestRecord> load_forest() {
    std::vector<ForestRecord> result;
    std::wstring content = read_file(L"forest.json");
    if (content.empty()) return result;
    
    JsonValue root = json_parse(content);
    if (root.type != JsonValue::Arr) return result;
    
    for (auto& rec_val : root.arr) {
        ForestRecord rec;
        if (auto p = rec_val.find(L"timestamp")) rec.timestamp = p->str;
        if (auto p = rec_val.find(L"status")) rec.status = p->str;
        if (auto p = rec_val.find(L"minutes")) rec.minutes = (int)p->num;
        if (auto p = rec_val.find(L"task")) rec.task = p->str;
        result.push_back(rec);
    }
    
    return result;
}

void save_forest(const std::vector<ForestRecord>& records) {
    JsonValue root;
    root.type = JsonValue::Arr;
    
    for (auto& rec : records) {
        JsonValue obj;
        obj.type = JsonValue::Obj;
        obj.obj.push_back({L"timestamp", {JsonValue::Str, false, 0, rec.timestamp}});
        obj.obj.push_back({L"status", {JsonValue::Str, false, 0, rec.status}});
        obj.obj.push_back({L"minutes", {JsonValue::Num, false, (double)rec.minutes}});
        obj.obj.push_back({L"task", {JsonValue::Str, false, 0, rec.task}});
        root.arr.push_back(obj);
    }
    
    write_file(L"forest.json", json_stringify(root, true));
}

Config load_config() {
    Config cfg;
    std::wstring content = read_file(L"config.json");
    if (content.empty()) return cfg;
    
    JsonValue root = json_parse(content);
    if (root.type != JsonValue::Obj) return cfg;
    
    if (auto p = root.find(L"title_prefix")) cfg.title_prefix = p->str;
    if (auto p = root.find(L"auto_start")) cfg.auto_start = p->b;
    if (auto p = root.find(L"card_text_size")) cfg.card_text_size = (int)p->num;
    if (auto p = root.find(L"focus_minutes")) cfg.focus_minutes = (int)p->num;
    
    return cfg;
}

void save_config(const Config& cfg) {
    JsonValue root;
    root.type = JsonValue::Obj;
    root.obj.push_back({L"title_prefix", {JsonValue::Str, false, 0, cfg.title_prefix}});
    root.obj.push_back({L"auto_start", {JsonValue::Bool, cfg.auto_start}});
    root.obj.push_back({L"card_text_size", {JsonValue::Num, false, (double)cfg.card_text_size}});
    root.obj.push_back({L"focus_minutes", {JsonValue::Num, false, (double)cfg.focus_minutes}});
    
    write_file(L"config.json", json_stringify(root, true));
}

bool is_auto_start() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    
    wchar_t value[MAX_PATH];
    DWORD size = sizeof(value);
    bool exists = (RegQueryValueExW(hKey, L"ToDoWell", nullptr, nullptr, (LPBYTE)value, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    
    return exists;
}

void set_auto_start(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return;
    }
    
    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring cmd = std::wstring(L"\"") + path + L"\"";
        RegSetValueExW(hKey, L"ToDoWell", 0, REG_SZ, (LPBYTE)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, L"ToDoWell");
    }
    
    RegCloseKey(hKey);
}

std::wstring now_iso() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02dT%02d:%02d:%02d", 
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring generate_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t a = dis(gen);
    uint64_t b = dis(gen);
    
    wchar_t buf[64];
    swprintf_s(buf, L"%08x-%04x-%04x-%04x-%012llx",
               (unsigned int)(a >> 32),
               (unsigned int)((a >> 16) & 0xFFFF),
               (unsigned int)(a & 0xFFFF),
               (unsigned int)(b >> 48),
               b & 0xFFFFFFFFFFFFull);
    
    return buf;
}

ParseResult quick_parse(const std::wstring& input) {
    ParseResult result;
    result.priority = 0;
    
    if (input.empty()) return result;
    
    std::wstring s = input;
    std::vector<std::wstring> labels;
    
    if (s[0] == L'!' || s.find(L"important") == 0) {
        result.priority = 1;
        labels.push_back(L"⭐重要");
        if (s[0] == L'!') {
            s = s.substr(1);
        } else {
            s = s.substr(9);
        }
        while (!s.empty() && std::iswspace(s[0])) s.erase(0, 1);
    }
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    std::wstring today;
    {
        wchar_t buf[16];
        swprintf_s(buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
        today = buf;
    }
    
    auto calc_date = [&](int year, int month, int day) -> std::wstring {
        wchar_t buf[16];
        swprintf_s(buf, L"%04d-%02d-%02d", year, month, day);
        return buf;
    };
    
    auto date_from_days = [&](int days) -> std::wstring {
        SYSTEMTIME temp = st;
        temp.wDay += days;
        while (temp.wDay > 31) {
            temp.wDay -= 31;
            temp.wMonth++;
            if (temp.wMonth > 12) {
                temp.wMonth = 1;
                temp.wYear++;
            }
        }
        return calc_date(temp.wYear, temp.wMonth, temp.wDay);
    };
    
    size_t pos = 0;
    
    if ((pos = s.find(L"今天")) != std::wstring::npos) {
        result.due_date = today;
        labels.push_back(L"📅今天");
        s.erase(pos, 2);
    } else if ((pos = s.find(L"明天")) != std::wstring::npos) {
        result.due_date = date_from_days(1);
        labels.push_back(L"📅明天");
        s.erase(pos, 2);
    } else if ((pos = s.find(L"后天")) != std::wstring::npos) {
        result.due_date = date_from_days(2);
        labels.push_back(L"📅后天");
        s.erase(pos, 2);
    } else if ((pos = s.find(L"大后天")) != std::wstring::npos) {
        result.due_date = date_from_days(3);
        labels.push_back(L"📅大后天");
        s.erase(pos, 3);
    }
    
    for (size_t i = 0; i < s.size(); ++i) {
        if (i + 2 < s.size() && std::iswdigit(s[i]) && (s[i+1] == L'月' || s[i+1] == L'月')) {
            int month = s[i] - L'0';
            if (i + 1 < s.size() && std::iswdigit(s[i+1])) {
                month = month * 10 + (s[i+1] - L'0');
                i++;
            }
            
            if (i + 1 < s.size() && s[i+1] == L'月') {
                i++;
                
                int day = 0;
                size_t day_start = i + 1;
                while (day_start < s.size() && std::iswdigit(s[day_start])) {
                    day = day * 10 + (s[day_start] - L'0');
                    day_start++;
                }
                
                if (day > 0 && day <= 31) {
                    int year = st.wYear;
                    std::wstring candidate = calc_date(year, month, day);
                    if (candidate < today) {
                        candidate = calc_date(year + 1, month, day);
                    }
                    result.due_date = candidate;
                    labels.push_back(L"📅" + std::to_wstring(month) + L"月" + std::to_wstring(day) + L"日");
                    
                    s.erase(i - 1 - (month >= 10 ? 1 : 0), day_start - (i - 1 - (month >= 10 ? 1 : 0)));
                    break;
                }
            }
        }
        
        if (std::iswdigit(s[i])) {
            int day = s[i] - L'0';
            if (i + 1 < s.size() && std::iswdigit(s[i+1])) {
                day = day * 10 + (s[i+1] - L'0');
                i++;
            }
            
            if (i + 1 < s.size() && (s[i+1] == L'号' || s[i+1] == L'日')) {
                if (day > 0 && day <= 31) {
                    int month = st.wMonth;
                    int year = st.wYear;
                    
                    std::wstring candidate = calc_date(year, month, day);
                    if (candidate < today) {
                        month++;
                        if (month > 12) {
                            month = 1;
                            year++;
                        }
                        candidate = calc_date(year, month, day);
                    }
                    
                    result.due_date = candidate;
                    labels.push_back(L"📅" + std::to_wstring(day) + L"号");
                    
                    s.erase(i - (day >= 10 ? 1 : 0), i + 2 - (i - (day >= 10 ? 1 : 0)));
                    break;
                }
            }
        }
    }
    
    for (size_t i = 0; i + 4 < s.size(); ++i) {
        if (std::iswdigit(s[i]) && std::iswdigit(s[i+1]) && s[i+2] == L':' && 
            std::iswdigit(s[i+3]) && std::iswdigit(s[i+4])) {
            int hh = (s[i] - L'0') * 10 + (s[i+1] - L'0');
            int mm = (s[i+3] - L'0') * 10 + (s[i+4] - L'0');
            
            if (hh >= 0 && hh < 24 && mm >= 0 && mm < 60) {
                wchar_t buf[8];
                swprintf_s(buf, L"%02d:%02d", hh, mm);
                result.due_time = buf;
                labels.push_back(L"🕒" + std::wstring(buf));
                s.erase(i, 5);
                break;
            }
        }
    }
    
    if (result.due_time.empty()) {
        struct TimePattern { const wchar_t* pattern; int hour; int minute; };
        TimePattern patterns[] = {
            {L"早上", 8, 0}, {L"上午", 10, 0}, {L"中午", 12, 0}, {L"下午", 14, 0}, 
            {L"晚上", 19, 0}, {L"夜里", 22, 0}
        };
        
        for (auto& pat : patterns) {
            if ((pos = s.find(pat.pattern)) != std::wstring::npos) {
                int hour = pat.hour;
                int minute = pat.minute;
                
                size_t num_pos = pos + wcslen(pat.pattern);
                if (num_pos < s.size() && std::iswdigit(s[num_pos])) {
                    int num = s[num_pos] - L'0';
                    if (num_pos + 1 < s.size() && std::iswdigit(s[num_pos + 1])) {
                        num = num * 10 + (s[num_pos + 1] - L'0');
                        num_pos++;
                    }
                    
                    if (num_pos + 1 < s.size() && s[num_pos + 1] == L'点') {
                        hour = num;
                        if (pat.hour >= 12 && hour < 12) hour += 12;
                        num_pos += 2;
                        
                        if (num_pos < s.size() && (s[num_pos] == L'半')) {
                            minute = 30;
                            num_pos++;
                        } else if (num_pos < s.size() && std::iswdigit(s[num_pos])) {
                            minute = s[num_pos] - L'0';
                            if (num_pos + 1 < s.size() && std::iswdigit(s[num_pos + 1])) {
                                minute = minute * 10 + (s[num_pos + 1] - L'0');
                                num_pos++;
                            }
                            num_pos++;
                        }
                    }
                }
                
                wchar_t buf[8];
                swprintf_s(buf, L"%02d:%02d", hour, minute);
                result.due_time = buf;
                labels.push_back(L"🕒" + std::wstring(buf));
                s.erase(pos, num_pos - pos);
                break;
            }
        }
    }
    
    if ((pos = s.find(L"每天")) != std::wstring::npos) {
        result.repeat_rule = L"daily";
        labels.push_back(L"🔁每天");
        s.erase(pos, 2);
    } else if ((pos = s.find(L"工作日")) != std::wstring::npos) {
        result.repeat_rule = L"weekdays";
        labels.push_back(L"🔁工作日");
        s.erase(pos, 3);
    } else if ((pos = s.find(L"每周")) != std::wstring::npos) {
        result.repeat_rule = L"weekly";
        size_t day_pos = pos + 2;
        
        std::map<wchar_t, int> day_map = {
            {L'一', 0}, {L'二', 1}, {L'三', 2}, {L'四', 3}, 
            {L'五', 4}, {L'六', 5}, {L'日', 6}
        };
        
        while (day_pos < s.size() && day_map.count(s[day_pos])) {
            result.repeat_days.push_back(day_map[s[day_pos]]);
            day_pos++;
        }
        
        if (!result.repeat_days.empty()) {
            std::wstring day_label = L"🔁每周";
            for (auto d : result.repeat_days) {
                const wchar_t* names[] = {L"一", L"二", L"三", L"四", L"五", L"六", L"日"};
                day_label += names[d];
            }
            labels.push_back(day_label);
        } else {
            labels.push_back(L"🔁每周");
        }
        
        s.erase(pos, day_pos - pos);
    } else if ((pos = s.find(L"每月")) != std::wstring::npos) {
        result.repeat_rule = L"monthly";
        size_t num_pos = pos + 2;
        
        while (num_pos < s.size()) {
            if (std::iswdigit(s[num_pos])) {
                int day = s[num_pos] - L'0';
                if (num_pos + 1 < s.size() && std::iswdigit(s[num_pos + 1])) {
                    day = day * 10 + (s[num_pos + 1] - L'0');
                    num_pos++;
                }
                result.repeat_days.push_back(day);
                num_pos++;
                
                if (num_pos < s.size() && (s[num_pos] == L'号' || s[num_pos] == L'日')) {
                    num_pos++;
                }
                
                if (num_pos < s.size() && s[num_pos] == L',') {
                    num_pos++;
                }
                
                while (num_pos < s.size() && std::iswspace(s[num_pos])) num_pos++;
            } else {
                break;
            }
        }
        
        if (!result.repeat_days.empty()) {
            std::wstring day_label = L"🔁每月";
            for (size_t i = 0; i < result.repeat_days.size(); ++i) {
                if (i > 0) day_label += L",";
                day_label += std::to_wstring(result.repeat_days[i]);
            }
            day_label += L"号";
            labels.push_back(day_label);
        } else {
            labels.push_back(L"🔁每月");
        }
        
        s.erase(pos, num_pos - pos);
    }
    
    while (!s.empty() && std::iswspace(s[0])) s.erase(0, 1);
    while (!s.empty() && std::iswspace(s.back())) s.pop_back();
    
    result.title = s;
    result.labels = labels;
    
    return result;
}
