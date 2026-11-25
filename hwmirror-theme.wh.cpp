// ==WindhawkMod==
// @id              hwmirror-theme
// @name            Huawei Multi-Screen Collaboration Theme
// @description     Windows theme and hover auto-hide for the HwMirror title bar and navigation bar.
// @version         1.20
// @include         HwMirror.exe
// @architecture    x86-64
// @compilerOptions -ladvapi32 -lgdi32 -lgdiplus -luser32 -lcomctl32 -lmsimg32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
适用于使用 DuiLib 的 64 位华为电脑管家多屏协同窗口。
跟随 Windows「设置 → 个性化 → 颜色」中的应用模式。
顶部和底部可分别选择跟随系统、浅色、深色、自定义，默认跟随系统。
自定义模式支持分别填写背景和文字/图标颜色，格式为 #RRGGBB。
深色按钮使用模组绘制的浅色图形，保留原控件的操作行为。
主题检查间隔为 400 毫秒。
鼠标进入协同窗口时显示顶部和底部，离开后延迟淡出并裁剪为透明。
默认离开延迟 450 毫秒，淡出 500 毫秒，显示 120 毫秒。
可在模组设置中关闭自动隐藏或调整时间。
裁剪区域可透出桌面。鼠标移回中间的手机画面即可重新显示。
动画采用淡出与边缘裁剪组合，隐藏时保持手机画面的原布局。
顶部和底部的裁剪覆盖整条窗口宽度，外边框与圆角随可见轮廓收起。
普通窗口完全隐藏两条栏后，同步缩小窗口实际高度。
鼠标移回手机画面时恢复窗口高度，再淡入两条栏。
收起时同时清零两条栏的固定高度和坐标，避免隐藏控件继续占位。
按标题栏和底栏之间的可用区域计算视频尺寸，收起后保留内容尺寸与位置。
同步移动实际承载视频的 Button 子窗口，保留视频原有宽度和高度。
缩窗及恢复期间延后父窗口绘制，完成定位后同步刷新。
保留窗口可见状态与视频子窗口的画面，避免过渡时露出桌面。
尺寸切换期间用上一帧覆盖视频区域约 100 毫秒，等待位置提交后接回实时视频。
展开和收起状态均保护视频区域，避免展开后的延迟布局逐次压缩内容高度。
手动缩放时按客户区尺寸变化同步调整视频区域，不锁定旧窗口大小。
横屏保留华为原生视频尺寸和右侧导航栏，隐藏时仅收起标题栏的实际高度。
横竖屏切换时丢弃旧尺寸保护，避免竖屏底栏被恢复到横屏画面中。
断开时恢复布局并清空会话尺寸，重连完成后按当前画布重新校准视频窗口。
每次收起前保存完整展开尺寸，恢复时使用该尺寸，避免尺寸取整误差逐次累积。
关闭模组即可恢复原界面。

安装方式

1. Windhawk 右下角「新建 Mod」。
2. 用本文件全部内容替换模板代码。
3. 点击「编译 Mod」，确认模组已启用。
4. 如暂未生效，断开并重新连接多屏协同。

验证方式

在 Windows 颜色设置中切换应用的浅色、深色模式，观察已打开的窗口。
如果 Windows 选择「自定义」模式，以「默认应用模式」为准。

兼容性

针对本机 HwMirror.exe 的 ClientWindow.xml 控件名称和 DuiLib 导出制作。
已完成编译、导出符号检查、淡出合成及透明裁剪恢复的隔离测试。
实际协同窗口的鼠标交互和布局动画仍需验证。
加载失败时可在高级设置启用日志，检查缺失的 DuiLib 符号。
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- topMode: system
  $name: 顶部颜色模式
  $options:
  - system: 跟随系统
  - light: 浅色
  - dark: 深色
  - custom: 自定义
- topBackground: "#202020"
  $name: 顶部自定义背景色
  $description: "仅自定义模式生效，填写 #RRGGBB，例如 #202020。"
- topForeground: "#F1F1F1"
  $name: 顶部自定义文字和图标颜色
  $description: "仅自定义模式生效，填写 #RRGGBB，例如 #F1F1F1。"
- bottomMode: system
  $name: 底部颜色模式
  $options:
  - system: 跟随系统
  - light: 浅色
  - dark: 深色
  - custom: 自定义
- bottomBackground: "#202020"
  $name: 底部自定义背景色
  $description: "仅自定义模式生效，填写 #RRGGBB，例如 #202020。"
- bottomForeground: "#F1F1F1"
  $name: 底部自定义图标颜色
  $description: "仅自定义模式生效，填写 #RRGGBB，例如 #F1F1F1。"
- autoHide: true
  $name: 自动隐藏顶部和底部
- hideDelayMs: 450
  $name: 鼠标离开后的等待时间（毫秒）
- fadeOutMs: 500
  $name: 淡出时间（毫秒）
- fadeInMs: 120
  $name: 显示时间（毫秒）
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cwchar>
#include <string_view>

namespace {

using FindControl = void* (*)(const void*, PCWSTR);
using GetPosition = const RECT& (*)(const void*);
using GetButtonState = UINT (*)(void*);
using IsEnabled = bool (*)(const void*);
using GetPaintWindow = HWND (*)(const void*);
using SetVisible = void (*)(void*, bool);
using NeedUpdate = void (*)(void*);
using GetFixedHeight = int (*)(const void*);
using SetFixedHeight = void (*)(void*, int);
using SetPosition = void (*)(void*, RECT, bool);
using SetNativePosition = BOOL (WINAPI*)(HWND, HWND, int, int, int, int, UINT);
using MoveNativeWindow = BOOL (WINAPI*)(HWND, int, int, int, int, BOOL);
using PaintContainer = void (*)(void*, HDC, const RECT&);
using MessageHandler = bool (*)(void*, UINT, WPARAM, LPARAM, LRESULT&);
using PaintControl = void (*)(void*, HDC);
using DrawText = void (*)(HDC, void*, RECT&, PCWSTR, DWORD, int, UINT);
using DrawTextEx = void (*)(HDC, void*, RECT&, PCWSTR, DWORD, int, UINT, DWORD, int);
using DrawHtmlText = void (*)(HDC, void*, RECT&, PCWSTR, DWORD, int,
                              RECT*, void*, int&, UINT, int*);

FindControl g_findControl;
GetPosition g_getPosition;
GetButtonState g_getButtonState;
IsEnabled g_isEnabled;
IsEnabled g_isVisible;
GetPaintWindow g_getPaintWindow;
SetVisible g_setVisible;
NeedUpdate g_needUpdate;
GetFixedHeight g_getFixedHeight;
SetFixedHeight g_setFixedHeight;
SetPosition g_setPosition;
SetNativePosition g_setNativePosition;
MoveNativeWindow g_moveNativeWindow;
PaintContainer g_paintContainer;
MessageHandler g_messageHandler;
PaintControl g_paintBackground;
PaintControl g_paintButtonImage;
DrawText g_drawText;
DrawTextEx g_drawTextEx;
DrawHtmlText g_drawHtmlText;

constexpr DWORD kDarkBackground = 0xff202020;
constexpr DWORD kTextColor = 0xfff1f1f1;
constexpr wchar_t kThemeKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr UINT kHot = 0x08;
constexpr UINT kPressed = 0x10;

enum class Icon {
    Back, Menu, Minimize, Maximize, Restore, Close, Home, Tasks
};

enum class ThemeMode { System, Light, Dark, Custom };

struct BarTheme {
    ThemeMode mode = ThemeMode::System;
    DWORD background = kDarkBackground;
    DWORD foreground = kTextColor;
};

struct BarPalette {
    DWORD background = 0xffffffff;
    DWORD foreground = 0xff363636;
    bool customPaint = false;
};

BarPalette ResolvePalette(const BarTheme& theme, bool systemDark) {
    if (theme.mode == ThemeMode::Custom) {
        return {theme.background, theme.foreground, true};
    }
    if (theme.mode == ThemeMode::Dark ||
        (theme.mode == ThemeMode::System && systemDark)) {
        return {kDarkBackground, kTextColor, true};
    }
    return {};
}

ThemeMode ParseThemeMode(PCWSTR text) {
    if (!text) return ThemeMode::System;
    if (wcscmp(text, L"light") == 0) return ThemeMode::Light;
    if (wcscmp(text, L"dark") == 0) return ThemeMode::Dark;
    if (wcscmp(text, L"custom") == 0) return ThemeMode::Custom;
    return ThemeMode::System;
}

DWORD ParseColor(PCWSTR text, DWORD fallback) {
    if (!text) return fallback;
    std::wstring_view value(text);
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == value.npos) return fallback;
    const auto last = value.find_last_not_of(L" \t\r\n");
    value = value.substr(first, last - first + 1);
    if (value.front() == L'#') value.remove_prefix(1);
    if (value.size() != 6) return fallback;
    DWORD color = 0;
    for (wchar_t character : value) {
        unsigned int digit;
        if (character >= L'0' && character <= L'9') digit = character - L'0';
        else if (character >= L'a' && character <= L'f') digit = character - L'a' + 10;
        else if (character >= L'A' && character <= L'F') digit = character - L'A' + 10;
        else return fallback;
        color = (color << 4) | digit;
    }
    return 0xff000000 | color;
}

