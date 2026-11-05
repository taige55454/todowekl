#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <string>

enum class FontId {
    FUI_REG,
    FUI_BOLD,
    FUI_SMALL,
    FUI_LARGE,
    EMOJI,
    EMOJI_LARGE,
    COUNT
};

namespace C {
    const D2D1_COLOR_F WHITE = {1.0f, 1.0f, 1.0f, 1.0f};
    const D2D1_COLOR_F BLACK = {0.0f, 0.0f, 0.0f, 1.0f};
    const D2D1_COLOR_F TEXT = {0.15f, 0.15f, 0.15f, 1.0f};
    const D2D1_COLOR_F TEXT_LIGHT = {0.5f, 0.5f, 0.5f, 1.0f};
    const D2D1_COLOR_F GRAY_LIGHT = {0.9f, 0.9f, 0.9f, 1.0f};
    const D2D1_COLOR_F BLUE = {0.0f, 0.48f, 0.8f, 1.0f};
    const D2D1_COLOR_F BLUE_LIGHT = {0.85f, 0.93f, 1.0f, 1.0f};
    const D2D1_COLOR_F GREEN = {0.13f, 0.68f, 0.29f, 1.0f};
    const D2D1_COLOR_F GREEN_LIGHT = {0.85f, 0.98f, 0.88f, 1.0f};
    const D2D1_COLOR_F RED = {0.85f, 0.15f, 0.15f, 1.0f};
    const D2D1_COLOR_F RED_LIGHT = {1.0f, 0.9f, 0.9f, 1.0f};
    const D2D1_COLOR_F YELLOW = {1.0f, 0.76f, 0.03f, 1.0f};
    const D2D1_COLOR_F YELLOW_LIGHT = {1.0f, 0.98f, 0.8f, 1.0f};
    const D2D1_COLOR_F DARK_BG = {0.18f, 0.18f, 0.18f, 1.0f};
    const D2D1_COLOR_F GOLD = {1.0f, 0.84f, 0.0f, 1.0f};
    const D2D1_COLOR_F GRAY = {0.6f, 0.6f, 0.6f, 1.0f};
}

struct TextSize { float width; float height; };

class Gfx {
public:
    void create(HWND hwnd);
    void destroy();
    void resize();
    void beginDraw();
    void endDraw();
    
    float clientW() const { return m_w; }
    float clientH() const { return m_h; }
    
    void fillRect(const D2D1_RECT_F& rc, D2D1_COLOR_F color);
    void fillRoundedRect(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F color);
    void drawRoundedRect(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F color, float strokeWidth);
    void drawLine(D2D1_POINT_2F p0, D2D1_POINT_2F p1, D2D1_COLOR_F color, float strokeWidth);
    void fillCircle(float cx, float cy, float r, D2D1_COLOR_F color);
    void drawCircle(float cx, float cy, float r, D2D1_COLOR_F color, float strokeWidth);
    
    void drawText(const std::wstring& text, FontId fid, D2D1_POINT_2F pos, D2D1_COLOR_F color);
    TextSize measureText(const std::wstring& text, FontId fid);
    
    ID2D1SolidColorBrush* getSolidBrush() { return m_brush; }
    IDWriteTextFormat* getFont(FontId fid) { return m_fonts[(int)fid]; }

private:
    HWND m_hwnd = nullptr;
    ID2D1Factory* m_factory = nullptr;
    ID2D1HwndRenderTarget* m_rt = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    IDWriteFactory* m_dwFactory = nullptr;
    IDWriteTextFormat* m_fonts[(int)FontId::COUNT] = {};
    float m_w = 0, m_h = 0;
    
    void createFonts();
};

extern Gfx g_gfx;
