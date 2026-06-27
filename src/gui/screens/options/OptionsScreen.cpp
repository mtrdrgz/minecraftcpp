#include "OptionsScreen.h"
#include "../../../client/Minecraft.h"
#include "../../../client/Options.h"
#include <string>
#include <cmath>

namespace mc::gui::screens {

using components::AbstractWidget;
using components::WidgetButton;
using components::Slider;
using components::CycleButton;

namespace {
    void drawCentered(render::GuiGraphics& g, render::Font* font, const std::string& s, int cx, int y) {
        if (font) font->drawString(g, s, (float)(cx - font->width(s) / 2), (float)y, { 1, 1, 1, 1 });
    }
    std::string pct100(double v) { return v <= 0.0 ? std::string("OFF") : std::to_string((int)(v * 100 + 0.5)) + "%"; }
}

// ── OptionsSubScreen (base) ──────────────────────────────────────────────────
OptionsSubScreen::OptionsSubScreen(const std::string& title, std::function<void()> back)
    : Screen(title), m_back(std::move(back)) {}

void OptionsSubScreen::addSlider(const std::string& label, double val, double mn, double mx,
                                 std::function<std::string(double)> fmt, std::function<void(double)> onCh, bool big) {
    m_pending.push_back({ std::make_unique<Slider>(0, 0, 150, 20, label, val, mn, mx, std::move(fmt), std::move(onCh)), big });
}
void OptionsSubScreen::addCycle(const std::string& label, std::vector<std::string> choices, int idx,
                                std::function<void(int)> onCh, bool big) {
    m_pending.push_back({ std::make_unique<CycleButton>(0, 0, 150, 20, label, std::move(choices), idx, std::move(onCh)), big });
}
void OptionsSubScreen::addToggle(const std::string& label, bool val, std::function<void(bool)> onCh, bool big) {
    addCycle(label, { "OFF", "ON" }, val ? 1 : 0, [onCh = std::move(onCh)](int i) { onCh(i == 1); }, big);
}

void OptionsSubScreen::addHeader(const std::string& text) {
    // 1:1 with OptionsList.addHeader: adds a HeaderEntry row with the given text.
    // The header row has extra top padding (lineHeight * 2 = 18px) if it's not
    // the first entry, plus the text height (9px) + 4px bottom.
    Row row;
    row.isHeader = true;
    row.headerText = text;
    row.height = m_rows.empty() ? 13 : (18 + 9 + 4);  // paddingTop + lineHeight + 4
    m_rows.push_back(std::move(row));
}

void OptionsSubScreen::init(Minecraft* mcp, int w, int h) {
    Screen::init(mcp, w, h);
    m_pending.clear();
    m_rows.clear();
    addOptions();

    // Convert pending widgets into rows (1:1 with OptionsList.Entry logic).
    // big = 1 widget per row (310px). small = 2 widgets per row (150px each).
    for (size_t i = 0; i < m_pending.size(); ++i) {
        auto& p = m_pending[i];
        p.w->setTextures(m_btnN, m_btnH);
        p.w->setSliderTextures(m_sliderTrack, m_sliderHandle, m_sliderHandleHl);
        if (p.big) {
            Row row;
            row.left = std::move(p.w);
            row.height = 25;  // DEFAULT_ITEM_HEIGHT
            m_rows.push_back(std::move(row));
        } else {
            // Pair small widgets 2-per-row (1:1 with OptionsList.addSmall).
            Row row;
            row.left = std::move(p.w);
            if (i + 1 < m_pending.size() && !m_pending[i + 1].big) {
                auto& p2 = m_pending[++i];
                p2.w->setTextures(m_btnN, m_btnH);
                p2.w->setSliderTextures(m_sliderTrack, m_sliderHandle, m_sliderHandleHl);
                row.right = std::move(p2.w);
            }
            row.height = 25;
            m_rows.push_back(std::move(row));
        }
    }
    m_pending.clear();

    // Compute list geometry (1:1 with HeaderAndFooterLayout).
    // headerHeight=33, footerHeight=33, contentY = headerHeight + 30 = 63.
    // listX = 0 (full width). listW = w. listH = h - headerHeight - footerHeight.
    m_headerHeight = 33;
    m_footerHeight = 33;
    m_listX = 0;
    m_listY = m_headerHeight + 30;  // preferredContentY = headerHeight + CONTENT_MARGIN_TOP(30)
    // Clamp: if content doesn't fit, move it up so it's centered.
    const int ch = contentHeight();
    const int maxListY = h - m_footerHeight - ch;
    if (m_listY > maxListY) m_listY = std::max(m_headerHeight, maxListY);
    m_listW = w;
    m_listH = h - m_footerHeight - m_listY;

    // Done button (1:1 with OptionsSubScreen.addFooter: 200px centered in footer).
    m_doneBtn = std::make_unique<WidgetButton>(w / 2 - 100, h - 27, 200, 20, "Done", [this]() { if (m_back) m_back(); });
    m_doneBtn->setTextures(m_btnN, m_btnH);
}

int OptionsSubScreen::contentHeight() const {
    int total = 0;
    for (const auto& r : m_rows) total += r.height;
    return total + 4;  // +4 like AbstractSelectionList.getMaxScroll
}

int OptionsSubScreen::maxScroll() const {
    return std::max(0, contentHeight() - m_listH);
}

void OptionsSubScreen::clampScroll() {
    if (m_scrollAmount < 0.0) m_scrollAmount = 0.0;
    if (m_scrollAmount > maxScroll()) m_scrollAmount = maxScroll();
}

void OptionsSubScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    (void)pt;
    // Background dim (in-world screens are semi-transparent).
    g.fill(0, 0, m_width, m_height, { 0.0f, 0.0f, 0.0f, 0.6f });

