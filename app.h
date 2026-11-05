#pragma once
#define NOMINMAX
#include <windows.h>
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
extern HBRUSH g_whiteBrush;