BarTheme ReadBarTheme(PCWSTR modeName, PCWSTR backgroundName, PCWSTR foregroundName) {
    const auto mode = WindhawkUtils::StringSetting::make(modeName);
    const auto background = WindhawkUtils::StringSetting::make(backgroundName);
    const auto foreground = WindhawkUtils::StringSetting::make(foregroundName);
    return {ParseThemeMode(mode.get()), ParseColor(background.get(), kDarkBackground),
            ParseColor(foreground.get(), kTextColor)};
}

DWORD MixColor(DWORD background, DWORD foreground, int percentage) {
    DWORD result = 0xff000000;
    for (int shift : {0, 8, 16}) {
        const int back = (background >> shift) & 0xff;
        const int front = (foreground >> shift) & 0xff;
        result |= ((back * (100 - percentage) + front * percentage) / 100) << shift;
    }
    return result;
}

constexpr struct {
    PCWSTR name;
    Icon icon;
} kButtons[] = {
    {L"appbackbtn", Icon::Back}, {L"setbtn", Icon::Menu},
    {L"minbtn", Icon::Minimize}, {L"maxbtn", Icon::Maximize},
    {L"lowerbtn", Icon::Restore}, {L"closebtn", Icon::Close},
    {L"backbtn", Icon::Back}, {L"homebtn", Icon::Home},
    {L"multitaskbtn", Icon::Tasks},
};

struct PaintContext {
    const void* manager;
    void* title;
    void* navigation;
    void* buttons[std::size(kButtons)];
    BarPalette topPalette;
    BarPalette bottomPalette;
    float opacity = 1;
};

struct WindowState {
    HWND window;
    void* manager;
    UINT_PTR timer = 0;
    HRGN originalRegion = nullptr;
    bool regionApplied = false;
    bool changingRegion = false;
    bool compact = false;
    bool changingGeometry = false;
    bool hasGeometrySnapshot = false;
    bool protectExpandedHeight = false;
    bool landscape = false;
    RECT expandedBounds{};
    RECT compactBounds{};
    void* compactTitle = nullptr;
    void* compactNavigation = nullptr;
    bool titleWasVisible = false;
    bool navigationWasVisible = false;
    int compactTop = 0;
    int compactBottom = 0;
    int titleFixedHeight = 0;
    int navigationFixedHeight = 0;
    RECT titleBounds{};
    RECT navigationBounds{};
    void* canvas = nullptr;
    RECT viewportBounds{};
    HWND videoWindow = nullptr;
    RECT videoBounds{};
    RECT videoClientBounds{};
    HWND transitionCover = nullptr;
    ULONGLONG coverUntil = 0;
    RECT lastTitleCut{};
    RECT lastNavigationCut{};
    RECT lastWindowBounds{};
    float visibility = 1;
    ULONGLONG lastHover = GetTickCount64();
    ULONGLONG lastTick = lastHover;
    bool menuOpen = false;
    bool moving = false;
    bool sessionNeedsRefresh = true;
    ULONGLONG readyAfter = GetTickCount64() + 800;
};

thread_local const PaintContext* g_paintContext;
std::atomic<bool> g_dark;
std::atomic<bool> g_layoutReported;
std::atomic<bool> g_unloading;
std::atomic<bool> g_autoHide{true};
std::atomic<int> g_hideDelay{450};
std::atomic<int> g_fadeOut{500};
std::atomic<int> g_fadeIn{120};
std::mutex g_colorsMutex;
BarTheme g_topTheme;
BarTheme g_bottomTheme;
std::mutex g_windowsMutex;
std::unordered_map<HWND, WindowState*> g_windows;
UINT g_cleanupMessage;
HANDLE g_stopEvent;
HANDLE g_themeThread;
ULONG_PTR g_gdiplus;
ATOM g_coverClass;
WCHAR g_coverClassName[96];

float AdvanceVisibility(float current, bool show, ULONGLONG elapsed,
                        int fadeIn, int fadeOut) {
    const float step = static_cast<float>(elapsed) / (show ? fadeIn : fadeOut);
    return std::clamp(current + (show ? step : -step), 0.0f, 1.0f);
}

float EaseVisibility(float visibility) {
    return visibility * visibility * (3 - 2 * visibility);
}

void LoadSettings() {
    const BarTheme top = ReadBarTheme(L"topMode", L"topBackground", L"topForeground");
    const BarTheme bottom =
        ReadBarTheme(L"bottomMode", L"bottomBackground", L"bottomForeground");
    {
        std::lock_guard lock(g_colorsMutex);
        g_topTheme = top;
        g_bottomTheme = bottom;
    }
    g_autoHide.store(Wh_GetIntSetting(L"autoHide") != 0);
    g_hideDelay.store(std::clamp(Wh_GetIntSetting(L"hideDelayMs"), 0, 10000));
    g_fadeOut.store(std::clamp(Wh_GetIntSetting(L"fadeOutMs"), 50, 5000));
    g_fadeIn.store(std::clamp(Wh_GetIntSetting(L"fadeInMs"), 50, 5000));
}

bool BelongsToWindow(HWND candidate, HWND window) {
    candidate = GetAncestor(candidate, GA_ROOT);
    while (candidate) {
        if (candidate == window) {
            return true;
        }
        candidate = GetWindow(candidate, GW_OWNER);
    }
    return false;
}

bool MouseOverWindow(HWND window) {
    POINT cursor;
    return GetCursorPos(&cursor) &&
           BelongsToWindow(WindowFromPoint(cursor), window);
}

LRESULT CALLBACK TransitionCoverProc(HWND window, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window, message, wparam, lparam);
}

void DismissTransitionCover(WindowState& state) {
    if (state.transitionCover) ShowWindow(state.transitionCover, SW_HIDE);
    state.coverUntil = 0;
}

void ShowTransitionCover(WindowState& state) {
    if (g_unloading.load() || state.moving || GetCapture() ||
        !g_coverClass || !state.videoWindow || !IsWindowVisible(state.videoWindow)) {
        return;
    }
    RECT bounds;
    if (!GetWindowRect(state.videoWindow, &bounds)) return;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) return;
    for (int y : {bounds.top + 2, (bounds.top + bounds.bottom) / 2, bounds.bottom - 3}) {
        if (!BelongsToWindow(WindowFromPoint(POINT{(bounds.left + bounds.right) / 2, y}),
                             state.window)) {
            return;
        }
    }

    HDC screen = GetDC(nullptr);
    if (!screen) return;
    HDC buffer = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    if (!buffer || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
        ReleaseDC(nullptr, screen);
        return;
    }
    HGDIOBJ previous = SelectObject(buffer, bitmap);
    bool captured = BitBlt(buffer, 0, 0, width, height, screen,
                           bounds.left, bounds.top, SRCCOPY | CAPTUREBLT) != 0;
    if (captured && !state.transitionCover) {
        state.transitionCover = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            g_coverClassName, L"", WS_POPUP, bounds.left, bounds.top, width, height,
            state.window, nullptr, GetModuleHandleW(nullptr), nullptr);
    }
    if (captured && state.transitionCover) {
        POINT position{bounds.left, bounds.top};
        POINT origin{};
        SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, 0};
        captured = UpdateLayeredWindow(state.transitionCover, screen, &position, &size,
            buffer, &origin, 0, &blend, ULW_ALPHA) != 0;
    } else {
        captured = false;
    }
    SelectObject(buffer, previous);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    ReleaseDC(nullptr, screen);
    if (captured) {
        HWND preceding = GetWindow(state.window, GW_HWNDPREV);
        if (preceding == state.transitionCover) {
            preceding = GetWindow(preceding, GW_HWNDPREV);
        }
        SetWindowPos(state.transitionCover, preceding, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
            SWP_SHOWWINDOW);
        DwmFlush();
        state.coverUntil = GetTickCount64() + 100;
    }
}

