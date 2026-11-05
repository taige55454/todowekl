#pragma once
#include <string>
#include <vector>

struct TodoItem {
    std::wstring uuid;
    std::wstring plan_uuid;
    std::wstring cycle_date;
    std::wstring text;
    bool done = false;
    std::wstring created;
    int priority = 0;
    std::wstring due_date;
    std::wstring due_time;
    std::wstring reminder_time;
    std::wstring repeat_rule;
    std::vector<int> repeat_days;
    std::wstring hide_until;
    std::wstring original_due;
    bool notified = false;
};

struct Project {
    std::wstring name;
    std::vector<TodoItem> todos;
};

struct Plan {
    std::wstring plan_uuid;
    std::wstring text;
    int priority = 0;
    std::wstring due_time;
    std::wstring repeat_rule;
    std::vector<int> repeat_days;
    std::wstring base_date;
};

struct HistoryItem {
    std::wstring plan_uuid;
    std::wstring cycle_date;
    std::wstring project;
    std::wstring text;
    std::wstring created;
    std::wstring completed;
    std::wstring due_date;
};

struct ForestRecord {
    std::wstring timestamp;
    std::wstring status;
    int minutes = 0;
    std::wstring task;
};

struct Config {
    std::wstring title_prefix = L"你好";
    bool auto_start = false;
    int card_text_size = 1;
    int focus_minutes = 25;
};

struct ParseResult {
    std::wstring title;
    std::wstring due_date;
    std::wstring due_time;
    int priority = 0;
    std::wstring repeat_rule;
    std::vector<int> repeat_days;
    std::wstring base_date;
    std::vector<std::wstring> labels;
};

std::vector<Project> load_projects();
void save_projects(const std::vector<Project>& projects);

std::vector<Plan> load_plans();
void save_plans(const std::vector<Plan>& plans);

std::vector<HistoryItem> load_history();
void save_history(const std::vector<HistoryItem>& history);

std::vector<ForestRecord> load_forest();
void save_forest(const std::vector<ForestRecord>& records);

Config load_config();
void save_config(const Config& cfg);

bool is_auto_start();
void set_auto_start(bool enable);

std::wstring now_iso();
std::wstring generate_uuid();

ParseResult quick_parse(const std::wstring& input);
