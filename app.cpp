#define NOMINMAX
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
    g_gfx.resize();
    m_w = g_gfx.clientW();
    m_h = g_gfx.clientH();
    markHitsDirty();
    requestRedraw();
}

LRESULT App::onNcHitTest(POINT screenPt) {
    if (m_isBallMode) {
        return HTCAPTION;
    }
    
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    int y = screenPt.y - rc.top;
    float ydip = toDip(y);
    
    // 顶部标题栏左侧区域允许拖拽窗口（避开右侧功能按钮区）
    int x = screenPt.x - rc.left;
    float xdip = toDip(x);
    if (ydip < AppC::TITLE_H && xdip < m_w - 160) {
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
    int y = 0, m = 0, d = 0;
    if (swscanf_s(date.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) {
        return date;
    }
    
    st.wYear = (WORD)y;
    st.wMonth = (WORD)m;
    st.wDay = (WORD)d;
    
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
    int y = 0, m = 0, d = 0;
    if (swscanf_s(currentDate.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) {
        return currentDate;
    }
    
    st.wYear = (WORD)y;
    st.wMonth = (WORD)m;
    st.wDay = (WORD)d;
    
    if (plan.repeat_rule == L"daily") {
        return addDaysToDate(currentDate, 1);
    } else if (plan.repeat_rule == L"weekdays") {
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        FileTimeToSystemTime(&ft, &st);
        int weekday = st.wDayOfWeek; // 0=周日, 5=周五, 6=周六
        
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
        
        if (st.wDay > maxDay) st.wDay = (WORD)maxDay;
        
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
    newTask.due_date = res.due_date;
    newTask.due_time = res.due_time;
    newTask.reminder_time = res.due_time;
    newTask.repeat_rule = res.repeat_rule;
    newTask.repeat_days = res.repeat_days;
    newTask.created = now_iso();
    newTask.done = false;
    newTask.notified = false;
    
    if (!plan_uuid.empty() && !res.due_date.empty()) {
        std::wstring today = now_iso().substr(0, 10);
        if (res.due_date > today) {
            std::wstring hideDate = addDaysToDate(res.due_date, -1);
            if (hideDate >= today) {
                newTask.hide_until = hideDate;
            }
        }
    }
    
    if (m_projects.empty()) {
        Project defaultProj;
        defaultProj.name = L"默认项目";
        m_projects.push_back(defaultProj);
    }
    m_projects[0].todos.insert(m_projects[0].todos.begin(), newTask);
    
    saveAll();
    syncRepeatPlans();
    
    m_quickInputText.clear();
    m_parsedResult = ParseResult();
    markHitsDirty();
    requestRedraw();
}

void App::drawTaskBadges(Gfx& g, const TodoItem& todo, float x, float y, float maxW) {
    float badgeX = x;
    float badgeY = y;
    float badgeH = 18.0f;
    float badgeGap = 4.0f;
    
    bool overdue = isOverdue(todo);
    
    if (todo.priority == 1) {
        std::wstring star = L"⭐";
        auto sz = g.measureText(star, FontId::FUI_SMALL);
        float w = sz.width + 8;
        g.fillRoundedRect({badgeX, badgeY, badgeX + w, badgeY + badgeH}, 4.0f, C::BLUE_LIGHT);
        g.drawText(star, FontId::FUI_SMALL, {badgeX + 4, badgeY + 2}, C::BLUE);
        badgeX += w + badgeGap;
    }
    
    if (!todo.due_date.empty()) {
        std::wstring dateStr = L"📅" + todo.due_date.substr(5);
        
        if (!todo.original_due.empty() && todo.original_due != todo.due_date) {
            dateStr = L"📅" + todo.original_due.substr(5) + L"(延)";
        }
        
        auto sz = g.measureText(dateStr, FontId::FUI_SMALL);
        float w = sz.width + 8;
        
        D2D1_COLOR_F bgColor = overdue ? C::RED_LIGHT : C::BLUE_LIGHT;
        D2D1_COLOR_F fgColor = overdue ? C::RED : C::BLUE;
        
        g.fillRoundedRect({badgeX, badgeY, badgeX + w, badgeY + badgeH}, 4.0f, bgColor);
        g.drawText(dateStr, FontId::FUI_SMALL, {badgeX + 4, badgeY + 2}, fgColor);
        badgeX += w + badgeGap;
    }
    
    if (!todo.due_time.empty()) {
        if (badgeX > x + 100) {
            badgeX = x;
            badgeY += badgeH + 2;
        }
        
        std::wstring timeStr = L"🕒" + todo.due_time;
        auto sz = g.measureText(timeStr, FontId::FUI_SMALL);
        float w = sz.width + 8;
        
        D2D1_COLOR_F bgColor = (overdue && todo.due_date.empty()) ? C::RED_LIGHT : C::GREEN_LIGHT;
        D2D1_COLOR_F fgColor = (overdue && todo.due_date.empty()) ? C::RED : C::GREEN;
        
        g.fillRoundedRect({badgeX, badgeY, badgeX + w, badgeY + badgeH}, 4.0f, bgColor);
        g.drawText(timeStr, FontId::FUI_SMALL, {badgeX + 4, badgeY + 2}, fgColor);
        badgeX += w + badgeGap;
    }
    
    if (!todo.repeat_rule.empty()) {
        std::wstring repeatStr = L"🔁" + getRuleDisplayStr(todo.repeat_rule, todo.repeat_days);
        auto sz = g.measureText(repeatStr, FontId::FUI_SMALL);
        float w = sz.width + 8;
        
        g.fillRoundedRect({badgeX, badgeY, badgeX + w, badgeY + badgeH}, 4.0f, C::YELLOW_LIGHT);
        g.drawText(repeatStr, FontId::FUI_SMALL, {badgeX + 4, badgeY + 2}, C::TEXT);
    }
}

void App::onChar(wchar_t ch) {
    if (m_editMode == ED_QUICK_ADD) {
        if (ch == L'\r') {
            commitQuickAdd();
            endEdit();
        } else if (ch == 27) {
            m_quickInputText.clear();
            endEdit();
        } else if (ch == 8) {
            if (!m_quickInputText.empty()) {
                m_quickInputText.pop_back();
                onQuickInputChanged();
            }
        } else if (ch >= 32) {
            m_quickInputText += ch;
            onQuickInputChanged();
        }
    }
}

void App::onKeyDown(int vk) {
    if (vk == VK_F5) {
        manualRefresh();
    } else if (vk == VK_ESCAPE && m_focusing) {
        stopFocus(false);
    }
}

void App::onLButtonDown(float xdip, float ydip) {
    if (m_isBallMode) {
        expandFromBall();
        return;
    }
    
    for (auto& hit : m_hits) {
        if (xdip >= hit.rc.left && xdip <= hit.rc.right &&
            ydip >= hit.rc.top && ydip <= hit.rc.bottom) {
            
            switch (hit.type) {
                case H_CLOSE:
                    collapseToBall();
                    break;
                    
                case H_REFRESH_BTN:
                    manualRefresh();
                    break;
                    
                case H_HANDOVER_BTN:
                    exportHandover();
                    break;
                    
                case H_HISTORY_BTN:
                    openHistory();
                    break;
                    
                case H_VIEW_TAB_ALL:
                    switchView(VIEW_ALL);
                    break;
                    
                case H_VIEW_TAB_QUAD:
                    switchView(VIEW_QUADRANT);
                    break;
                    
                case H_VIEW_TAB_PLANS:
                    switchView(VIEW_PLANS);
                    break;
                    
                case H_VIEW_TAB_FOREST:
                    switchView(VIEW_FOREST);
                    break;
                    
                case H_TODO_CIRCLE:
                    if (hit.pi >= 0 && hit.ti >= 0) {
                        completeTodo(hit.pi, hit.ti);
                    }
                    break;
                    
                case H_PLAN_ADD:
                    beginEdit(ED_QUICK_ADD, -1, -1, L"");
                    break;
                    
                case H_QUICK_INPUT:
                    beginEdit(ED_QUICK_ADD, -1, -1, m_quickInputText);
                    break;
                    
                case H_FOCUS_START:
                    if (hit.pi >= 0 && hit.pi < (int)m_projects.size()) {
                        auto& todos = m_projects[hit.pi].todos;
                        if (hit.ti >= 0 && hit.ti < (int)todos.size()) {
                            startFocus(todos[hit.ti].uuid);
                        } else if (!todos.empty()) {
                            startFocus(todos[0].uuid);
                        }
                    }
                    break;
                    
                case H_FOCUS_STOP:
                    stopFocus(false);
                    break;
                    
                default:
                    break;
            }
            
            break;
        }
    }
}

void App::onLButtonUp(float xdip, float ydip) {
}

void App::onLButtonDblClk(float xdip, float ydip) {
}

void App::completeTodo(int pi, int ti) {
    if (pi < 0 || pi >= (int)m_projects.size()) return;
    auto& proj = m_projects[pi];
    if (ti < 0 || ti >= (int)proj.todos.size()) return;
    
    auto& todo = proj.todos[ti];
    
    todo.done = true;
    
    HistoryItem h;
    h.plan_uuid = todo.plan_uuid;
    h.cycle_date = todo.cycle_date;
    h.project = proj.name;
    h.text = todo.text;
    h.created = todo.created;
    h.completed = now_iso();
    h.due_date = todo.due_date;
    m_history.push_back(h);
    
    if (!todo.plan_uuid.empty()) {
        Plan* plan = nullptr;
        for (auto& p : m_plans) {
            if (p.plan_uuid == todo.plan_uuid) {
                plan = &p;
                break;
            }
        }
        
        if (plan) {
            std::wstring nextDate = getNextCycleDate(*plan, todo.due_date.empty() ? now_iso().substr(0, 10) : todo.due_date);
            
            TodoItem nextTodo;
            nextTodo.uuid = generate_uuid();
            nextTodo.plan_uuid = plan->plan_uuid;
            nextTodo.cycle_date = nextDate;
            nextTodo.text = plan->text;
            nextTodo.priority = plan->priority;
            nextTodo.due_date = nextDate;
            nextTodo.due_time = plan->due_time;
            nextTodo.reminder_time = plan->due_time;
            nextTodo.repeat_rule = plan->repeat_rule;
            nextTodo.repeat_days = plan->repeat_days;
            nextTodo.created = now_iso();
            nextTodo.done = false;
            nextTodo.notified = false;
            
            std::wstring today = now_iso().substr(0, 10);
            if (nextDate > today) {
                std::wstring hideDate = addDaysToDate(nextDate, -1);
                if (hideDate >= today) {
                    nextTodo.hide_until = hideDate;
                }
            }
            
            proj.todos.insert(proj.todos.begin(), nextTodo);
            // 因为在头部插入了新任务，当前完成了的索引向后偏移 1
            ti++;
        }
    }
    
    proj.todos.erase(proj.todos.begin() + ti);
    
    saveAll();
    markHitsDirty();
    requestRedraw();
}

void App::beginEdit(EditMode mode, int pi, int ti, const std::wstring& initial) {
    m_editMode = mode;
    m_editPi = pi;
    m_editTi = ti;
    m_editText = initial;
    
    if (mode == ED_QUICK_ADD) {
        m_quickInputText = initial;
        onQuickInputChanged();
    }
    
    requestRedraw();
}

void App::endEdit() {
    m_editMode = ED_NONE;
    m_editPi = -1;
    m_editTi = -1;
    m_editText.clear();
    
    requestRedraw();
}

void App::commitEdit() {
    if (m_editMode == ED_NONE) return;
    
    if (m_editMode == ED_QUICK_ADD) {
        commitQuickAdd();
    }
    
    endEdit();
}

void App::openHistory() {
    std::wstring msg = L"📚 历史记录（最近30条）\n\n";
    
    std::map<std::wstring, std::vector<HistoryItem>> grouped;
    size_t start = m_history.size() > 30 ? m_history.size() - 30 : 0;
    for (size_t i = start; i < m_history.size(); ++i) {
        std::wstring date = m_history[i].completed.substr(0, 10);
        grouped[date].push_back(m_history[i]);
    }
    
    for (auto it = grouped.rbegin(); it != grouped.rend(); ++it) {
        msg += L"📂 " + it->first + L"  (完成 " + std::to_wstring(it->second.size()) + L" 项)\n";
        for (auto& h : it->second) {
            msg += L"  ✅ " + h.text;
            if (!h.cycle_date.empty() && h.cycle_date != it->first) {
                msg += L" [周期:" + h.cycle_date.substr(5) + L"]";
            }
            msg += L"\n";
        }
        msg += L"\n";
    }
    
    MessageBoxW(m_hwnd, msg.c_str(), L"历史记录", MB_OK | MB_ICONINFORMATION);
}

void App::exportHandover() {
    std::wstring text = L"【前台未完成待办交接】\n";
    text += L"交接时间：" + now_iso().substr(0, 16) + L"\n";
    text += L"------------------------\n";
    
    std::wstring today = now_iso().substr(0, 10);
    std::wstring tomorrow = addDaysToDate(today, 1);
    
    int count = 0;
    for (auto& proj : m_projects) {
        for (auto& todo : proj.todos) {
            if (todo.done) continue;
            
            std::wstring due = todo.due_date;
            if (due.empty() || due <= tomorrow) {
                count++;
                text += std::to_wstring(count) + L". " + todo.text;
                
                if (!due.empty() || !todo.due_time.empty()) {
                    text += L" （截止：";
                    if (!due.empty()) text += due.substr(5);
                    if (!todo.due_time.empty()) text += L" " + todo.due_time;
                    text += L"）";
                }
                
                if (!todo.repeat_rule.empty()) {
                    text += L" [⚠️" + getRuleDisplayStr(todo.repeat_rule, todo.repeat_days) + L"]";
                }
                
                text += L"\n";
            }
        }
    }
    
    if (count == 0) {
        text += L"当前无紧急未完成事项，辛苦了！🎉\n";
    }
    
    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        size_t len = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            void* locked = GlobalLock(hMem);
            if (locked) {
                memcpy(locked, text.c_str(), len);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }
    
    showToast(L"✅ 交接清单已复制");
}

void App::manualRefresh() {
    m_lastSyncDate.clear();
    syncRepeatPlans();
    markHitsDirty();
    requestRedraw();
}

void App::switchView(ViewMode mode) {
    m_viewMode = mode;
    m_scroll = 0;
    m_scrollTarget = 0;
    markHitsDirty();
    requestRedraw();
}

void App::startFocus(const std::wstring& taskUuid) {
    m_focusing = true;
    m_focusTaskUuid = taskUuid;
    m_focusTotalMs = m_cfg.focus_minutes * 60 * 1000;
    m_focusRemainingMs = m_focusTotalMs;
    m_focusTreeAnim = 0.0f;
    requestRedraw();
}

void App::stopFocus(bool success) {
    if (!m_focusing) return;
    
    m_focusing = false;
    
    ForestRecord rec;
    rec.timestamp = now_iso();
    rec.status = success ? L"healthy" : L"withered";
    rec.minutes = m_cfg.focus_minutes;
    rec.task = L"自由专注";
    
    int targetPi = -1, targetTi = -1;
    for (size_t pi = 0; pi < m_projects.size(); ++pi) {
        for (size_t ti = 0; ti < m_projects[pi].todos.size(); ++ti) {
            if (m_projects[pi].todos[ti].uuid == m_focusTaskUuid) {
                rec.task = m_projects[pi].todos[ti].text;
                targetPi = (int)pi;
                targetTi = (int)ti;
                break;
            }
        }
        if (targetPi != -1) break;
    }
    
    if (success && targetPi != -1 && targetTi != -1) {
        completeTodo(targetPi, targetTi);
    }
    
    m_forestRecords.push_back(rec);
    saveAll();
    
    if (success) {
        showFocusSuccess();
    }
    
    requestRedraw();
}

void App::tickFocus(float dt) {
    if (m_focusRemainingMs > 0) {
        m_focusRemainingMs -= (int)(dt * 1000);
        if (m_focusRemainingMs < 0) m_focusRemainingMs = 0;
        
        m_focusTreeAnim = 1.0f - (float)m_focusRemainingMs / (float)m_focusTotalMs;
        
        if (m_focusRemainingMs <= 0) {
            stopFocus(true);
        }
        
        requestRedraw();
    }
}

void App::showFocusSuccess() {
    MessageBoxW(m_hwnd, 
                L"专注完成！\n\n路漫漫其修远兮\n休息一下吧！", 
                L"🌲 专注成功", 
                MB_OK | MB_ICONINFORMATION);
}

void App::expandFromBall() {
    if (!m_isBallMode || m_ballAnimating) return;
    
    m_ballAnimating = true;
    m_isBallMode = false;
    
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int expandW = toPx(AppC::EXPANDED_W);
    int expandH = toPx(AppC::EXPANDED_H);
    int targetX = workArea.right - expandW - toPx(20);
    int targetY = workArea.top + toPx(60);
    
    SetWindowPos(m_hwnd, HWND_TOPMOST, targetX, targetY, expandW, expandH, 
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    m_w = AppC::EXPANDED_W;
    m_h = AppC::EXPANDED_H;
    
    markHitsDirty();
    requestRedraw();
}

void App::collapseToBall() {
    if (m_isBallMode || m_ballAnimating) return;
    
    m_ballAnimating = true;
    m_isBallMode = true;
    
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int ballSize = toPx(AppC::BALL_SIZE);
    int targetX = workArea.right - ballSize - toPx(20);
    int targetY = workArea.top + toPx(60);
    
    SetWindowPos(m_hwnd, HWND_TOPMOST, targetX, targetY, ballSize, ballSize, 
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    m_w = AppC::BALL_SIZE;
    m_h = AppC::BALL_SIZE;
    
    markHitsDirty();
    requestRedraw();
}

void App::tickBallAnimation(float dt) {
    if (!m_ballAnimating) return;
    
    float target = m_isBallMode ? 0.0f : 1.0f;
    approach(m_ballExpandAnim, target, dt, 8.0f);
    
    if (std::abs(m_ballExpandAnim - target) < 0.01f) {
        m_ballExpandAnim = target;
        m_ballAnimating = false;
    }
    
    requestRedraw();
}

void App::tick(float dt) {
    tickBallAnimation(dt);
    
    if (m_isBallMode && !m_ballAnimating) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(m_hwnd, &pt);
        float mx = toDip(pt.x);
        float my = toDip(pt.y);
        
        bool hovering = (mx >= 0 && mx <= m_w && my >= 0 && my <= m_h);
        
        if (hovering) {
            m_ballHoverTimer += dt;
            if (m_ballHoverTimer >= 0.4f) {
                expandFromBall();
                m_ballHoverTimer = 0;
            }
        } else {
            m_ballHoverTimer = 0;
        }
    }
    
    if (!m_isBallMode && !m_ballAnimating) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(m_hwnd, &pt);
        float mx = toDip(pt.x);
        float my = toDip(pt.y);
        
        bool outside = (mx < -10 || mx > m_w + 10 || my < -10 || my > m_h + 10);
        
        if (outside && m_editMode == ED_NONE && !m_focusing) {
            m_ballLeaveTimer += dt;
            if (m_ballLeaveTimer >= 0.8f) {
                collapseToBall();
                m_ballLeaveTimer = 0;
            }
        } else {
            m_ballLeaveTimer = 0;
        }
    }
    
    m_reminderCheckTimer += dt;
    if (m_reminderCheckTimer >= 30.0f) {
        m_reminderCheckTimer = 0;
        checkReminders();
        
        std::wstring today = now_iso().substr(0, 10);
        if (m_lastSyncDate != today) {
            syncRepeatPlans();
        }
    }
    
    if (m_focusing) {
        tickFocus(dt);
    }
    
    if (m_toastAlpha > 0) {
        m_toastAlpha -= dt * 0.5f;
        if (m_toastAlpha < 0) m_toastAlpha = 0;
        requestRedraw();
    }
    
    approach(m_scroll, m_scrollTarget, dt, 10.0f);
}

void App::rebuildHits() {
    m_hits.clear();
    m_hitsDirty = false;
}

void App::render() {
    // 每次重绘前必须清空热区，因为各视图的 render 会重新 push_back 当前帧的热区
    rebuildHits();
    
    Gfx& g = g_gfx;
    g.beginDraw();
    
    if (m_isBallMode) {
        renderBallMode(g);
    } else {
        renderExpandedMode(g);
    }
    
    if (m_toastAlpha > 0) {
        std::wstring msg = m_toastMessage;
        auto sz = g.measureText(msg, FontId::FUI_REG);
        float toastW = sz.width + 40;
        float toastH = 36;
        float toastX = (m_w - toastW) / 2;
        float toastY = m_h * 0.3f;
        
        D2D1_COLOR_F bg = C::DARK_BG;
        bg.a = m_toastAlpha * 0.9f;
        g.fillRoundedRect({toastX, toastY, toastX + toastW, toastY + toastH}, 8.0f, bg);
        
        D2D1_COLOR_F fg = C::WHITE;
        fg.a = m_toastAlpha;
        g.drawText(msg, FontId::FUI_REG, {toastX + 20, toastY + 9}, fg);
    }
    
    g.endDraw();
}

void App::renderBallMode(Gfx& g) {
    float cx = m_w / 2;
    float cy = m_h / 2;
    float r = m_w / 2 - 2;
    
    D2D1_COLOR_F ballBg = C::DARK_BG;
    ballBg.a = 0.85f;
    g.fillCircle(cx, cy, r, ballBg);
    
    D2D1_COLOR_F borderColor = {1.0f, 0.84f, 0.0f, 0.9f};
    g.drawCircle(cx, cy, r, borderColor, 2.0f);
    
    std::wstring icon = L"🥚";
    if (m_focusing) {
        if (m_focusTreeAnim < 0.25f) icon = L"🌰";
        else if (m_focusTreeAnim < 0.6f) icon = L"🌱";
        else if (m_focusTreeAnim < 0.9f) icon = L"🌿";
        else icon = L"🌲";
    }
    
    auto sz = g.measureText(icon, FontId::EMOJI);
    g.drawText(icon, FontId::EMOJI, {cx - sz.width / 2, cy - sz.height / 2 - (m_focusing ? 8.0f : 0.0f)}, C::WHITE);
    
    if (m_focusing) {
        int mins = m_focusRemainingMs / 60000;
        int secs = (m_focusRemainingMs % 60000) / 1000;
        wchar_t timeBuf[16];
        swprintf_s(timeBuf, L"%02d:%02d", mins, secs);
        
        auto tsz = g.measureText(timeBuf, FontId::FUI_BOLD);
        D2D1_COLOR_F timeColor = {1.0f, 0.84f, 0.0f, 1.0f};
        g.drawText(timeBuf, FontId::FUI_BOLD, {cx - tsz.width / 2, cy + sz.height / 2 - 2}, timeColor);
    }
}

void App::renderExpandedMode(Gfx& g) {
    D2D1_COLOR_F panelBg = {0.98f, 0.98f, 0.99f, 0.99f};
    g.fillRoundedRect({0, 0, m_w, m_h}, AppC::CORNER_R, panelBg);
    
    D2D1_COLOR_F borderColor = {0.7f, 0.7f, 0.7f, 0.8f};
    g.drawRoundedRect({0, 0, m_w, m_h}, AppC::CORNER_R, borderColor, 1.0f);
    
    float titleH = AppC::TITLE_H;
    std::wstring title = m_cfg.title_prefix;
    g.drawText(title, FontId::FUI_BOLD, {12, 10}, C::TEXT);
    
    float btnY = 8;
    float btnX = m_w - 40;
    
    if (m_hoverClose) {
        g.fillCircle(btnX + 10, btnY + 10, 10, C::RED_LIGHT);
    }
    g.drawText(L"×", FontId::FUI_BOLD, {btnX + 4, btnY + 2}, m_hoverClose ? C::RED : C::TEXT_LIGHT);
    m_hits.push_back({{btnX, btnY, btnX + 20, btnY + 20}, H_CLOSE});
    btnX -= 35;
    
    g.drawText(L"🔄", FontId::FUI_SMALL, {btnX, btnY + 2}, C::TEXT);
    m_hits.push_back({{btnX, btnY, btnX + 28, btnY + 20}, H_REFRESH_BTN});
    btnX -= 35;
    
    g.drawText(L"📋", FontId::FUI_SMALL, {btnX, btnY + 2}, C::TEXT);
    m_hits.push_back({{btnX, btnY, btnX + 28, btnY + 20}, H_HANDOVER_BTN});
    btnX -= 35;
    
    g.drawText(L"📚", FontId::FUI_SMALL, {btnX, btnY + 2}, C::TEXT);
    m_hits.push_back({{btnX, btnY, btnX + 28, btnY + 20}, H_HISTORY_BTN});
    
    float tabY = titleH + 6;
    float tabH = 32;
    float tabW = (m_w - 32) / 4;
    float tabX = 8;
    
    struct TabDef { ViewMode mode; const wchar_t* label; HitType hit; };
    TabDef tabs[] = {
        {VIEW_ALL, L"📋 当前", H_VIEW_TAB_ALL},
        {VIEW_QUADRANT, L"🎯 象限", H_VIEW_TAB_QUAD},
        {VIEW_PLANS, L"🗓️ 周期", H_VIEW_TAB_PLANS},
        {VIEW_FOREST, L"🌲 森林", H_VIEW_TAB_FOREST}
    };
    
    for (auto& tab : tabs) {
        bool active = (m_viewMode == tab.mode);
        
        D2D1_COLOR_F bg = active ? C::GREEN : C::GRAY_LIGHT;
        D2D1_COLOR_F fg = active ? C::WHITE : C::TEXT;
        
        g.fillRoundedRect({tabX, tabY, tabX + tabW - 4, tabY + tabH}, 6.0f, bg);
        
        auto sz = g.measureText(tab.label, FontId::FUI_SMALL);
        g.drawText(tab.label, FontId::FUI_SMALL, 
                   {tabX + (tabW - 4 - sz.width) / 2, tabY + (tabH - sz.height) / 2}, fg);
        
        m_hits.push_back({{tabX, tabY, tabX + tabW - 4, tabY + tabH}, tab.hit});
        tabX += tabW;
    }
    
    float contentTop = tabY + tabH + 8;
    float contentBot = m_h - 50;
    
    switch (m_viewMode) {
        case VIEW_ALL:
            renderAllTasksView(g, contentTop, contentBot);
            break;
        case VIEW_QUADRANT:
            renderQuadrantView(g, contentTop, contentBot);
            break;
        case VIEW_PLANS:
            renderPlansView(g, contentTop, contentBot);
            break;
        case VIEW_FOREST:
            renderForestView(g, contentTop, contentBot);
            break;
    }
    
    renderQuickInputBar(g, contentBot);
    
    if (!m_parsedResult.labels.empty()) {
        renderParsedPreview(g, contentBot - 20);
    }
}

void App::renderAllTasksView(Gfx& g, float contentTop, float contentBot) {
    float y = contentTop - m_scroll;
    float maxW = m_w - 24;
    
    std::wstring today = now_iso().substr(0, 10);
    
    for (size_t pi = 0; pi < m_projects.size(); ++pi) {
        auto& proj = m_projects[pi];
        for (size_t ti = 0; ti < proj.todos.size(); ++ti) {
            auto& todo = proj.todos[ti];
            
            if (!todo.hide_until.empty() && today < todo.hide_until) {
                continue;
            }
            
            float rowH = 56;
            
            if (y + rowH < contentTop || y > contentBot) {
                y += rowH + 4;
                continue;
            }
            
            D2D1_COLOR_F cardBg = C::WHITE;
            g.fillRoundedRect({12, y, m_w - 12, y + rowH}, 6.0f, cardBg);
            
            g.drawLine({12, y + rowH}, {m_w - 12, y + rowH}, C::GRAY_LIGHT, 1.0f);
            
            float circleX = 24;
            float circleY = y + 18;
            float circleR = AppC::CIRCLE_R;
            
            g.drawCircle(circleX, circleY, circleR, C::TEXT_LIGHT, 2.0f);
            m_hits.push_back({{circleX - circleR - 4, y, circleX + circleR + 4, y + rowH}, 
                              H_TODO_CIRCLE, (int)pi, (int)ti});
            
            float textX = circleX + circleR + 14;
            g.drawText(todo.text, FontId::FUI_REG, {textX, y + 8}, C::TEXT);
            
            drawTaskBadges(g, todo, textX, y + 28, maxW - textX - 12);
            
            y += rowH + 4;
        }
    }
    
    m_contentH = y + m_scroll - contentTop + 20;
}

void App::renderQuadrantView(Gfx& g, float contentTop, float contentBot) {
    float availW = m_w - 24;
    float availH = contentBot - contentTop - 8;
    float quadW = (availW - 8) / 2;
    float quadH = (availH - 8) / 2;
    
    struct QuadDef {
        int index;
        const wchar_t* title;
        D2D1_COLOR_F bgColor;
        D2D1_COLOR_F headerColor;
        float x, y;
    };
    
    QuadDef quads[] = {
        {1, L"🔥 重要且紧急", {1.0f, 0.92f, 0.93f, 1.0f}, {1.0f, 0.8f, 0.82f, 1.0f}, 12, contentTop},
        {2, L"⭐ 重要不紧急", {0.91f, 0.96f, 0.91f, 1.0f}, {0.78f, 0.90f, 0.79f, 1.0f}, 12 + quadW + 4, contentTop},
        {3, L"⚡ 紧急不重要", {1.0f, 0.97f, 0.88f, 1.0f}, {1.0f, 0.88f, 0.51f, 1.0f}, 12, contentTop + quadH + 4},
        {4, L"🍃 不重要不紧急", {0.96f, 0.96f, 0.96f, 1.0f}, {0.88f, 0.88f, 0.88f, 1.0f}, 12 + quadW + 4, contentTop + quadH + 4}
    };
    
    for (int i = 0; i < 4; ++i) {
        m_quadTaskIndices[i].clear();
    }
    
    std::wstring today = now_iso().substr(0, 10);
    for (size_t pi = 0; pi < m_projects.size(); ++pi) {
        auto& proj = m_projects[pi];
        for (size_t ti = 0; ti < proj.todos.size(); ++ti) {
            auto& todo = proj.todos[ti];
            if (!todo.hide_until.empty() && today < todo.hide_until) {
                continue;
            }
            
            int quad = computeQuadrant(todo);
            m_quadTaskIndices[quad - 1].push_back((int)(pi * 1000 + ti));
        }
    }
    
    for (auto& q : quads) {
        float x = q.x;
        float y = q.y;
        
        g.fillRoundedRect({x, y, x + quadW, y + quadH}, 6.0f, q.bgColor);
        m_quadRects[q.index - 1] = {x, y, x + quadW, y + quadH};
        
        g.fillRoundedRect({x, y, x + quadW, y + 28}, 4.0f, q.headerColor);
        auto sz = g.measureText(q.title, FontId::FUI_SMALL);
        g.drawText(q.title, FontId::FUI_SMALL, {x + (quadW - sz.width) / 2, y + 6}, C::TEXT);
        
        float itemY = y + 32;
        for (int idx : m_quadTaskIndices[q.index - 1]) {
            int pi = idx / 1000;
            int ti = idx % 1000;
            
            if (pi >= (int)m_projects.size() || ti >= (int)m_projects[pi].todos.size()) continue;
            auto& todo = m_projects[pi].todos[ti];
            
            if (itemY + 30 > y + quadH) break;
            
            std::wstring display = L"◯ " + todo.text;
            if (display.size() > 20) display = display.substr(0, 17) + L"...";
            
            if (!todo.due_time.empty()) {
                display += L" 🕒" + todo.due_time;
            }
            
            g.drawText(display, FontId::FUI_SMALL, {x + 8, itemY}, C::TEXT);
            
            m_hits.push_back({{x, itemY, x + quadW, itemY + 24}, H_TODO_CIRCLE, pi, ti});
            
            itemY += 26;
        }
    }
}

void App::renderPlansView(Gfx& g, float contentTop, float contentBot) {
    float y = contentTop;
    
    float btnH = 36;
    D2D1_COLOR_F btnBg = C::GREEN_LIGHT;
    g.fillRoundedRect({12, y, m_w - 12, y + btnH}, 6.0f, btnBg);
    
    std::wstring btnText = L"➕ 添加周期计划";
    auto sz = g.measureText(btnText, FontId::FUI_REG);
    g.drawText(btnText, FontId::FUI_REG, {(m_w - sz.width) / 2, y + (btnH - sz.height) / 2}, C::GREEN);
    
    m_hits.push_back({{12, y, m_w - 12, y + btnH}, H_PLAN_ADD});
    y += btnH + 8;
    
    for (size_t i = 0; i < m_plans.size(); ++i) {
        auto& plan = m_plans[i];
        float rowH = 52;
        
        if (y + rowH > contentBot) break;
        
        g.fillRoundedRect({12, y, m_w - 12, y + rowH}, 6.0f, C::WHITE);
        
        g.drawText(plan.text, FontId::FUI_REG, {24, y + 8}, C::TEXT);
        
        std::wstring ruleStr = getRuleDisplayStr(plan.repeat_rule, plan.repeat_days);
        if (!plan.due_time.empty()) {
            ruleStr += L" 🕒" + plan.due_time;
        }
        if (plan.priority == 1) {
            ruleStr = L"⭐ " + ruleStr;
        }
        
        g.drawText(ruleStr, FontId::FUI_SMALL, {24, y + 30}, C::TEXT_LIGHT);
        
        m_hits.push_back({{12, y, m_w - 12, y + rowH}, H_PLAN_ITEM, -1, (int)i});
        
        y += rowH + 4;
    }
    
    m_contentH = y - contentTop + 20;
}

void App::renderForestView(Gfx& g, float contentTop, float contentBot) {
    float centerX = m_w / 2;
    float y = contentTop + 20;
    
    if (!m_focusing) {
        std::wstring hint = L"选择一个任务开始专注";
        auto sz = g.measureText(hint, FontId::FUI_REG);
        g.drawText(hint, FontId::FUI_REG, {centerX - sz.width / 2, y}, C::TEXT_LIGHT);
        y += 40;
        
        int count = 0;
        for (size_t pi = 0; pi < m_projects.size(); ++pi) {
            auto& proj = m_projects[pi];
            for (size_t ti = 0; ti < proj.todos.size(); ++ti) {
                if (count >= 5) break;
                auto& todo = proj.todos[ti];
                
                std::wstring taskText = todo.text;
                if (taskText.size() > 24) taskText = taskText.substr(0, 21) + L"...";
                
                float btnH = 36;
                D2D1_COLOR_F btnBg = C::BLUE_LIGHT;
                g.fillRoundedRect({20, y, m_w - 20, y + btnH}, 6.0f, btnBg);
                
                auto tsz = g.measureText(taskText, FontId::FUI_REG);
                g.drawText(taskText, FontId::FUI_REG, {centerX - tsz.width / 2, y + (btnH - tsz.height) / 2}, C::BLUE);
                
                m_hits.push_back({{20, y, m_w - 20, y + btnH}, H_FOCUS_START, (int)pi, (int)ti});
                
                y += btnH + 8;
                count++;
            }
        }
    } else {
        std::wstring treeIcon;
        if (m_focusTreeAnim < 0.25f) treeIcon = L"🌰";
        else if (m_focusTreeAnim < 0.6f) treeIcon = L"🌱";
        else if (m_focusTreeAnim < 0.9f) treeIcon = L"🌿";
        else treeIcon = L"🌲";
        
        auto sz = g.measureText(treeIcon, FontId::EMOJI_LARGE);
        g.drawText(treeIcon, FontId::EMOJI_LARGE, {centerX - sz.width / 2, y}, C::TEXT);
        y += sz.height + 20;
        
        int mins = m_focusRemainingMs / 60000;
        int secs = (m_focusRemainingMs % 60000) / 1000;
        wchar_t timeBuf[16];
        swprintf_s(timeBuf, L"%02d:%02d", mins, secs);
        
        auto tsz = g.measureText(timeBuf, FontId::FUI_LARGE);
        g.drawText(timeBuf, FontId::FUI_LARGE, {centerX - tsz.width / 2, y}, C::GREEN);
        y += tsz.height + 20;
        
        float barW = m_w - 60;
        float barH = 6;
        float barX = 30;
        float progress = m_focusTreeAnim;
        
        g.fillRoundedRect({barX, y, barX + barW, y + barH}, 3.0f, C::GRAY_LIGHT);
        g.fillRoundedRect({barX, y, barX + barW * progress, y + barH}, 3.0f, C::GREEN);
        y += barH + 30;
        
        float btnH = 36;
        D2D1_COLOR_F btnBg = C::RED_LIGHT;
        g.fillRoundedRect({centerX - 60, y, centerX + 60, y + btnH}, 6.0f, btnBg);
        
        std::wstring btnText = L"⚠️ 放弃";
        auto bsz = g.measureText(btnText, FontId::FUI_REG);
        g.drawText(btnText, FontId::FUI_REG, {centerX - bsz.width / 2, y + (btnH - bsz.height) / 2}, C::RED);
        
        m_hits.push_back({{centerX - 60, y, centerX + 60, y + btnH}, H_FOCUS_STOP});
    }
}

void App::renderQuickInputBar(Gfx& g, float y) {
    float inputH = 38;
    float inputY = y + 4;
    
    D2D1_COLOR_F borderCol = (m_editMode == ED_QUICK_ADD) ? C::GREEN : C::GRAY_LIGHT;
    g.fillRoundedRect({12, inputY, m_w - 12, inputY + inputH}, 6.0f, C::WHITE);
    g.drawRoundedRect({12, inputY, m_w - 12, inputY + inputH}, 6.0f, borderCol, 1.5f);
    
    std::wstring displayText;
    if (m_quickInputText.empty()) {
        displayText = (m_editMode == ED_QUICK_ADD) ? L"| 请输入内容并按回车..." : L"💡 快速新增 (如: ! 每月1号, 15号交租)";
    } else {
        displayText = m_quickInputText + (m_editMode == ED_QUICK_ADD ? L"|" : L"");
    }
    D2D1_COLOR_F textColor = m_quickInputText.empty() ? C::TEXT_LIGHT : C::TEXT;
    
    g.drawText(displayText, FontId::FUI_REG, {20, inputY + 10}, textColor);
    
    m_hits.push_back({{12, inputY, m_w - 12, inputY + inputH}, H_QUICK_INPUT});
}

void App::renderParsedPreview(Gfx& g, float y) {
    if (m_parsedResult.labels.empty()) return;
    
    std::wstring preview = L"智能识别: ";
    for (size_t i = 0; i < m_parsedResult.labels.size(); ++i) {
        if (i > 0) preview += L" | ";
        preview += m_parsedResult.labels[i];
    }
    
    g.drawText(preview, FontId::FUI_SMALL, {16, y}, C::GREEN);
}