void RestoreWindowRegion(WindowState& state) {
    if (!state.regionApplied) {
        return;
    }
    HRGN restored = nullptr;
    if (state.originalRegion) {
        restored = CreateRectRgn(0, 0, 0, 0);
        if (!restored ||
            CombineRgn(restored, state.originalRegion, nullptr, RGN_COPY) == ERROR) {
            if (restored) DeleteObject(restored);
            return;
        }
    }
    state.changingRegion = true;
    const bool success =
        SetWindowRgn(state.window, restored, !state.changingGeometry) != 0;
    state.changingRegion = false;
    if (success) {
        // SetWindowRgn owns the region on success.
        state.regionApplied = false;
        if (state.originalRegion) DeleteObject(state.originalRegion);
        state.originalRegion = nullptr;
    } else if (restored) {
        DeleteObject(restored);
    }
}

bool IsLandscapeLayout(const WindowState& state) {
    void* side = g_findControl(state.manager, L"horizon_navigationbtn");
    return side && g_isVisible(side);
}

void SetCompactControls(WindowState& state, bool restore) {
    void* title = g_findControl(state.manager, L"titleBar");
    void* navigation = g_findControl(state.manager, L"navigationbtn");
    if (title && title == state.compactTitle) {
        g_setFixedHeight(title, restore ? state.titleFixedHeight : 0);
        g_setVisible(title, restore && state.titleWasVisible);
        RECT bounds = state.titleBounds;
        if (!restore) bounds.bottom = bounds.top;
        g_setPosition(title, bounds, false);
    }
    if (navigation && navigation == state.compactNavigation) {
        g_setFixedHeight(navigation, restore ? state.navigationFixedHeight : 0);
        const bool landscape = IsLandscapeLayout(state);
        if (!restore || landscape == state.landscape || landscape) {
            g_setVisible(navigation, restore && state.navigationWasVisible && !landscape);
        }
        // Returning from landscape must retain the application's newly shown
        // portrait navigation, rather than restoring its old hidden state.
        RECT bounds = state.navigationBounds;
        if (!restore) bounds.bottom = bounds.top;
        g_setPosition(navigation, bounds, false);
    }
    g_needUpdate(state.manager);
}

RECT PhoneViewport(const WindowState& state) {
    RECT bounds{};
    if (!GetClientRect(state.window, &bounds)) return bounds;
    void* title = g_findControl(state.manager, L"titleBar");
    if (title && g_isVisible(title)) {
        const RECT& bar = g_getPosition(title);
        bounds.top += std::max(0L, bar.bottom - bar.top);
    }
    void* navigation = g_findControl(state.manager, L"navigationbtn");
    if (navigation && g_isVisible(navigation)) {
        // Posted layout may still contain coordinates from the compact client.
        // Thickness is stable across hover transitions; the old origin is not.
        const RECT& bar = g_getPosition(navigation);
        bounds.bottom -= std::max(0L, bar.bottom - bar.top);
    }
    void* side = g_findControl(state.manager, L"horizon_navigationbtn");
    if (side && g_isVisible(side)) {
        const RECT& position = g_getPosition(side);
        if (position.left >= (bounds.left + bounds.right) / 2) {
            bounds.right -= std::max(0L, position.right - position.left);
        } else {
            bounds.left += std::max(0L, position.right - position.left);
        }
    }
    return bounds;
}