    // Title (1:1 with addTitleHeader: centered text at top of header).
    drawCentered(g, m_minecraft->font(), m_title, m_width / 2, 15);

    // List background (1:1 with extractListBackground: menu_list_background tiled).
    // We tile a 32×32 pattern across the list area. If no texture, use dark fill.
    if (m_listBg) {
        // Tile the 32x32 background texture across the list area.
        for (int y = m_listY; y < m_listY + m_listH; y += 32) {
            for (int x = m_listX; x < m_listX + m_listW; x += 32) {
                const int dw = std::min(32, m_listX + m_listW - x);
                const int dh = std::min(32, m_listY + m_listH - y);
                g.blitSized(m_listBg, x, y, dw, dh, 0, 0, dw, dh, 32, 32);
            }
        }
    } else {
        g.fill(m_listX, m_listY, m_listX + m_listW, m_listY + m_listH, { 0.05f, 0.05f, 0.05f, 0.8f });
    }

    // Render list items with scissor (clip to list area).
    // 1:1 with AbstractSelectionList.enableScissor + extractListItems.
    // We implement scissor by skipping items outside the visible range.
    const int scrollY = (int)m_scrollAmount;
    const int x0 = m_width / 2 - 155;
    const int x1 = m_width / 2 + 5;
    int y = m_listY - scrollY;
    render::Font* font = m_minecraft->font();

    for (auto& row : m_rows) {
        const int rowY = y;
        const int rowH = row.height;
        y += rowH;

        // Skip rows entirely above the visible area.
        if (rowY + rowH < m_listY) continue;
        // Skip rows entirely below the visible area.
        if (rowY > m_listY + m_listH) continue;

        if (row.isHeader) {
            // Header text (1:1 with HeaderEntry: centered at x0, with paddingTop).
            if (font) {
                const int tw = font->width(row.headerText);
                font->drawString(g, row.headerText, (float)(x0 + 4), (float)(rowY + (row.height > 13 ? 18 : 0)), { 1, 1, 1, 1 });
            }
        } else if (row.right) {
            // Small row: two widgets side by side.
            row.left->setPos(x0, rowY);
            row.left->setSize(150, 20);
            row.right->setPos(x1, rowY);
            row.right->setSize(150, 20);
            // Clip rendering to visible area (manual scissor).
            if (rowY >= m_listY && rowY + 20 <= m_listY + m_listH) {
                row.left->render(g, *font, mx, my);
                row.right->render(g, *font, mx, my);
            }
        } else {
            // Big row: single widget 310px.
            row.left->setPos(m_width / 2 - 155, rowY);
            row.left->setSize(310, 20);
            if (rowY >= m_listY && rowY + 20 <= m_listY + m_listH) {
                row.left->render(g, *font, mx, my);
            }
        }
    }

    // Header separator (1:1 with extractListSeparators: 2px tall, 32px wide tiled).
    // Drawn at listY - 2.
    if (m_headerSep) {
        for (int x = m_listX; x < m_listX + m_listW; x += 32) {
            const int dw = std::min(32, m_listX + m_listW - x);
            g.blitSized(m_headerSep, x, m_listY - 2, dw, 2, 0, 0, dw, 2, 32, 2);
        }
    } else {
        g.fill(m_listX, m_listY - 2, m_listX + m_listW, m_listY, { 0.3f, 0.3f, 0.3f, 1.0f });
    }
    // Footer separator at listY + listH.
    const int footerY = m_listY + m_listH;
    if (m_footerSep) {
        for (int x = m_listX; x < m_listX + m_listW; x += 32) {
            const int dw = std::min(32, m_listX + m_listW - x);
            g.blitSized(m_footerSep, x, footerY, dw, 2, 0, 0, dw, 2, 32, 2);
        }
    } else {
        g.fill(m_listX, footerY, m_listX + m_listW, footerY + 2, { 0.3f, 0.3f, 0.3f, 1.0f });
    }

