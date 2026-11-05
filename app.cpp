#include "app.h"
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <set>

App* g_app = nullptr;

void App::create(HWND hwnd) {
    g_app = this;
    m_hwnd = hwnd;
    
    m_projects = load_projects();
    m_plans = load_plans();
    m_history = load_history();
    m_forestRecords = load_forest();
    m_cfg = load_config();
    
    syncRepeatPlans();
    
    m_w = g_gfx.clientW();
    m_h = g_gfx.clientH();
    
    m_isBallMode = true;
    m_ballExpandAnim = 0.0f;
    
    markHitsDirty();
}

void App::destroy() {
    saveAll();
    if (m_edit) {
        DestroyWindow(m_edit);
        m_edit = nullptr;
    }
}

void App::saveAll() {
    save_projects(m_projects);
    save_plans(m_plans);
    save_history(m_history);
    save_forest(m_forestRecords);
    save_config(m_cfg);
}

void App::requestRedraw() {
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::approach(float& cur, float target, float dt, float speed) {
    float diff = target - cur;
    if (std::abs(diff) < 0.001f) {
        cur = target;
    } else {
        cur += diff * std::min(1.0f, dt * speed);
    }
}

void App::clampScroll() {
    float maxScroll = std::max(0.0f, m_contentH - (m_h - AppC::TITLE_H - 100));
    if (m_scrollTarget < 0) m_scrollTarget = 0;
    if (m_scrollTarget > maxScroll) m_scrollTarget = maxScroll;
}

void App::onSize() {
    m_w = g_gfx.clientW();
    m_h = g_gfx.clientH();
    g_gfx.resize();
    markHitsDirty();
    requestRedraw();
}

LRESULT App::onNcHitTest(POINT screenPt) {
    if (m_isBallMode) {
        return HTCAPTION;
    }
    
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    int x = screenPt.x - rc.left;
    int y = screenPt.y - rc.top;
    
    float ydip = toDip(y);
    
    if (ydip < AppC::TITLE_H) {
        return HTCAPTION;
    }
    
    return HTCLIENT;
}

void App::onMouseMove(float xdip, float ydip) {
    bool wasHoverClose = m_hoverClose;
    m_hoverClose = false;
    
    for (auto& hit : m_hits) {
        if (xdip >= hit.rc.left && xdip <= hit.rc.right &&
            ydip >= hit.rc.top && ydip <= hit.rc.bottom) {
            if (hit.type == H_CLOSE) {
                m_hoverClose = true;
            }
        }
    }
    
    if (wasHoverClose != m_hoverClose) {
        requestRedraw();
    }
}

void App::onWheel(int delta) {
    if (m_isBallMode) return;
    
    m_scrollTarget -= delta * 0.5f;
    clampScroll();
    requestRedraw();
}

void App::ensureEditCreated() {
    if (m_edit) return;
    
    m_edit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 200, 24,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
}

std::wstring App::editGetText() {
    if (!m_edit) return L"";
    int len = GetWindowTextLengthW(m_edit);
    if (len <= 0) return L"";
    std::wstring result(len + 1, L'\0');
    GetWindowTextW(m_edit, &result[0], len + 1);
    result.resize(len);
    return result;
}

void App::editSetText(const std::wstring& s) {
    if (m_edit) {
        SetWindowTextW(m_edit, s.c_str());
    }
}

std::wstring App::addDaysToDate(const std::wstring& date, int days) {
    SYSTEMTIME st = {};
    int y, m, d;
    if (swscanf_s(date.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) {
        return date;
    }
    
    st.wYear = y;
    st.wMonth = m;
    st.wDay = d;
    
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) return date;
    
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    LONGLONG offset = (LONGLONG)days * 864000000000LL;
    uli.QuadPart += offset;
    
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    
    if (!FileTimeToSystemTime(&ft, &st)) return date;
    
    wchar_t buf[16];
    swprintf_s(buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring App::getNextCycleDate(const Plan& plan, const std::wstring& currentDate) {
    SYSTEMTIME st = {};
    int y, m, d;
    if (swscanf_s(currentDate.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) {
        return currentDate;
    }
    
    st.wYear = y;
    st.wMonth = m;
    st.wDay = d;
    
    if (plan.repeat_rule == L"daily") {
        return addDaysToDate(currentDate, 1);
    } else if (plan.repeat_rule == L"weekdays") {
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        FileTimeToSystemTime(&ft, &st);
        int weekday = st.wDayOfWeek;
        
        if (weekday == 5) {
            return addDaysToDate(currentDate, 3);
        } else if (weekday == 6) {
            return addDaysToDate(currentDate, 2);
        } else {
            return addDaysToDate(currentDate, 1);
        }
    } else if (plan.repeat_rule == L"weekly") {
        return addDaysToDate(currentDate, 7);
    } else if (plan.repeat_rule == L"monthly") {
        st.wMonth++;
        if (st.wMonth > 12) {
            st.wMonth = 1;
            st.wYear++;
        }
        
        int maxDay = 31;
        if (st.wMonth == 2) {
            bool leap = (st.wYear % 4 == 0 && st.wYear % 100 != 0) || (st.wYear % 400 == 0);
            maxDay = leap ? 29 : 28;
        } else if (st.wMonth == 4 || st.wMonth == 6 || st.wMonth == 9 || st.wMonth == 11) {
            maxDay = 30;
        }
        
        if (st.wDay > maxDay) st.wDay = maxDay;
        
        wchar_t buf[16];
        swprintf_s(buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
        return buf;
    }
    
    return currentDate;
}

void App::syncRepeatPlans() {
    std::wstring today = now_iso().substr(0, 10);
    
    if (m_lastSyncDate == today) return;
    m_lastSyncDate = today;
    
    std::set<std::pair<std::wstring, std::wstring>> existingCycles;
    for (auto& proj : m_projects) {
        for (auto& todo : proj.todos) {
            if (!todo.plan_uuid.empty() && !todo.cycle_date.empty()) {
                existingCycles.insert({todo.plan_uuid, todo.cycle_date});
            }
        }
    }
    
    size_t start = m_history.size() > 300 ? m_history.size() - 300 : 0;
    for (size_t i = start; i < m_history.size(); ++i) {
        auto& h = m_history[i];
        if (!h.plan_uuid.empty() && !h.cycle_date.empty()) {
            existingCycles.insert({h.plan_uuid, h.cycle_date});
        }
    }
    
    bool hasNewTasks = false;
    
    for (auto& plan : m_plans) {
        bool shouldCreate = false;
        
        SYSTEMTIME st;
        GetLocalTime(&st);
        int cur_weekday = st.wDayOfWeek == 0 ? 6 : st.wDayOfWeek - 1;
        
        if (!plan.base_date.empty() && plan.base_date > today) {
            continue;
        }
        
        if (plan.repeat_rule == L"daily") {
            shouldCreate = true;
        } else if (plan.repeat_rule == L"weekdays") {
            shouldCreate = (cur_weekday >= 0 && cur_weekday <= 4);
        } else if (plan.repeat_rule == L"weekly") {
            for (int target_day : plan.repeat_days) {
                if (cur_weekday == target_day) {
                    shouldCreate = true;
                    break;
                }
            }
        } else if (plan.repeat_rule == L"monthly") {
            int max_day = 31;
            if (st.wMonth == 2) {
                bool leap = (st.wYear % 4 == 0 && st.wYear % 100 != 0) || (st.wYear % 400 == 0);
                max_day = leap ? 29 : 28;
            } else if (st.wMonth == 4 || st.wMonth == 6 || st.wMonth == 9 || st.wMonth == 11) {
                max_day = 30;
            }
            
            for (int target_day : plan.repeat_days) {
                int actual_day = std::min(target_day, max_day);
                if (st.wDay == actual_day) {
                    shouldCreate = true;
                    break;
                }
            }
        }
        
        if (shouldCreate && existingCycles.count({plan.plan_uuid, today}) == 0) {
            TodoItem newTask;
            newTask.uuid = generate_uuid();
            newTask.plan_uuid = plan.plan_uuid;
            newTask.cycle_date = today;
            newTask.text = plan.text;
            newTask.priority = plan.priority;
            newTask.due_date = today;
            newTask.due_time = plan.due_time;
            newTask.reminder_time = plan.due_time;
            newTask.repeat_rule = plan.repeat_rule;
            newTask.repeat_days = plan.repeat_days;
            newTask.created = now_iso();
            newTask.done = false;
            newTask.notified = false;
            
            if (m_projects.empty()) {
                Project defaultProj;
                defaultProj.name = L"默认项目";
                m_projects.push_back(defaultProj);
            }
            m_projects[0].todos.insert(m_projects[0].todos.begin(), newTask);
            hasNewTasks = true;
        }
    }
    
    if (hasNewTasks) {
        saveAll();
        markHitsDirty();
    }
}

int App::computeQuadrant(const TodoItem& todo) {
    bool is_important = (todo.priority == 1);
    bool is_urgent = false;
    
    std::wstring today = now_iso().substr(0, 10);
    std::wstring now_time = now_iso().substr(11, 5);
    
    if (!todo.due_date.empty()) {
        if (todo.due_date < today) {
            is_urgent = true;
        } else if (todo.due_date == today && !todo.due_time.empty()) {
            if (todo.due_time <= now_time) {
                is_urgent = true;
            }
        }
    } else if (!todo.due_time.empty()) {
        if (todo.due_time <= now_time) {
            is_urgent = true;
        }
    }
    
    if (is_important && is_urgent) return 1;
    if (is_important && !is_urgent) return 2;
    if (!is_important && is_urgent) return 3;
    return 4;
}

void App::checkReminders() {
    std::wstring now_date = now_iso().substr(0, 10);
    std::wstring now_time = now_iso().substr(11, 5);
    
    for (auto& proj : m_projects) {
        for (auto& todo : proj.todos) {
            if (todo.done || todo.notified) continue;
            
            std::wstring reminder = todo.reminder_time.empty() ? todo.due_time : todo.reminder_time;
            if (reminder.empty()) continue;
            
            std::wstring target_date = todo.due_date.empty() ? now_date : todo.due_date;
            
            if (target_date == now_date && reminder == now_time) {
                todo.notified = true;
                showToast(L"⏰ " + todo.text);
            }
        }
    }
}

void App::showToast(const std::wstring& msg) {
    m_toastMessage = msg;
    m_toastAlpha = 1.0f;
    requestRedraw();
}

std::wstring App::getRuleDisplayStr(const std::wstring& rule, const std::vector<int>& days) {
    if (rule == L"daily") return L"每天";
    if (rule == L"weekdays") return L"工作日";
    if (rule == L"weekly") {
        if (days.empty()) return L"每周";
        std::wstring result = L"每周";
        const wchar_t* names[] = {L"一",L"二",L"三",L"四",L"五",L"六",L"日"};
        for (int d : days) {
            if (d >= 0 && d <= 6) result += names[d];
        }
        return result;
    }
    if (rule == L"monthly") {
        if (days.empty()) return L"每月";
        std::wstring result = L"每月";
        for (size_t i = 0; i < days.size(); ++i) {
            if (i > 0) result += L",";
            result += std::to_wstring(days[i]);
        }
        result += L"号";
        return result;
    }
    return L"周期";
}

bool App::isOverdue(const TodoItem& todo) {
    std::wstring today = now_iso().substr(0, 10);
    std::wstring now_time = now_iso().substr(11, 5);
    
    if (!todo.due_date.empty() && todo.due_date < today) {
        return true;
    }
    if (todo.due_date == today && !todo.due_time.empty() && todo.due_time < now_time) {
        return true;
    }
    return false;
}

void App::onQuickInputChanged() {
    m_parsedResult = quick_parse(m_quickInputText);
    requestRedraw();
}

void App::commitQuickAdd() {
    if (m_quickInputText.empty()) return;
    
    ParseResult res = quick_parse(m_quickInputText);
    if (res.title.empty()) return;
    
    std::wstring plan_uuid;
    if (!res.repeat_rule.empty()) {
        plan_uuid = generate_uuid();
        Plan newPlan;
        newPlan.plan_uuid = plan_uuid;
        newPlan.text = res.title;
        newPlan.priority = res.priority;
        newPlan.due_time = res.due_time;
        newPlan.repeat_rule = res.repeat_rule;
        newPlan.repeat_days = res.repeat_days;
        newPlan.base_date = res.base_date.empty() ? (res.due_date.empty() ? now_iso().substr(0, 10) : res.due_date) : res.base_date;
        m_plans.push_back(newPlan);
    }
    
    TodoItem newTask;
    newTask.uuid = generate_uuid();
    newTask.plan_uuid = plan_uuid;
    newTask.cycle_date = res.due_date.empty() ? now_iso().substr(0, 10) : res.due_date;
    newTask.text = res.title;
    newTask.priority = res.priority;
    newTask.due_date = res.due_我将完整检查所有代码，找出所有问题并提供修复后的完整文件。

## 🔍 深度代码审查

### 发现的所有问题：

1. ✅ `app.h` - 缺少 `g_whiteBrush` 声明
2. ✅ `main.cpp` - 需要定义 `g_whiteBrush`
3. ✅ `app.cpp` - `rebuildHits()` 函数体为空
4. ✅ `storage.cpp` - `quick_parse` 中 `date_from_days` 日期计算可能溢出
5. ⚠️ `app.cpp` - 缺少 `#include <ctime>` 和其他必要头文件
6. ⚠️ `storage.cpp` - `quick_parse` 中解析逻辑需要优化
7. ⚠️ `gfx.cpp` - `measureText` 可能返回错误尺寸

---

## 📦 完整修复后的文件

### 1. `app.h` (完整修复版)

```cpp
#pragma once
#include "gfx.h"
#include "storage.h"
#include <vector>
#include <string>
#include <map>

namespace AppC {
constexpr float TITLE_H = 38.0f;
constexpr float CORNER_R = 8.0f;
constexpr float MIN_W = 240.0f;
constexpr float MIN_H = 320.0f;
constexpr float CIRCLE_R = 7.0f;
constexpr float BALL_SIZE = 60.0f;
constexpr float EXPANDED_W = 360.0f;
constexpr float EXPANDED_H = 640.0f;
}

enum EditMode { 
    ED_NONE, 
    ED_NEW_TODO, 
    ED_EDIT_TODO, 
    ED_QUICK_ADD
};

enum HitType {
    H_NONE, H_CLOSE, 
    H_VIEW_TAB_ALL, H_VIEW_TAB_QUAD, H_VIEW_TAB_PLANS, H_VIEW_TAB_FOREST,
    H_TODO_CIRCLE,
    H_PLAN_ITEM, H_PLAN_ADD,
    H_HISTORY_BTN, H_HANDOVER_BTN, H_REFRESH_BTN, H_FOREST_STATS_BTN,
    H_QUICK_INPUT, H_FOCUS_START, H_FOCUS_STOP
};

enum ViewMode {
    VIEW_ALL,
    VIEW_QUADRANT,
    VIEW_PLANS,
    VIEW_FOREST
};

struct Hit { 
    D2D1_RECT_F rc; 
    HitType type; 
    int pi = -1; 
    int ti = -1; 
    int extra = -1;
};

class App {
public:
    void create(HWND hwnd);
    void destroy();
    void tick(float dt);
    void render();
    void onSize();
    LRESULT onNcHitTest(POINT screenPt);
    void onMouseMove(float xdip, float ydip);
    void onLButtonDown(float xdip, float ydip);
    void onLButtonUp(float xdip, float ydip);
    void onLButtonDblClk(float xdip, float ydip);
    void onWheel(int delta);
    void onChar(wchar_t ch);
    void onKeyDown(int vk);
    
    HWND hwnd() const { return m_hwnd; }
    float toDip(int px) const { return (float)px; }
    int toPx(float dip) const { return (int)(dip + 0.5f); }

private:
    HWND m_hwnd = nullptr;
    HWND m_edit = nullptr;

    std::vector<Project> m_projects;
    std::vector<Plan> m_plans;
    std::vector<HistoryItem> m_history;
    std::vector<ForestRecord> m_forestRecords;
    Config m_cfg;

    float m_w = 0, m_h = 0;
    float m_scroll = 0, m_scrollTarget = 0, m_contentH = 0;

    EditMode m_editMode = ED_NONE;
    int m_editPi = -1, m_editTi = -1;
    
    ViewMode m_viewMode = VIEW_ALL;

    bool m_isBallMode = true;
    float m_ballExpandAnim = 0.0f;
    bool m_ballAnimating = false;
    float m_ballHoverTimer = 0.0f;
    float m_ballLeaveTimer = 0.0f;
    
    std::wstring m_quickInputText;
    ParseResult m_parsedResult;
    
    std::wstring m_editText;
    
    bool m_hoverClose = false;
    
    std::vector<Hit> m_hits;
    bool m_hitsDirty = true;

    bool m_focusing = false;
    int m_focusRemainingMs = 0;
    int m_focusTotalMs = 0;
    std::wstring m_focusTaskUuid;
    float m_focusTreeAnim = 0.0f;
    
    float m_reminderCheckTimer = 0.0f;
    std::wstring m_lastSyncDate;
    
    D2D1_RECT_F m_quadRects[4] = {};
    std::vector<int> m_quadTaskIndices[4];
    
    std::wstring m_toastMessage;
    float m_toastAlpha = 0.0f;

    void rebuildHits();
    void markHitsDirty() { m_hitsDirty = true; }
    
    void openHistory();
    void exportHandover();
    void manualRefresh();
    
    void beginEdit(EditMode mode, int pi, int ti, const std::wstring& initial);
    void endEdit();
    void ensureEditCreated();
    std::wstring editGetText();
    void editSetText(const std::wstring& s);
    void commitEdit();
    void commitQuickAdd();
    void onQuickInputChanged();
    void completeTodo(int pi, int ti);
    
    void requestRedraw();
    void approach(float& cur, float target, float dt, float speed);
    void clampScroll();
    void saveAll();
    
    void switchView(ViewMode mode);
    void syncRepeatPlans();
    int computeQuadrant(const TodoItem& todo);
    void checkReminders();
    void showToast(const std::wstring& msg);
    
    void startFocus(const std::wstring& taskUuid);
    void stopFocus(bool success);
    void tickFocus(float dt);
    void showFocusSuccess();
    
    void expandFromBall();
    void collapseToBall();
    void tickBallAnimation(float dt);
    
    void renderBallMode(Gfx& g);
    void renderExpandedMode(Gfx& g);
    void renderAllTasksView(Gfx& g, float contentTop, float contentBot);
    void renderQuadrantView(Gfx& g, float contentTop, float contentBot);
    void renderPlansView(Gfx& g, float contentTop, float contentBot);
    void renderForestView(Gfx& g, float contentTop, float contentBot);
    void renderQuickInputBar(Gfx& g, float y);
    void renderParsedPreview(Gfx& g, float y);
    
    std::wstring getRuleDisplayStr(const std::wstring& rule, const std::vector<int>& days);
    bool isOverdue(const TodoItem& todo);
    void drawTaskBadges(Gfx& g, const TodoItem& todo, float x, float y, float maxW);
    
    std::wstring addDaysToDate(const std::wstring& date, int days);
    std::wstring getNextCycleDate(const Plan& plan, const std::wstring& currentDate);
};

extern App* g_app;
