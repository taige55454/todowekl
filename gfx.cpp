#include "gfx.h"
#include <algorithm>

Gfx g_gfx;

void Gfx::create(HWND hwnd) {
    m_hwnd = hwnd;
    
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_factory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), 
                        (IUnknown**)&m_dwFactory);
    
    RECT rc;
    GetClientRect(hwnd, &rc);
    m_w = (float)(rc.right - rc.left);
    m_h = (float)(rc.bottom - rc.top);
    
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    props.dpiX = 96.0f;
    props.dpiY = 96.0f;
    
    m_factory->CreateHwndRenderTarget(
        props,
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU((UINT32)m_w, (UINT32)m_h)),
        &m_rt
    );
    
    m_rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1.0f), &m_brush);
    createFonts();
}

void Gfx::createFonts() {
    struct FontDef {
        FontId id;
        const wchar_t* family;
        float size;
        DWRITE_FONT_WEIGHT weight;
    };
    
    FontDef defs[] = {
        {FontId::FUI_REG, L"Microsoft YaHei UI", 14.0f, DWRITE_FONT_WEIGHT_NORMAL},
        {FontId::FUI_BOLD, L"Microsoft YaHei UI", 16.0f, DWRITE_FONT_WEIGHT_BOLD},
        {FontId::FUI_SMALL, L"Microsoft YaHei UI", 12.0f, DWRITE_FONT_WEIGHT_NORMAL},
        {FontId::FUI_LARGE, L"Microsoft YaHei UI", 24.0f, DWRITE_FONT_WEIGHT_BOLD},
        {FontId::EMOJI, L"Segoe UI Emoji", 32.0f, DWRITE_FONT_WEIGHT_NORMAL},
        {FontId::EMOJI_LARGE, L"Segoe UI Emoji", 64.0f, DWRITE_FONT_WEIGHT_NORMAL},
    };
    
    for (auto& def : defs) {
        m_dwFactory->CreateTextFormat(
            def.family, nullptr, def.weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            def.size, L"", &m_fonts[(int)def.id]
        );
    }
}

void Gfx::destroy() {
    for (auto& f : m_fonts) {
        if (f) f->Release();
        f = nullptr;
    }
    if (m_brush) m_brush->Release();
    if (m_rt) m_rt->Release();
    if (m_dwFactory) m_dwFactory->Release();
    if (m_factory) m_factory->Release();
}

void Gfx::resize() {
    if (!m_rt) return;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    m_w = (float)(rc.right - rc.left);
    m_h = (float)(rc.bottom - rc.top);
    m_rt->Resize(D2D1::SizeU((UINT32)m_w, (UINT32)m_h));
}

void Gfx::beginDraw() {
    if (m_rt) m_rt->BeginDraw();
}

void Gfx::endDraw() {
    if (m_rt) m_rt->EndDraw();
}

void Gfx::fillRect(const D2D1_RECT_F& rc, D2D1_COLOR_F color) {
    m_brush->SetColor(color);
    m_rt->FillRectangle(rc, m_brush);
}

void Gfx::fillRoundedRect(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F color) {
    m_brush->SetColor(color);
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(rc, radius, radius), m_brush);
}

void Gfx::drawRoundedRect(const D2D1_RECT_F& rc, float radius, D2D1_COLOR_F color, float strokeWidth) {
    m_brush->SetColor(color);
    m_rt->DrawRoundedRectangle(D2D1::RoundedRect(rc, radius, radius), m_brush, strokeWidth);
}

void Gfx::drawLine(D2D1_POINT_2F p0, D2D1_POINT_2F p1, D2D1_COLOR_F color, float strokeWidth) {
    m_brush->SetColor(color);
    m_rt->DrawLine(p0, p1, m_brush, strokeWidth);
}

void Gfx::fillCircle(float cx, float cy, float r, D2D1_COLOR_F color) {
    m_brush->SetColor(color);
    m_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), m_brush);
}

void Gfx::drawCircle(float cx, float cy, float r, D2D1_COLOR_F color, float strokeWidth) {
    m_brush->SetColor(color);
    m_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), m_brush, strokeWidth);
}

void Gfx::drawText(const std::wstring& text, FontId fid, D2D1_POINT_2F pos, D2D1_COLOR_F color) {
    m_brush->SetColor(color);
    auto fmt = m_fonts[(int)fid];
    if (!fmt) return;
    
    m_rt->DrawTextW(text.c_str(), (UINT32)text.size(), fmt,
                    D2D1::RectF(pos.x, pos.y, pos.x + 2000, pos.y + 200),
                    m_brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
}

TextSize Gfx::measureText(const std::wstring& text, FontId fid) {
    auto fmt = m_fonts[(int)fid];
    if (!fmt) return {0, 0};
    
    IDWriteTextLayout* layout = nullptr;
    m_dwFactory->CreateTextLayout(text.c_str(), (UINT32)text.size(), fmt, 2000, 200, &layout);
    
    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);
    layout->Release();
    
    return {metrics.width, metrics.height};
}