    // Scrollbar (1:1 with extractScrollbar).
    // Only drawn when the list is scrollable (content > visible height).
    if (maxScroll() > 0) {
        const int barW = 12;
        const int barX = m_listX + m_listW - barW - 2;  // right edge - 2
        // Background
        if (m_scrollerBg) {
            g.blitSized(m_scrollerBg, barX, m_listY, barW, m_listH, 0, 0, barW, m_listH, 12, m_listH);
        } else {
            g.fill(barX, m_listY, barX + barW, m_listY + m_listH, { 0.1f, 0.1f, 0.1f, 0.8f });
        }
        // Scroller handle — height proportional to visible/content ratio.
        const int scrollerH = std::max(12, m_listH * m_listH / contentHeight());
        const int scrollerY = m_listY + (int)(m_scrollAmount * (m_listH - scrollerH) / maxScroll());
        if (m_scroller) {
            g.blitSized(m_scroller, barX, scrollerY, barW, scrollerH, 0, 0, barW, scrollerH, 12, scrollerH);
        } else {
            g.fill(barX, scrollerY, barX + barW, scrollerY + scrollerH, { 0.6f, 0.6f, 0.6f, 1.0f });
        }
    }

    // Done button (footer).
    if (m_doneBtn) m_doneBtn->render(g, *font, mx, my);
}

void OptionsSubScreen::mouseClicked(double x, double y, int button) {
    if (button != 0) return;
    render::Font* font = m_minecraft->font();
    // Done button
    if (m_doneBtn && m_doneBtn->mouseClicked(x, y, button)) return;

    // Scrollbar click
    if (maxScroll() > 0) {
        const int barW = 12;
        const int barX = m_listX + m_listW - barW - 2;
        if ((int)x >= barX && (int)x < barX + barW && (int)y >= m_listY && (int)y < m_listY + m_listH) {
            m_scrollingBar = true;
            return;
        }
    }

    // List items (only those visible)
    const int scrollY = (int)m_scrollAmount;
    const int x0 = m_width / 2 - 155;
    const int x1 = m_width / 2 + 5;
    int yi = m_listY - scrollY;
    for (auto& row : m_rows) {
        const int rowY = yi;
        const int rowH = row.height;
        yi += rowH;
        if (rowY + rowH < m_listY) continue;
        if (rowY > m_listY + m_listH) continue;
        if (rowY >= m_listY && rowY + 20 <= m_listY + m_listH) {
            if (row.isHeader) continue;
            if (row.right) {
                row.left->setPos(x0, rowY); row.left->setSize(150, 20);
                row.right->setPos(x1, rowY); row.right->setSize(150, 20);
                if (row.left->mouseClicked(x, y, button)) return;
                if (row.right->mouseClicked(x, y, button)) return;
            } else {
                row.left->setPos(m_width / 2 - 155, rowY); row.left->setSize(310, 20);
                if (row.left->mouseClicked(x, y, button)) return;
            }
        }
    }
}

void OptionsSubScreen::mouseReleased(double x, double y, int button) {
    m_scrollingBar = false;
    // Forward to visible sliders so they stop dragging.
    const int scrollY = (int)m_scrollAmount;
    int yi = m_listY - scrollY;
    for (auto& row : m_rows) {
        const int rowY = yi;
        yi += row.height;
        if (rowY + row.height < m_listY) continue;
        if (rowY > m_listY + m_listH) continue;
        if (!row.isHeader && rowY >= m_listY && rowY + 20 <= m_listY + m_listH) {
            if (row.left) row.left->mouseReleased(x, y, button);
            if (row.right) row.right->mouseReleased(x, y, button);
        }
    }
}

void OptionsSubScreen::mouseDragged(double x, double y, int button, double dx, double dy) {
    if (m_scrollingBar && button == 0 && maxScroll() > 0) {
        const int scrollerH = std::max(12, m_listH * m_listH / contentHeight());
        const int trackH = m_listH - scrollerH;
        if (trackH > 0) {
            m_scrollAmount += (double)dy * maxScroll() / trackH;
            clampScroll();
        }
        return;
    }
    // Forward to visible sliders.
    const int scrollY = (int)m_scrollAmount;
    int yi = m_listY - scrollY;
    for (auto& row : m_rows) {
        const int rowY = yi;
        yi += row.height;
        if (rowY + row.height < m_listY) continue;
        if (rowY > m_listY + m_listH) continue;
        if (!row.isHeader && rowY >= m_listY && rowY + 20 <= m_listY + m_listH) {
            if (row.left) row.left->mouseDragged(x, y, button, dx, dy);
            if (row.right) row.right->mouseDragged(x, y, button, dx, dy);
        }
    }
}