void FindVideoWindow(WindowState& state) {
    struct Search {
        HWND parent;
        int width;
        int minimumHeight;
        HWND match = nullptr;
        RECT bounds{};
        int count = 0;
    } search{state.window, state.viewportBounds.right - state.viewportBounds.left,
             (state.viewportBounds.bottom - state.viewportBounds.top) / 2};
    EnumChildWindows(state.window, [](HWND child, LPARAM parameter) -> BOOL {
        auto& search = *reinterpret_cast<Search*>(parameter);
        WCHAR className[32];
        if (GetParent(child) != search.parent ||
            !(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) ||
            !GetClassNameW(child, className, std::size(className)) ||
            _wcsicmp(className, L"Button") != 0) {
            return TRUE;
        }
        RECT bounds;
        if (!GetWindowRect(child, &bounds)) return TRUE;
        MapWindowPoints(nullptr, search.parent, reinterpret_cast<POINT*>(&bounds), 2);
        WCHAR caption[32]{};
        GetWindowTextW(child, caption, std::size(caption));
        const bool videoCaption = wcsncmp(caption, L"Loading", 7) == 0;
        if (videoCaption ||
            (bounds.right - bounds.left == search.width &&
             bounds.bottom - bounds.top > search.minimumHeight)) {
            search.match = child;
            search.bounds = bounds;
            ++search.count;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    state.videoWindow = search.count == 1 ? search.match : nullptr;
    state.videoBounds = search.bounds;
}

RECT VideoTarget(const WindowState& state) {
    RECT target = state.videoBounds;
    if (state.compact) {
        OffsetRect(&target,
            state.expandedBounds.left - state.compactBounds.left,
            state.expandedBounds.top - state.compactBounds.top);
    } else if (!state.changingGeometry && !IsRectEmpty(&state.videoClientBounds)) {
        RECT client;
        if (GetClientRect(state.window, &client)) {
            // Preserve the bar insets while allowing genuine parent resizes.
            target.right += client.right - state.videoClientBounds.right;
            target.bottom += client.bottom - state.videoClientBounds.bottom;
        }
    }
    return target;
}

bool ProtectedVideoTarget(HWND window, RECT& target, bool& changingGeometry) {
    if (g_unloading.load()) return false;
    std::lock_guard lock(g_windowsMutex);
    for (const auto& [parent, state] : g_windows) {
        if (state->videoWindow == window && !IsRectEmpty(&state->videoBounds) &&
            GetParent(window) == parent) {
            // Rotation belongs to the application's renderer. Never apply
            // portrait insets or a portrait snapshot to its landscape surface.
            if (IsLandscapeLayout(*state) != state->landscape) return false;
            if (state->landscape && !state->compact && !state->changingGeometry) {
                return false;
            }
            target = VideoTarget(*state);
            if (IsRectEmpty(&target)) return false;
            changingGeometry = state->changingGeometry;
            return true;
        }
    }
    return false;
}

BOOL WINAPI SetNativePositionHook(HWND window, HWND after, int x, int y,
                                  int width, int height, UINT flags) {
    RECT target;
    bool changingGeometry = false;
    if (ProtectedVideoTarget(window, target, changingGeometry)) {
        x = target.left;
        y = target.top;
        width = target.right - target.left;
        height = target.bottom - target.top;
        flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
        if (changingGeometry) flags |= SWP_NOREDRAW;
    }
    return g_setNativePosition(window, after, x, y, width, height, flags);
}

BOOL WINAPI MoveNativeWindowHook(HWND window, int x, int y,
                                 int width, int height, BOOL repaint) {
    RECT target;
    bool changingGeometry = false;
    if (ProtectedVideoTarget(window, target, changingGeometry)) {
        x = target.left;
        y = target.top;
        width = target.right - target.left;
        height = target.bottom - target.top;
        if (changingGeometry) repaint = FALSE;
    }
    return g_moveNativeWindow(window, x, y, width, height, repaint);
}

void AlignVideoWindow(WindowState& state) {
    if (IsLandscapeLayout(state) != state.landscape) return;
    if (state.landscape && !state.compact && !state.changingGeometry) return;
    const bool refreshing = !state.compact && !state.changingGeometry &&
        state.sessionNeedsRefresh && GetTickCount64() >= state.readyAfter;
    if (refreshing) {
        state.canvas = g_findControl(state.manager, L"canvas");
        if (!state.canvas) return;
        state.viewportBounds = PhoneViewport(state);
        if (IsRectEmpty(&state.viewportBounds)) return;
        FindVideoWindow(state);
        if (!state.videoWindow) return;
        state.videoBounds = state.viewportBounds;
        GetClientRect(state.window, &state.videoClientBounds);
    }
    if (!state.videoWindow || GetParent(state.videoWindow) != state.window) {
        return;
    }
    RECT current;
    if (!GetWindowRect(state.videoWindow, &current)) return;
    MapWindowPoints(nullptr, state.window, reinterpret_cast<POINT*>(&current), 2);
    const RECT target = VideoTarget(state);
    if (!EqualRect(&current, &target)) {
        const UINT flags = SWP_NOACTIVATE | SWP_NOZORDER |
            (state.changingGeometry ? SWP_NOREDRAW : 0);
        if (!g_setNativePosition(state.videoWindow, nullptr, target.left, target.top,
            target.right - target.left, target.bottom - target.top,
            flags)) return;
    }
    if (refreshing) {
        state.sessionNeedsRefresh = false;
        state.lastHover = GetTickCount64();
        state.readyAfter = state.lastHover + 150;
    }
}

bool BeginGeometryChange(WindowState& state, bool preserveFrame = true) {
    if (preserveFrame) ShowTransitionCover(state);
    state.changingGeometry = true;
    return IsWindowVisible(state.window);
}

void EndGeometryChange(WindowState& state, bool wasVisible) {
    state.changingGeometry = false;
    if (wasVisible) {
        RedrawWindow(state.window, nullptr, nullptr,
            RDW_INVALIDATE | RDW_FRAME | RDW_NOCHILDREN | RDW_UPDATENOW);
        if (state.coverUntil) state.coverUntil = GetTickCount64() + 100;
    }
}

bool SyncOrientation(WindowState& state);

bool RestoreCompactWindow(WindowState& state, bool preserveFrame = true) {
    if (state.changingGeometry) return false;
    if (SyncOrientation(state)) return true;
    if (!state.compact) {
        return true;
    }
    if (!state.hasGeometrySnapshot) {
        Wh_Log(L"Cannot restore compact window without an expanded geometry snapshot");
        return false;
    }
    RECT current;
    if (!GetWindowRect(state.window, &current)) {
        return false;
    }
    RECT target = state.expandedBounds;
    OffsetRect(&target, current.left - state.compactBounds.left,
                current.top - state.compactBounds.top);
    const bool wasVisible = BeginGeometryChange(state, preserveFrame);
    state.compact = false;
    SetCompactControls(state, true);
    const bool restored = g_setNativePosition(state.window, nullptr, target.left,
        target.top, target.right - target.left, target.bottom - target.top,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS) != 0;
    if (!restored) {
        state.compact = true;
        SetCompactControls(state, false);
        // Preserve the intended expanded size even if a failed operation moved the window.
        state.expandedBounds = target;
        GetWindowRect(state.window, &state.compactBounds);
        Wh_Log(L"Could not restore expanded HwMirror window");
    } else {
        state.hasGeometrySnapshot = false;
        state.expandedBounds = target;
        state.protectExpandedHeight = !state.landscape;
    }
    g_needUpdate(state.manager);
    AlignVideoWindow(state);
    EndGeometryChange(state, wasVisible);
    return restored;
}

void RestoreMinimizedLayout(WindowState& state) {
    if (!state.compact || !state.hasGeometrySnapshot || state.changingGeometry) return;
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(state.window, &placement)) return;
    // ShowWindow(SW_MINIMIZE) can bypass SC_MINIMIZE. At WM_SIZE it is too
    // late to resize the HWND: update its saved normal rectangle instead.
    RECT& normal = placement.rcNormalPosition;
    OffsetRect(&normal, state.expandedBounds.left - state.compactBounds.left,
               state.expandedBounds.top - state.compactBounds.top);
    normal.right = normal.left + state.expandedBounds.right - state.expandedBounds.left;
    normal.bottom = normal.top + state.expandedBounds.bottom - state.expandedBounds.top;
    state.changingGeometry = true;
    if (SetWindowPlacement(state.window, &placement)) {
        state.compact = false;
        state.hasGeometrySnapshot = false;
        state.protectExpandedHeight = !state.landscape;
        SetCompactControls(state, true);
        RestoreWindowRegion(state);
    }
    state.changingGeometry = false;
}

void ResetSessionLayout(WindowState& state) {
    DismissTransitionCover(state);
    state.canvas = nullptr;
    state.viewportBounds = RECT{};
    state.videoWindow = nullptr;
    state.videoBounds = RECT{};
    state.videoClientBounds = RECT{};
    state.compactTitle = nullptr;
    state.compactNavigation = nullptr;
    state.compactTop = 0;
    state.compactBottom = 0;
    state.hasGeometrySnapshot = false;
    state.protectExpandedHeight = false;
    state.landscape = IsLandscapeLayout(state);
    state.expandedBounds = RECT{};
    state.compactBounds = RECT{};
    state.titleFixedHeight = 0;
    state.navigationFixedHeight = 0;
    state.titleBounds = RECT{};
    state.navigationBounds = RECT{};
    state.visibility = 1;
    state.menuOpen = false;
    state.moving = false;
    state.lastHover = GetTickCount64();
    state.lastTick = state.lastHover;
    state.sessionNeedsRefresh = true;
    state.readyAfter = state.lastHover + 800;
}

bool SyncOrientation(WindowState& state) {
    const bool landscape = IsLandscapeLayout(state);
    if (state.changingGeometry || landscape == state.landscape) return false;
    DismissTransitionCover(state);
    // Do not restore the portrait outer rectangle: the caller may already be
    // committing the application's landscape size in WM_WINDOWPOSCHANGING.
    state.changingGeometry = true;
    if (state.compact) {
        state.compact = false;
        SetCompactControls(state, true);
    }
    if (landscape) {
        void* navigation = g_findControl(state.manager, L"navigationbtn");
        if (navigation) g_setVisible(navigation, false);
    }
    RestoreWindowRegion(state);
    ResetSessionLayout(state);
    g_needUpdate(state.manager);
    state.changingGeometry = false;
    return true;
}

bool CompactWindow(WindowState& state) {
    if (state.changingGeometry) return false;
    SyncOrientation(state);
    if (state.compact) {
        return true;
    }
    if (state.sessionNeedsRefresh) return false;
    if (IsZoomed(state.window) || IsIconic(state.window)) {
        return false;
    }
    RECT current;
    if (!GetWindowRect(state.window, &current)) {
        return false;
    }
    state.compactTitle = g_findControl(state.manager, L"titleBar");
    state.compactNavigation = g_findControl(state.manager, L"navigationbtn");
    state.titleWasVisible = state.compactTitle && g_isVisible(state.compactTitle);
    state.navigationWasVisible =
        state.compactNavigation && g_isVisible(state.compactNavigation);
    const auto height = [](void* control, bool visible) {
        if (!visible) return 0;
        const RECT& bounds = g_getPosition(control);
        return std::max(0, static_cast<int>(bounds.bottom - bounds.top));
    };
    state.compactTop = height(state.compactTitle, state.titleWasVisible);
    state.compactBottom = height(state.compactNavigation, state.navigationWasVisible);
    state.canvas = g_findControl(state.manager, L"canvas");
    if (!state.canvas) {
        return false;
    }
    state.viewportBounds = PhoneViewport(state);
    if (IsRectEmpty(&state.viewportBounds)) {
        return false;
    }
    FindVideoWindow(state);
    if (state.landscape) {
        // Landscape may contain intentional letterboxing. Retain the exact
        // renderer rectangle instead of expanding it to the whole viewport.
        if (!state.videoWindow || IsRectEmpty(&state.videoBounds)) return false;
    } else {
        state.videoBounds = state.viewportBounds;
    }
    GetClientRect(state.window, &state.videoClientBounds);
    if (state.compactTitle) {
        state.titleFixedHeight = g_getFixedHeight(state.compactTitle);
        state.titleBounds = g_getPosition(state.compactTitle);
    }
    if (state.compactNavigation) {
        state.navigationFixedHeight = g_getFixedHeight(state.compactNavigation);
        state.navigationBounds = g_getPosition(state.compactNavigation);
    }
    const int compactHeight = current.bottom - current.top -
                              state.compactTop - state.compactBottom;
    if (compactHeight <= 0 || state.compactTop + state.compactBottom <= 0) {
        return false;
    }
    state.expandedBounds = current;
    state.compactBounds = current;
    state.compactBounds.top += state.compactTop;
    state.compactBounds.bottom -= state.compactBottom;
    state.hasGeometrySnapshot = true;
    const bool wasVisible = BeginGeometryChange(state);
    RestoreWindowRegion(state);
    if (state.regionApplied) {
        EndGeometryChange(state, wasVisible);
        return false;
    }
    SetCompactControls(state, false);
    state.compact = true;
    const bool resized = g_setNativePosition(state.window, nullptr, current.left,
        current.top + state.compactTop, current.right - current.left,
        compactHeight,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS) != 0;
    if (!resized) {
        state.compact = false;
        state.hasGeometrySnapshot = false;
        SetCompactControls(state, true);
        Wh_Log(L"Could not compact HwMirror window");
    } else {
        // Windows or the application may adjust the requested rectangle.
        GetWindowRect(state.window, &state.compactBounds);
    }
    g_needUpdate(state.manager);
    AlignVideoWindow(state);
    EndGeometryChange(state, wasVisible);
    return resized;
}

void ConstrainCompactResize(WindowState& state, WINDOWPOS& position) {
    if (SyncOrientation(state)) return;
    if (state.landscape && !state.compact) return;
    if (state.changingGeometry || (position.flags & SWP_NOSIZE)) {
        return;
    }
    // The minimized icon rectangle is not a new phone/window size.
    if (IsIconic(state.window)) return;
    RECT current;
    if (!GetWindowRect(state.window, &current)) return;
    if (!state.compact) {
        // During restore GetWindowRect can still report the minimized icon.
        const int expectedWidth = state.protectExpandedHeight
            ? state.expandedBounds.right - state.expandedBounds.left
            : current.right - current.left;
        if (state.moving || position.cx != expectedWidth ||
            IsZoomed(state.window)) {
            state.protectExpandedHeight = false;
        } else if (state.protectExpandedHeight) {
            // Late aspect-ratio/layout updates from a hover transition must
            // not become the smaller baseline for the next transition.
            position.cy = state.expandedBounds.bottom - state.expandedBounds.top;
        }
        return;
    }
    if (position.cx != current.right - current.left) {
        RestoreCompactWindow(state, false);
        state.protectExpandedHeight = false;
        DismissTransitionCover(state);
        state.visibility = 1;
        state.lastHover = GetTickCount64();
        return;
    }
    // Ignore the application's automatic height reset while controls are hidden.
    if (position.cy != current.bottom - current.top) {
        position.cy = current.bottom - current.top;
        if (!(position.flags & SWP_NOMOVE)) position.y = current.top;
    }
}

RECT BarCut(WindowState& state, PCWSTR name, float opacity, bool top,
            POINT clientOrigin, const RECT& windowBounds) {
    RECT cut{};
    void* control = g_findControl(state.manager, name);
    if (!control || !g_isVisible(control)) {
        return cut;
    }
    cut = g_getPosition(control);
    const int hiddenHeight =
        static_cast<int>((cut.bottom - cut.top) * (1 - opacity) + 0.5f);
    if (hiddenHeight <= 0) {
        return RECT{};
    }
    if (top) {
        cut.bottom = cut.top + hiddenHeight;
    } else {
        cut.top = cut.bottom - hiddenHeight;
    }
    OffsetRect(&cut, clientOrigin.x, clientOrigin.y);
    // Include the non-client perimeter, not just the bar's inner controls.
    cut.left = windowBounds.left;
    cut.right = windowBounds.right;
    if (top) {
        cut.top = windowBounds.top;
    } else {
        cut.bottom = windowBounds.bottom;
    }
    return cut;
}

void UpdateTransparentRegion(WindowState& state, float opacity) {
    if (opacity >= 1) {
        RestoreWindowRegion(state);
        return;
    }
    RECT screenBounds;
    POINT origin{};
    if (!GetWindowRect(state.window, &screenBounds) ||
        !ClientToScreen(state.window, &origin)) {
        return;
    }
    origin.x -= screenBounds.left;
    origin.y -= screenBounds.top;
    const RECT bounds{0, 0, screenBounds.right - screenBounds.left,
                      screenBounds.bottom - screenBounds.top};
    const RECT titleCut = BarCut(state, L"titleBar", opacity, true, origin, bounds);
    const RECT navigationCut =
        BarCut(state, L"navigationbtn", opacity, false, origin, bounds);
    if (IsRectEmpty(&titleCut) && IsRectEmpty(&navigationCut)) {
        RestoreWindowRegion(state);
        return;
    }
    if (state.regionApplied && EqualRect(&state.lastTitleCut, &titleCut) &&
        EqualRect(&state.lastNavigationCut, &navigationCut) &&
        EqualRect(&state.lastWindowBounds, &bounds)) {
        return;
    }
    if (!state.regionApplied) {
        if (state.originalRegion) DeleteObject(state.originalRegion);
        state.originalRegion = CreateRectRgn(0, 0, 0, 0);
        if (!state.originalRegion) {
            return;
        }
        if (GetWindowRgn(state.window, state.originalRegion) == ERROR) {
            DeleteObject(state.originalRegion);
            state.originalRegion = nullptr;
        }
    }

    HRGN region = CreateRectRgnIndirect(&bounds);
    HRGN cut = CreateRectRgn(0, 0, 0, 0);
    bool valid = region && cut;
    if (valid && state.originalRegion) {
        valid = CombineRgn(region, state.originalRegion, nullptr, RGN_COPY) != ERROR;
    }
    for (const RECT& rect : {titleCut, navigationCut}) {
        if (valid && !IsRectEmpty(&rect)) {
            valid = SetRectRgn(cut, rect.left, rect.top, rect.right, rect.bottom) &&
                    CombineRgn(region, region, cut, RGN_DIFF) != ERROR;
        }
    }
    RECT outline = bounds;
    if (!IsRectEmpty(&titleCut)) {
        outline.top = std::clamp(titleCut.bottom, bounds.top, bounds.bottom);
    }
    if (!IsRectEmpty(&navigationCut)) {
        outline.bottom = std::clamp(navigationCut.top, outline.top, bounds.bottom);
    }
    static const auto getWindowDpi = reinterpret_cast<UINT (WINAPI*)(HWND)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    const int dpi = getWindowDpi ? getWindowDpi(state.window) : 96;
    const int corner = std::max(4, MulDiv(8, dpi, 96));
    HRGN rounded = CreateRoundRectRgn(outline.left, outline.top,
                                      outline.right + 1, outline.bottom + 1,
                                      corner, corner);
    if (valid) {
        valid = rounded && CombineRgn(region, region, rounded, RGN_AND) != ERROR;
    }
    if (rounded) DeleteObject(rounded);
    if (cut) DeleteObject(cut);
    if (valid) {
        state.changingRegion = true;
        valid = SetWindowRgn(state.window, region, TRUE) != 0;
        state.changingRegion = false;
    }
    if (valid) {
        state.regionApplied = true;
        state.lastTitleCut = titleCut;
        state.lastNavigationCut = navigationCut;
        state.lastWindowBounds = bounds;
    } else if (region) {
        DeleteObject(region);
    }
}

void TickWindow(WindowState& state) {
    if (state.changingGeometry) return;
    SyncOrientation(state);
    const ULONGLONG now = GetTickCount64();
    if (state.coverUntil && now >= state.coverUntil) {
        DismissTransitionCover(state);
    }
    const ULONGLONG elapsed = now - state.lastTick;
    state.lastTick = now;
    const HWND capture = GetCapture();
    const bool hover = MouseOverWindow(state.window) ||
                       (capture && BelongsToWindow(capture, state.window)) ||
                       state.menuOpen || state.moving;
    if (hover) {
        state.lastHover = now;
    }
    const bool enabled = g_autoHide.load();
    const bool show = !enabled || hover ||
        state.sessionNeedsRefresh || now < state.readyAfter ||
        now - state.lastHover < static_cast<ULONGLONG>(g_hideDelay.load());
    const float previous = state.visibility;
    state.visibility = enabled
        ? AdvanceVisibility(previous, show, elapsed, g_fadeIn.load(), g_fadeOut.load())
        : 1;
    const float opacity = EaseVisibility(state.visibility);
    if (opacity > 0 && state.compact) {
        if (!RestoreCompactWindow(state)) {
            return;
        }
    }
    if (opacity > 0 || !CompactWindow(state)) {
        UpdateTransparentRegion(state, opacity);
    }
    if (state.landscape) {
        // Native landscape layout can letterbox the stream. Replacing that
        // rectangle with client-minus-bars crops the decoded picture.
        state.sessionNeedsRefresh = false;
    } else {
        AlignVideoWindow(state);
    }
    if (previous != state.visibility) {
        InvalidateRect(state.window, nullptr, FALSE);
    }
}

void ForgetWindow(WindowState* state) {
    {
        std::lock_guard lock(g_windowsMutex);
        g_windows.erase(state->window);
    }
    if (state->originalRegion) DeleteObject(state->originalRegion);
    if (state->transitionCover) DestroyWindow(state->transitionCover);
    delete state;
}

LRESULT CALLBACK WindowSubclass(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam, DWORD_PTR refData) {
    auto* state = reinterpret_cast<WindowState*>(refData);
    if (message == g_cleanupMessage) {
        KillTimer(window, state->timer);
        DismissTransitionCover(*state);
        RestoreCompactWindow(*state, false);
        RestoreWindowRegion(*state);
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(window, WindowSubclass);
        ForgetWindow(state);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    if (message == WM_NCDESTROY) {
        KillTimer(window, state->timer);
        ForgetWindow(state);
        return DefSubclassProc(window, message, wparam, lparam);
    }
    if (message == WM_SIZE && wparam == SIZE_MINIMIZED && IsIconic(window)) {
        RestoreMinimizedLayout(*state);
    }
    if (message == WM_SHOWWINDOW && !state->changingGeometry && !IsIconic(window)) {
        if (RestoreCompactWindow(*state, false)) {
            // Visibility changes invalidate control/video handles, but a posted
            // compact layout must not replace the restored outer height.
            const bool protectHeight = state->protectExpandedHeight;
            const RECT expanded = state->expandedBounds;
            RestoreWindowRegion(*state);
            ResetSessionLayout(*state);
            state->protectExpandedHeight = protectHeight && !state->landscape;
            state->expandedBounds = expanded;
        }
    }
    if (message == WM_SIZE && wparam == SIZE_RESTORED &&
        !state->compact && !state->changingGeometry) {
        state->sessionNeedsRefresh = true;
        state->readyAfter = GetTickCount64() + 200;
    }
    if (message == WM_CLOSE || message == WM_DPICHANGED ||
        message == WM_ENTERSIZEMOVE) {
        DismissTransitionCover(*state);
        state->protectExpandedHeight = false;
    }
    if (!state->changingRegion && !state->changingGeometry &&
        (message == WM_DPICHANGED ||
         (message == WM_WINDOWPOSCHANGING &&
          !(reinterpret_cast<const WINDOWPOS*>(lparam)->flags & SWP_NOSIZE)))) {
        RestoreWindowRegion(*state);
    }
    if (!state->changingGeometry &&
        (message == WM_CLOSE || message == WM_DPICHANGED ||
         message == WM_ENTERSIZEMOVE ||
         (message == WM_SYSCOMMAND &&
          ((wparam & 0xfff0) == SC_MINIMIZE || (wparam & 0xfff0) == SC_MAXIMIZE ||
           (wparam & 0xfff0) == SC_RESTORE)) ||
         (message == WM_SIZE && wparam == SIZE_MAXIMIZED))) {
        RestoreCompactWindow(*state, false);
        // Taskbar minimize/restore must retain the expanded baseline. Maximize,
        // DPI and user resizing intentionally establish a different size.
        if (!(message == WM_SYSCOMMAND &&
              ((wparam & 0xfff0) == SC_MINIMIZE || (wparam & 0xfff0) == SC_RESTORE))) {
            state->protectExpandedHeight = false;
        }
    }
    if (message == WM_WINDOWPOSCHANGING) {
        ConstrainCompactResize(*state, *reinterpret_cast<WINDOWPOS*>(lparam));
    }
    if (message == WM_ENTERMENULOOP || message == WM_EXITMENULOOP) {
        state->menuOpen = message == WM_ENTERMENULOOP;
    }
    if (message == WM_ENTERSIZEMOVE || message == WM_EXITSIZEMOVE) {
        state->moving = message == WM_ENTERSIZEMOVE;
    }
    if (message == WM_TIMER && wparam == state->timer) {
        if (!g_unloading.load() && !IsIconic(window) && IsWindowVisible(window)) {
            TickWindow(*state);
        }
        return 0;
    }
    const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_NCHITTEST && result == HTCAPTION && state->compact) {
        return HTCLIENT;
    }
    if (message == WM_NCHITTEST && result == HTCAPTION && state->visibility < 1) {
        void* title = g_findControl(state->manager, L"titleBar");
        if (title) {
            POINT point{static_cast<short>(LOWORD(lparam)),
                        static_cast<short>(HIWORD(lparam))};
            ScreenToClient(window, &point);
            if (point.y >= g_getPosition(title).bottom) {
                return HTCLIENT;
            }
        }
    }
    return result;
}

WindowState* EnsureWindowState(void* manager) {
    if (g_unloading.load()) {
        return nullptr;
    }
    const HWND window = g_getPaintWindow(manager);
    if (!window) {
        return nullptr;
    }
    WindowState* existing = nullptr;
    {
        std::lock_guard lock(g_windowsMutex);
        const auto found = g_windows.find(window);
        if (found != g_windows.end()) {
            existing = found->second;
        }
    }
    if (existing) {
        if (existing->manager != manager) {
            existing->manager = manager;
            if (RestoreCompactWindow(*existing, false)) {
                RestoreWindowRegion(*existing);
                ResetSessionLayout(*existing);
            }
        }
        return existing;
    }
    auto* state = new WindowState{};
    state->window = window;
    state->manager = manager;
    if (!WindhawkUtils::SetWindowSubclassFromAnyThread(
            window, WindowSubclass, reinterpret_cast<DWORD_PTR>(state))) {
        delete state;
        return nullptr;
    }
    state->timer = SetTimer(window, reinterpret_cast<UINT_PTR>(state), 16, nullptr);
    if (!state->timer) {
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(window, WindowSubclass);
        delete state;
        return nullptr;
    }
    std::lock_guard lock(g_windowsMutex);
    g_windows.emplace(window, state);
    return state;
}

bool ReadDarkMode() {
    DWORD light = 1;
    DWORD bytes = sizeof(light);
    return RegGetValueW(HKEY_CURRENT_USER, kThemeKey, L"AppsUseLightTheme",
                        RRF_RT_REG_DWORD, nullptr, &light, &bytes) == ERROR_SUCCESS &&
           light == 0;
}

void RepaintWindows() {
    EnumWindows([](HWND window, LPARAM) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (pid == GetCurrentProcessId() && IsWindowVisible(window)) {
            InvalidateRect(window, nullptr, FALSE);
        }
        return TRUE;
    }, 0);
}

DWORD WINAPI WatchTheme(void*) {
    while (WaitForSingleObject(g_stopEvent, 400) == WAIT_TIMEOUT) {
        const bool dark = ReadDarkMode();
        if (g_dark.exchange(dark) != dark) {
            RepaintWindows();
        }
    }
    return 0;
}

bool MessageHandlerHook(void* manager, UINT message, WPARAM wparam,
                        LPARAM lparam, LRESULT& result) {
    if (message != WM_PAINT && message != WM_PRINTCLIENT && message != WM_PRINT) {
        return g_messageHandler(manager, message, wparam, lparam, result);
    }
    if (message == WM_PAINT) {
        const HWND window = g_getPaintWindow(manager);
        bool deferPaint = false;
        {
            std::lock_guard lock(g_windowsMutex);
            const auto found = g_windows.find(window);
            deferPaint = found != g_windows.end() &&
                found->second->manager == manager && found->second->changingGeometry;
        }
        if (deferPaint) {
            ValidateRect(window, nullptr);
            result = 0;
            return true;
        }
    }

    PaintContext context{};
    context.manager = manager;
    {
        std::lock_guard lock(g_colorsMutex);
        const bool dark = g_dark.load();
        context.topPalette = ResolvePalette(g_topTheme, dark);
        context.bottomPalette = ResolvePalette(g_bottomTheme, dark);
    }
    context.title = g_findControl(manager, L"titleBar");
    context.navigation = g_findControl(manager, L"navigationbtn");
    const bool mirrorLayout = context.title && context.navigation &&
                              g_findControl(manager, L"canvas");
    if (mirrorLayout) {
        if (!g_layoutReported.exchange(true)) {
            Wh_Log(L"Matched HwMirror titleBar, navigationbtn and canvas");
        }
        for (size_t i = 0; i < std::size(kButtons); ++i) {
            context.buttons[i] = g_findControl(manager, kButtons[i].name);
        }
        if (auto* state = EnsureWindowState(manager)) {
            context.opacity = EaseVisibility(state->visibility);
        }
    }

    const auto* previous = g_paintContext;
    g_paintContext = mirrorLayout ? &context : nullptr;
    const bool handled = g_messageHandler(manager, message, wparam, lparam, result);
    g_paintContext = previous;
    return handled;
}

void PaintBackgroundHook(void* control, HDC dc) {
    const auto* context = g_paintContext;
    if (context &&
        (control == context->title || control == context->navigation)) {
        const auto& palette = control == context->title
            ? context->topPalette : context->bottomPalette;
        if (palette.customPaint) {
            const DWORD color = palette.background;
            const COLORREF previous = SetDCBrushColor(dc,
                RGB((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff));
            FillRect(dc, &g_getPosition(control), static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            SetDCBrushColor(dc, previous);
            return;
        }
    }
    g_paintBackground(control, dc);
}

void PaintContainerHook(void* control, HDC dc, const RECT& paint) {
    const auto* context = g_paintContext;
    if (!context || (control != context->title && control != context->navigation) ||
        context->opacity >= 1) {
        g_paintContainer(control, dc, paint);
        return;
    }
    if (context->opacity <= 0) {
        return;
    }

    const RECT& bounds = g_getPosition(control);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    RECT intersection;
    if (width <= 0 || height <= 0 || !IntersectRect(&intersection, &bounds, &paint)) {
        return;
    }
    HDC buffer = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
    if (!buffer || !bitmap) {
        if (buffer) DeleteDC(buffer);
        if (bitmap) DeleteObject(bitmap);
        g_paintContainer(control, dc, paint);
        return;
    }
    HGDIOBJ previousBitmap = SelectObject(buffer, bitmap);
    BitBlt(buffer, 0, 0, width, height, dc, bounds.left, bounds.top, SRCCOPY);
    SetWindowOrgEx(buffer, bounds.left, bounds.top, nullptr);
    IntersectClipRect(buffer, intersection.left, intersection.top,
                      intersection.right, intersection.bottom);
    g_paintContainer(control, buffer, paint);
    const BLENDFUNCTION blend{AC_SRC_OVER, 0,
        static_cast<BYTE>(context->opacity * 255 + 0.5f), 0};
    AlphaBlend(dc, bounds.left, bounds.top, width, height, buffer,
               bounds.left, bounds.top, width, height, blend);
    SelectObject(buffer, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
}

void DrawGlyph(Gdiplus::Graphics& graphics, Gdiplus::Pen& pen, Icon icon) {
    using Gdiplus::PointF;
    switch (icon) {
        case Icon::Back: {
            const PointF triangle[] = {{3, 8}, {12, 2}, {12, 14}, {3, 8}};
            graphics.DrawLines(&pen, triangle, std::size(triangle));
            break;
        }
        case Icon::Menu:
            for (int y : {4, 8, 12}) {
                graphics.DrawLine(&pen, 3, y, 13, y);
            }
            break;
        case Icon::Minimize:
            graphics.DrawLine(&pen, 3, 8, 13, 8);
            break;
        case Icon::Maximize:
            graphics.DrawRectangle(&pen, 3, 3, 10, 10);
            break;
        case Icon::Restore:
            graphics.DrawRectangle(&pen, 2, 5, 9, 9);
            graphics.DrawLine(&pen, 5, 2, 14, 2);
            graphics.DrawLine(&pen, 14, 2, 14, 11);
            break;
        case Icon::Close:
            graphics.DrawLine(&pen, 3, 3, 13, 13);
            graphics.DrawLine(&pen, 13, 3, 3, 13);
            break;
        case Icon::Home:
            graphics.DrawEllipse(&pen, 2, 2, 12, 12);
            break;
        case Icon::Tasks: {
            Gdiplus::GraphicsPath shape;
            shape.AddArc(2, 2, 4, 4, 180, 90);
            shape.AddArc(10, 2, 4, 4, 270, 90);
            shape.AddArc(10, 10, 4, 4, 0, 90);
            shape.AddArc(2, 10, 4, 4, 90, 90);
            shape.CloseFigure();
            graphics.DrawPath(&pen, &shape);
            break;
        }
    }
}

void PaintButtonImageHook(void* button, HDC dc) {
    const auto* context = g_paintContext;
    if (context) {
        const auto* match = std::find(std::begin(context->buttons),
                                      std::end(context->buttons), button);
        if (match != std::end(context->buttons)) {
            const size_t index = match - std::begin(context->buttons);
            const auto& palette = index >= 6 ? context->bottomPalette : context->topPalette;
            if (!palette.customPaint) {
                g_paintButtonImage(button, dc);
                return;
            }
            const RECT& bounds = g_getPosition(button);
            Gdiplus::Graphics graphics(dc);
            if (graphics.GetLastStatus() == Gdiplus::Ok) {
                const auto icon = kButtons[index].icon;
                const bool enabled = g_isEnabled(button);
                const UINT state = g_getButtonState(button);
                const int width = bounds.right - bounds.left;
                const int height = bounds.bottom - bounds.top;
                const bool navigation = index >= 6;
                const float size = height * (navigation ? 0.40f : 0.49f);

                graphics.SetPageUnit(Gdiplus::UnitPixel);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                if (enabled && (state & (kHot | kPressed))) {
                    const DWORD hover = icon == Icon::Close ? 0xffc42b1c
                        : MixColor(palette.background, palette.foreground,
                                   state & kPressed ? 20 : 12);
                    Gdiplus::SolidBrush brush{Gdiplus::Color(hover)};
                    const Gdiplus::Rect area(bounds.left, bounds.top, width, height);
                    graphics.FillRectangle(&brush, area);
                }

                graphics.TranslateTransform(bounds.left + (width - size) / 2,
                                            bounds.top + (height - size) / 2);
                graphics.ScaleTransform(size / 16, size / 16);
                const DWORD foreground = enabled ? palette.foreground
                    : MixColor(palette.background, palette.foreground, 40);
                const DWORD ink = enabled && icon == Icon::Close &&
                    (state & (kHot | kPressed)) ? kTextColor : foreground;
                Gdiplus::Pen pen(Gdiplus::Color(ink), 1.25f);
                DrawGlyph(graphics, pen, icon);
                return;
            }
        }
    }
    g_paintButtonImage(button, dc);
}

DWORD TitleTextColor(const void* manager, const RECT& rect, DWORD color) {
    const auto* context = g_paintContext;
    if (context && context->topPalette.customPaint && manager == context->manager) {
        const RECT& title = g_getPosition(context->title);
        if (rect.top >= title.top && rect.top < title.bottom &&
            rect.bottom <= title.bottom && rect.left >= title.left &&
            rect.left < title.right) {
            return context->topPalette.foreground;
        }
    }
    return color;
}

void DrawTextHook(HDC dc, void* manager, RECT& rect, PCWSTR text,
                  DWORD color, int font, UINT flags) {
    g_drawText(dc, manager, rect, text, TitleTextColor(manager, rect, color), font, flags);
}

void DrawTextExHook(HDC dc, void* manager, RECT& rect, PCWSTR text,
                    DWORD color, int font, UINT flags, DWORD extra, int spacing) {
    g_drawTextEx(dc, manager, rect, text, TitleTextColor(manager, rect, color),
                 font, flags, extra, spacing);
}

void DrawHtmlTextHook(HDC dc, void* manager, RECT& rect, PCWSTR text, DWORD color,
                      int font, RECT* links, void* linkTexts, int& linkCount,
                      UINT flags, int* extra) {
    g_drawHtmlText(dc, manager, rect, text, TitleTextColor(manager, rect, color),
                   font, links, linkTexts, linkCount, flags, extra);
}

template <typename Function>
bool Resolve(HMODULE module, const char* name, Function& function) {
    function = reinterpret_cast<Function>(GetProcAddress(module, name));
    if (!function) {
        Wh_Log(L"Unsupported DuiLib version, missing export: %S", name);
    }
    return function != nullptr;
}

template <typename Function>
bool Hook(HMODULE module, const char* name, Function callback, Function& original) {
    Function target;
    return Resolve(module, name, target) &&
           Wh_SetFunctionHook(reinterpret_cast<void*>(target),
                              reinterpret_cast<void*>(callback),
                              reinterpret_cast<void**>(&original));
}

void ReleaseResources() {
    if (g_themeThread) {
        SetEvent(g_stopEvent);
        WaitForSingleObject(g_themeThread, INFINITE);
        CloseHandle(g_themeThread);
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
    }
    if (g_gdiplus) {
        Gdiplus::GdiplusShutdown(g_gdiplus);
    }
    if (g_coverClass) {
        UnregisterClassW(g_coverClassName, GetModuleHandleW(nullptr));
    }
}

}  // namespace

BOOL Wh_ModInit() {
    HMODULE dui = GetModuleHandleW(L"DuiLib.dll");
    if (!dui) {
        Wh_Log(L"DuiLib.dll was not loaded");
        return FALSE;
    }

    if (!Resolve(dui, "?FindControl@CPaintManagerUI@DuiLib@@QEBAPEAVCControlUI@2@PEB_W@Z", g_findControl) ||
        !Resolve(dui, "?GetPos@CControlUI@DuiLib@@UEBAAEBUtagRECT@@XZ", g_getPosition) ||
        !Resolve(dui, "?GetButtonState@CButtonUI@DuiLib@@QEAAIXZ", g_getButtonState) ||
        !Resolve(dui, "?IsEnabled@CControlUI@DuiLib@@UEBA_NXZ", g_isEnabled) ||
        !Resolve(dui, "?IsVisible@CControlUI@DuiLib@@UEBA_NXZ", g_isVisible) ||
        !Resolve(dui, "?GetPaintWindow@CPaintManagerUI@DuiLib@@QEBAPEAUHWND__@@XZ", g_getPaintWindow) ||
        !Resolve(dui, "?SetVisible@CContainerUI@DuiLib@@UEAAX_N@Z", g_setVisible) ||
        !Resolve(dui, "?NeedUpdate@CPaintManagerUI@DuiLib@@QEAAXXZ", g_needUpdate) ||
        !Resolve(dui, "?GetFixedHeight@CControlUI@DuiLib@@UEBAHXZ", g_getFixedHeight) ||
        !Resolve(dui, "?SetFixedHeight@CControlUI@DuiLib@@UEAAXH@Z", g_setFixedHeight) ||
        !Resolve(dui, "?SetPos@CHorizontalLayoutUI@DuiLib@@UEAAXUtagRECT@@_N@Z", g_setPosition) ||
        !Hook(dui, "?DoPaint@CContainerUI@DuiLib@@UEAAXPEAUHDC__@@AEBUtagRECT@@@Z", PaintContainerHook, g_paintContainer) ||
        !Hook(dui, "?MessageHandler@CPaintManagerUI@DuiLib@@QEAA_NI_K_JAEA_J@Z", MessageHandlerHook, g_messageHandler) ||
        !Hook(dui, "?PaintBkColor@CControlUI@DuiLib@@UEAAXPEAUHDC__@@@Z", PaintBackgroundHook, g_paintBackground) ||
        !Hook(dui, "?PaintStatusImage@CButtonUI@DuiLib@@UEAAXPEAUHDC__@@@Z", PaintButtonImageHook, g_paintButtonImage) ||
        !Hook(dui, "?DrawTextW@CRenderEngine@DuiLib@@SAXPEAUHDC__@@PEAVCPaintManagerUI@2@AEAUtagRECT@@PEB_WKHI@Z", DrawTextHook, g_drawText) ||
        !Hook(dui, "?DrawTextW@CRenderEngine@DuiLib@@SAXPEAUHDC__@@PEAVCPaintManagerUI@2@AEAUtagRECT@@PEB_WKHIKH@Z", DrawTextExHook, g_drawTextEx) ||
        !Hook(dui, "?DrawHtmlText@CRenderEngine@DuiLib@@SAXPEAUHDC__@@PEAVCPaintManagerUI@2@AEAUtagRECT@@PEB_WKHPEAU5@PEAVCDuiString@2@AEAHIPEAH@Z", DrawHtmlTextHook, g_drawHtmlText)) {
        Wh_Log(L"Could not install HwMirror paint hooks");
        return FALSE;
    }

    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!Hook(user32, "SetWindowPos", SetNativePositionHook, g_setNativePosition) ||
        !Hook(user32, "MoveWindow", MoveNativeWindowHook, g_moveNativeWindow)) {
        return FALSE;
    }

    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplus, &input, nullptr) != Gdiplus::Ok) {
        return FALSE;
    }
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        ReleaseResources();
        return FALSE;
    }
    g_dark.store(ReadDarkMode());
    LoadSettings();
    std::swprintf(g_coverClassName, std::size(g_coverClassName),
                   L"Windhawk.HwMirrorTheme.Cover.%p",
                   reinterpret_cast<void*>(TransitionCoverProc));
    WNDCLASSW coverClass{};
    coverClass.lpfnWndProc = TransitionCoverProc;
    coverClass.hInstance = GetModuleHandleW(nullptr);
    coverClass.lpszClassName = g_coverClassName;
    g_coverClass = RegisterClassW(&coverClass);
    if (!g_coverClass) {
        ReleaseResources();
        return FALSE;
    }
    g_cleanupMessage = RegisterWindowMessageW(L"Windhawk.HwMirrorTheme.Cleanup.v1");
    if (!g_cleanupMessage) {
        ReleaseResources();
        return FALSE;
    }
    return TRUE;
}

void Wh_ModAfterInit() {
    g_themeThread = CreateThread(nullptr, 0, WatchTheme, nullptr, 0, nullptr);
    if (!g_themeThread) {
        Wh_Log(L"Could not start Windows theme watcher");
    }
    RepaintWindows();
}

void Wh_ModUninit() {
    ReleaseResources();
    RepaintWindows();
}

void Wh_ModBeforeUninit() {
    g_unloading.store(true);
    std::vector<HWND> windows;
    {
        std::lock_guard lock(g_windowsMutex);
        for (const auto& [window, state] : g_windows) {
            windows.push_back(window);
        }
    }
    for (HWND window : windows) {
        SendMessageW(window, g_cleanupMessage, 0, 0);
    }
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    RepaintWindows();
}