void OptionsSubScreen::mouseScrolled(double x, double y, double dx, double dy) {
    (void)x; (void)y; (void)dx;
    // 1:1 with AbstractSelectionList.mouseScrolled: scrollAmount += scrollY * scrollRate.
    // Vanilla scrollRate is 16 (default ScrollbarSettings).
    m_scrollAmount -= dy * 16.0;
    clampScroll();
}

// ── OptionsScreen (title + sub-header FOV/Online + category grid + Done) ────
// 1:1 port of OptionsScreen.java. Layout from HeaderAndFooterLayout(screen, 61, 33):
//   - Header (height 61): vertical LinearLayout containing:
//       • StringWidget "Options" (title, centered)
//       • horizontal LinearLayout (spacing 8) containing:
//           - FOV slider (this.options.fov().createButton) — 200px wide
//           - "Online..." button — 150px wide
//   - Contents: GridLayout (2 columns, 150px each, 4px padding)
//       Skin Customization | Music & Sounds
//       Video Settings     | Controls
//       Language           | Chat Settings
//       Resource Packs     | Accessibility Settings
//       Telemetry Data     | Credits & Attribution
//   - Footer (height 33): Done button (200px, centered)
//
// The FOV slider is NOT centered in the content — it's in the sub-header row
// to the LEFT of the Online button. This matches vanilla exactly.
OptionsScreen::OptionsScreen() : Screen("Options") {}

void OptionsScreen::init(Minecraft* mcp, int w, int h) {
    Screen::init(mcp, w, h);
    m_widgets.clear();

    auto add = [&](std::unique_ptr<AbstractWidget> wgt) {
        wgt->setTextures(m_btnN, m_btnH);
        wgt->setSliderTextures(m_sliderTrack, m_sliderHandle, m_sliderHandleHl);
        m_widgets.push_back(std::move(wgt));
    };
    Minecraft* mc = m_minecraft;
    auto openSub = [mc, this](std::unique_ptr<gui::Screen> s) {
        if (auto* sub = dynamic_cast<OptionsSubScreen*>(s.get())) {
            sub->setButtonTextures(m_btnN, m_btnH);
            sub->setSliderTextures(m_sliderTrack, m_sliderHandle, m_sliderHandleHl);
            sub->setListTextures(mc->headerSepTex(), mc->footerSepTex(),
                                 mc->scrollerTex(), mc->scrollerBgTex(), mc->listBgTex());
        }
        mc->setScreen(std::move(s));
    };
    auto back = [mc]() { mc->openOptionsScreen(); };

    // ── Header (height 61): title + sub-header row ──
    // Title at y=8 (HeaderAndFooterLayout header starts at y=0, title is centered
    // vertically in the 61px header). Sub-header row at y=8+8+8=24 (title 9px +
    // spacing 8px). Vanilla positions the sub-header via LinearLayout, which
    // centers it horizontally. FOV is 200px, Online is 150px, spacing 8px →
    // total 358px → starts at w/2 - 179.
    GameOptions& o = mc->options();
    const int subY = 24;  // below title
    const int subW = 200 + 8 + 150;  // FOV + spacing + Online
    const int subX = w / 2 - subW / 2;
    {
        // FOV slider: 200px wide, left side of sub-header.
        auto fov = std::make_unique<Slider>(subX, subY, 200, 20, "FOV", o.fov, 30.0, 110.0,
            [](double v) { int f = (int)(v + 0.5); return f >= 110 ? std::string("Quake Pro") : std::to_string(f); },
            [&o](double v) { o.fov = v; });
        add(std::move(fov));
    }
    // Online button: 150px wide, right side of sub-header.
    add(std::make_unique<WidgetButton>(subX + 200 + 8, subY, 150, 20, "Online...", []() {}));

    // ── Contents: 2-column GridLayout ──
    // Vanilla uses GridLayout with 2 columns, 4px horizontal padding, 4px bottom
    // padding, centered. Each cell is 150px wide. The grid starts below the
    // header (y = headerHeight + 30 = 61 + 30 = 91, but clamped to fit).
    // We use the same x0/x1 as OptionsSubScreen: w/2-155 and w/2+5 (310px total
    // with 4px gap = 150+4+150+... vanilla's GridLayout padding is 4px).
    struct Cat { const char* label; int kind; };
    static const Cat CATS[] = {
        { "Skin Customization...", 6 }, { "Music & Sounds...", 1 }, { "Video Settings...", 2 },
        { "Controls...", 3 }, { "Language...", 4 }, { "Chat Settings...", 7 },
        { "Resource Packs...", 8 }, { "Accessibility Settings", 5 }, { "Telemetry Data...", 9 },
        { "Credits & Attribution...", 10 },
    };
    const int x0 = w / 2 - 155, x1 = w / 2 + 5;
    // Content starts at headerHeight(61) + CONTENT_MARGIN_TOP(30) = 91.
    // GridLayout paddingBottom=4 → row spacing = 20+4 = 24.
    const int top = 91;
    const int rowSpacing = 24;
    for (int i = 0; i < (int)(sizeof(CATS) / sizeof(CATS[0])); ++i) {
        const int col = i % 2, row = i / 2;
        const std::string label = CATS[i].label;
        std::string title = label;
        if (auto p = title.find("..."); p != std::string::npos) title = title.substr(0, p);
        const int kind = CATS[i].kind;
        add(std::make_unique<WidgetButton>(col == 0 ? x0 : x1, top + row * rowSpacing, 150, 20, label,
            [openSub, back, title, kind]() {
                std::unique_ptr<gui::Screen> s;
                switch (kind) {
                    case 1:  s = std::make_unique<SoundOptionsScreen>(title, back); break;
                    case 2:  s = std::make_unique<VideoSettingsScreen>(title, back); break;
                    case 3:  s = std::make_unique<ControlsScreen>(title, back); break;
                    case 4:  s = std::make_unique<LanguageSelectScreen>(title, back); break;
                    case 5:  s = std::make_unique<AccessibilityOptionsScreen>(title, back); break;
                    case 6:  s = std::make_unique<SkinCustomizationScreen>(title, back); break;
                    case 7:  s = std::make_unique<ChatOptionsScreen>(title, back); break;
                    case 8:  s = std::make_unique<ResourcePacksScreen>(title, back); break;
                    case 9:  s = std::make_unique<TelemetryInfoScreen>(title, back); break;
                    case 10: s = std::make_unique<CreditsAndAttributionScreen>(title, back); break;
                    default: s = std::make_unique<OptionsSubScreen>(title, back); break;
                }
                openSub(std::move(s));
            }));
    }

    // ── Footer (height 33): Done button centered ──
    // Footer is at screen.height - footerHeight(33). Done is 200px centered.
    add(std::make_unique<WidgetButton>(w / 2 - 100, h - 27, 200, 20, "Done", [this]() { if (m_back) m_back(); }));
}

void OptionsScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    g.fill(0, 0, m_width, m_height, { 0.0f, 0.0f, 0.0f, 0.6f });
    drawCentered(g, m_minecraft->font(), "Options", m_width / 2, 15);
    render::Font* font = m_minecraft->font();
    for (auto& widget : m_widgets) widget->render(g, *font, mx, my);
}

void OptionsScreen::mouseClicked(double x, double y, int button) {
    std::function<void()> action;
    for (auto& widget : m_widgets) {
        action = widget->clickAction(x, y, button);
        if (action) break;
        if (widget->mouseClicked(x, y, button)) return;
    }
    if (action) action();
}

void OptionsScreen::mouseReleased(double x, double y, int button) {
    for (auto& widget : m_widgets) if (widget->mouseReleased(x, y, button)) break;
}

void OptionsScreen::mouseDragged(double x, double y, int button, double dx, double dy) {
    for (auto& widget : m_widgets) if (widget->mouseDragged(x, y, button, dx, dy)) break;
}

// ── Concrete category screens (real options from the Java) ───────────────────
void SoundOptionsScreen::addOptions() {
    GameOptions& o = mc()->options();
    // SoundOptionsScreen.java: master (big), then every other SoundSource (small),
    // then subtitles + directional audio.
    static const char* NAMES[] = { "Master Volume", "Music", "Jukebox/Note Blocks", "Weather",
        "Blocks", "Hostile Creatures", "Friendly Creatures", "Players", "Ambient/Environment", "Voice/Speech" };
    addSlider(NAMES[0], o.volume[0], 0.0, 1.0, pct100, [&o](double v) { o.volume[0] = (float)v; }, true);
    for (int i = 1; i < (int)audio::SoundSource::COUNT; ++i)
        addSlider(NAMES[i], o.volume[i], 0.0, 1.0, pct100, [&o, i](double v) { o.volume[i] = (float)v; });
    addToggle("Show Subtitles", o.showSubtitles, [&o](bool b) { o.showSubtitles = b; });
    addToggle("Directional Audio", o.directionalAudio, [&o](bool b) { o.directionalAudio = b; });
}

void VideoSettingsScreen::addOptions() {
    // 1:1 port of VideoSettingsScreen.addOptions(). Three sections, each with a
    // header (OptionsList.addHeader) + options. The real vanilla screen has
    // ~28 controls across 3 sections — we port the full list.
    GameOptions& o = mc()->options();

    // ── Display section ──
    addHeader("Display");
    // Vanilla: addBig(fullscreenOption) — a custom OptionInstance for fullscreen
    // resolution. We skip it (no monitor enumeration) and just add the rest.
    // displayOptions: framerateLimit, enableVsync, inactivityFpsLimit, guiScale,
    // fullscreen, exclusiveFullscreen, gamma.
    addSlider("Max Framerate", 60.0, 10.0, 260.0,
              [](double v) { int f = (int)(v + 0.5); return f >= 260 ? std::string("Unlimited") : std::to_string(f) + " fps"; },
              [](double) {});
    addToggle("VSync", o.vsync, [&o](bool b) { o.vsync = b; });
    addSlider("Inactive FPS Limit", 60.0, 1.0, 120.0,
              [](double v) { int f = (int)(v + 0.5); return f >= 120 ? std::string("Unlimited") : std::to_string(f) + " fps"; },
              [](double) {});
    addCycle("GUI Scale", { "Auto", "1", "2", "3", "4" }, o.guiScale, [&o](int i) { o.guiScale = i; });
    addToggle("Fullscreen", o.fullscreen, [&o](bool b) { o.fullscreen = b; });
    addToggle("Exclusive Fullscreen", false, [](bool) {});
    addSlider("Brightness", o.gamma, 0.0, 1.0,
              [](double v) { if (v <= 0) return std::string("Moody"); if (v >= 1) return std::string("Bright"); return std::to_string((int)(v * 100)) + "%"; },
              [&o](double v) { o.gamma = v; });

    // ── Quality section ──
    addHeader("Quality");
    // Vanilla: addBig(graphicsPreset) — a custom option. We add the graphics
    // cycle as a big button.
    addCycle("Graphics", { "Fast", "Fancy", "Fabulous!" }, o.graphics, [&o](int i) { o.graphics = i; }, true);
    // qualityOptions: biomeBlendRadius, renderDistance, prioritizeChunkUpdates,
    // simulationDistance, ambientOcclusion, cloudStatus, particles, mipmapLevels,
    // entityShadows, entityDistanceScaling, menuBackgroundBlurriness, cloudRange,
    // cutoutLeaves, improvedTransparency, textureFiltering, maxAnisotropyBit,
    // weatherRadius.
    addSlider("Biome Blend", 2.0, 0.0, 7.0,
              [](double v) { int b = (int)(v + 0.5); return b == 0 ? std::string("None") : std::to_string(b * 2 + 1) + " biomes"; },
              [](double) {});
    addSlider("Render Distance", (double)o.renderDistance, 2.0, 32.0,
              [](double v) { return std::to_string((int)(v + 0.5)) + " chunks"; },
              [&o](double v) { o.renderDistance = (int)(v + 0.5); });
    addCycle("Prioritize Chunk Updates", { "None", "Player Affected", "Nearby" }, 0, [](int) {});
    addSlider("Simulation Distance", 8.0, 5.0, 32.0,
              [](double v) { return std::to_string((int)(v + 0.5)) + " chunks"; },
              [](double) {});
    addCycle("Smooth Lighting", { "Off", "Low", "High" }, 2, [](int) {});
    addCycle("Clouds", { "Off", "Fast", "Fancy" }, 2, [](int) {});
    addCycle("Particles", { "All", "Decreased", "Minimal" }, 0, [](int) {});
    addSlider("Mipmap Levels", 4.0, 0.0, 4.0,
              [](double v) { int m = (int)(v + 0.5); return m == 0 ? std::string("Off") : std::to_string(m); },
              [](double) {});
    addToggle("Entity Shadows", false, [](bool) {});
    addSlider("Entity Distance", 1.0, 0.5, 1.5,
              [](double v) { return std::to_string((int)(v * 100)) + "%"; },
              [](double) {});
    addSlider("Menu Background Blurriness", 0.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Cloud Range", 256.0, 32.0, 1024.0,
              [](double v) { return std::to_string((int)(v + 0.5)) + " blocks"; },
              [](double) {});
    addToggle("Cutout Leaves", false, [](bool) {});
    addToggle("Improved Transparency", false, [](bool) {});
    addCycle("Texture Filtering", { "Nearest", "Linear" }, 0, [](int) {});
    addSlider("Anisotropic Filtering", 0.0, 0.0, 4.0,
              [](double v) { int a = (int)(v + 0.5); return a == 0 ? std::string("Off") : std::to_string(1 << a) + "x"; },
              [](double) {});
    addSlider("Weather Radius", 0.0, 0.0, 32.0,
              [](double v) { int w = (int)(v + 0.5); return w == 0 ? std::string("Default") : std::to_string(w) + " blocks"; },
              [](double) {});

    // ── Preferences section ──
    addHeader("Preferences");
    // preferenceOptions: showAutosaveIndicator, vignette, attackIndicator,
    // chunkSectionFadeInTime.
    addToggle("Show Autosave Indicator", true, [](bool) {});
    addToggle("Vignette", true, [](bool) {});
    addToggle("Attack Indicator", false, [](bool) {});
    addSlider("Chunk Section Fade In Time", 1.0, 0.0, 5.0,
              [](double v) { return std::to_string(v).substr(0, 4) + "s"; },
              [](double) {});
}

void ControlsScreen::addOptions() {
    GameOptions& o = mc()->options();
    // Mouse settings. (The rebindable key-binding list is its own sub-system, TODO.)
    addSlider("Sensitivity", o.sensitivity, 0.0, 1.0,
              [](double v) { int p = (int)(v * 200 + 0.5); return p >= 200 ? std::string("HYPERSPEED!!!") : std::to_string(p) + "%"; },
              [&o](double v) { o.sensitivity = v; }, true);
    addToggle("Invert Mouse", o.invertYMouse, [&o](bool b) { o.invertYMouse = b; });
    addToggle("Auto-Jump", o.autoJump, [&o](bool b) { o.autoJump = b; });
    addToggle("Discrete Scrolling", o.discreteMouseScroll, [&o](bool b) { o.discreteMouseScroll = b; });
    addToggle("Touchscreen Mode", o.touchscreen, [&o](bool b) { o.touchscreen = b; });
}

// ── AccessibilityOptionsScreen ──────────────────────────────────────────────
// Port of AccessibilityOptionsScreen.options(): the accessibility-related
// toggles/sliders. Real vanilla has 24 OptionInstance entries; we expose the
// ones that exist in our GameOptions. The rest are stubbed as no-op toggles
// so the screen layout matches vanilla (each row is a real control).
void AccessibilityOptionsScreen::addOptions() {
    GameOptions& o = mc()->options();
    // Real toggles from GameOptions.
    addToggle("Show Subtitles", o.showSubtitles, [&o](bool b) { o.showSubtitles = b; });
    addToggle("View Bobbing", o.viewBobbing, [&o](bool b) { o.viewBobbing = b; });
    addToggle("Dark Mojang Studios Background", false, [](bool) {});
    addToggle("Hide Lightning Flashes", false, [](bool) {});
    addToggle("Hide Splash Texts", false, [](bool) {});
    addToggle("Narrator Hotkey", false, [](bool) {});
    addToggle("High Contrast", false, [](bool) {});
    addToggle("Rotate With Minecart", false, [](bool) {});
    addToggle("High Contrast Block Outline", false, [](bool) {});
    // Sliders for effect scales (no-op onChange — real values not wired yet).
    addSlider("Text Background Opacity", 0.5, 0.0, 1.0, pct100, [](double) {}, true);
    addSlider("Chat Opacity", 0.5, 0.0, 1.0, pct100, [](double) {});
    addSlider("Chat Line Spacing", 0.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    addSlider("Chat Delay", 0.0, 0.0, 6.0, [](double v) { return std::to_string((int)(v * 1000)) + " ms"; }, [](double) {});
    addSlider("Notification Display Time", 1.0, 0.5, 12.0, [](double v) { return std::to_string(v).substr(0, 4) + "s"; }, [](double) {});
    addSlider("Menu Background Blurriness", 0.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Screen Effect Scale", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("FOV Effect Scale", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Darkness Effect Scale", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Damage Tilt Strength", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Glint Speed", 0.5, 0.0, 1.0, pct100, [](double) {});
    addSlider("Glint Strength", 0.75, 0.0, 1.0, pct100, [](double) {});
    addSlider("Panorama Speed", 1.0, 0.1, 4.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    // Narrator cycle (Off/System/Chat/All).
    addCycle("Narrator", { "Off", "System", "Chat", "All" }, 0, [](int) {}, true);
}

// ── LanguageSelectScreen ────────────────────────────────────────────────────
// Simplified port: vanilla has a scrolling ObjectSelectionList of all installed
// languages + a search box. We show the common languages as cycle buttons +
// a "Done" button. The language actually applied doesn't change (i18n not
// ported), but the screen layout + navigation is 1:1 with vanilla's
// OptionsSubScreen base.
void LanguageSelectScreen::addOptions() {
    addCycle("Language", {
        "English (US)",
        "English (UK)",
        "Español (España)",
        "Español (México)",
        "Français",
        "Deutsch",
        "Italiano",
        "Português (Brasil)",
        "Русский",
        "日本語",
        "中文(简体)",
        "中文(繁體)",
        "한국어",
    }, 0, [](int) {}, true);
    addToggle("Force Unicode Font", false, [](bool) {});
    addToggle("Japanese Glyph Variants", false, [](bool) {});
}

// ── SkinCustomizationScreen ─────────────────────────────────────────────────
// Port of SkinCustomizationScreen.addOptions(): toggles for each
// PlayerModelPart (cape, jacket, left_sleeve, right_sleeve, left_pants_leg,
// right_pants_leg, hat) + main hand cycle. All default to ON (vanilla default).
void SkinCustomizationScreen::addOptions() {
    addToggle("Cape", true, [](bool) {});
    addToggle("Jacket", true, [](bool) {});
    addToggle("Left Sleeve", true, [](bool) {});
    addToggle("Right Sleeve", true, [](bool) {});
    addToggle("Left Pants Leg", true, [](bool) {});
    addToggle("Right Pants Leg", true, [](bool) {});
    addToggle("Hat", true, [](bool) {});
    addCycle("Main Hand", { "Left", "Right" }, 1, [](int) {}, true);
}

// ── ChatOptionsScreen ───────────────────────────────────────────────────────
// Port of ChatOptionsScreen.options(): 18 chat-related controls.
void ChatOptionsScreen::addOptions() {
    addCycle("Chat Visibility", { "Shown", "Commands Only", "Hidden" }, 0, [](int) {}, true);
    addToggle("Chat Colors", false, [](bool) {});
    addToggle("Web Links", true, [](bool) {});
    addToggle("Prompt on Links", true, [](bool) {});
    addSlider("Chat Opacity", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Text Background Opacity", 0.5, 0.0, 1.0, pct100, [](double) {});
    addSlider("Chat Scale", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    addSlider("Chat Line Spacing", 0.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    addSlider("Chat Delay", 0.0, 0.0, 6.0, [](double v) { return std::to_string((int)(v * 1000)) + " ms"; }, [](double) {});
    addSlider("Chat Width", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 320)) + "px"; }, [](double) {});
    addSlider("Chat Height (Focused)", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 180)) + "px"; }, [](double) {});
    addSlider("Chat Height (Unfocused)", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 90)) + "px"; }, [](double) {});
    addCycle("Narrator", { "Off", "System", "Chat", "All" }, 0, [](int) {}, true);
    addToggle("Auto Suggestions", true, [](bool) {});
    addToggle("Hide Matched Names", false, [](bool) {});
    addToggle("Reduced Debug Info", false, [](bool) {});
    addToggle("Only Show Secure Chat", false, [](bool) {});
    addToggle("Save Chat Drafts", true, [](bool) {});
}

// ── ResourcePacksScreen ─────────────────────────────────────────────────────
// Simplified port. Vanilla has a two-pane list (available / selected) with
// drag-to-reorder. We show a placeholder label + Done — the pack list UI is
// a significant porting effort (ObjectSelectionList, drag-drop, pack stacking).
void ResourcePacksScreen::addOptions() {
    // No interactive controls — vanilla uses a custom list widget, not
    // sliders/cycles. The Done button (added by the base class) is the only
    // control. The title is shown by the base render().
}

// ── TelemetryInfoScreen ─────────────────────────────────────────────────────
// Port of TelemetryInfoScreen. Vanilla shows a telemetry toggle + info text +
// links. We show the toggle + a placeholder.
void TelemetryInfoScreen::addOptions() {
    addToggle("Send Telemetry Data", false, [](bool) {}, true);
}

// ── CreditsAndAttributionScreen ─────────────────────────────────────────────
// Port of CreditsAndAttributionScreen. Three buttons (Credits, Attribution,
// Licenses) + Done. In vanilla, Credits opens WinScreen (the end-game credits
// scroll); Attribution and Licenses open ConfirmLinkScreen (URLs). We make
// them no-op toggles for now (the actual screens are not ported).
void CreditsAndAttributionScreen::addOptions() {
    addToggle("View Credits", false, [](bool) {}, true);
    addToggle("View Attribution", false, [](bool) {});
    addToggle("View Licenses", false, [](bool) {});
}

} // namespace mc::gui::screens
