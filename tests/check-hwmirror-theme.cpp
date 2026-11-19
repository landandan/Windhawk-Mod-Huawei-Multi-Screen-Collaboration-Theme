#include "../hwmirror-theme.wh.cpp"
#include <cassert>
#include <cstdio>

struct Control {
    RECT bounds;
    bool visible = true;
    int fixedHeight = 0;
};

Control title{{0, 0, 180, 32}, true, 32};
Control navigation{{0, 80, 180, 120}, true, 40};
Control home{{70, 80, 110, 120}};
Control canvas{{0, 32, 180, 80}};
Control sideNavigation{{140, 32, 180, 120}, false, 0};
bool matchingLayout = true;
int originalPaintCalls = 0;
HDC dc;
HWND layoutWindow;
HWND nativeVideo;
int bufferedGeometryChanges = 0;
WindowState* adjustedGeometry = nullptr;
int compactHeightAdjustment = 0;
int compactWidthAdjustment = 0;
int compactXAdjustment = 0;
int compactYAdjustment = 0;
bool failRestoreOnce = false;
int layoutScalePercent = 100;
BOOL WINAPI CheckedSetWindowPos(HWND window, HWND after, int x, int y,
                                int width, int height, UINT flags) {
    if (layoutWindow && window == layoutWindow) {
        assert(flags & SWP_NOREDRAW);
        assert(flags & SWP_NOCOPYBITS);
        ++bufferedGeometryChanges;
        if (adjustedGeometry && !adjustedGeometry->compact && failRestoreOnce) {
            failRestoreOnce = false;
            SetWindowPos(window, after, x + 3, y + 2, width, height - 4, flags);
            SetLastError(ERROR_INVALID_FUNCTION);
            return FALSE;
        }
        if (adjustedGeometry && adjustedGeometry->compact) {
            height += compactHeightAdjustment;
            width += compactWidthAdjustment;
            x += compactXAdjustment;
            y += compactYAdjustment;
        }
    }
    return SetWindowPos(window, after, x, y, width, height, flags);
}

void* FindTestControl(const void*, PCWSTR name) {
    if (wcscmp(name, L"titleBar") == 0) return &title;
    if (wcscmp(name, L"navigationbtn") == 0) return &navigation;
    if (wcscmp(name, L"homebtn") == 0) return &home;
    if (wcscmp(name, L"horizon_navigationbtn") == 0) return &sideNavigation;
    if (wcscmp(name, L"canvas") == 0 && matchingLayout) return &canvas;
    return nullptr;
}

bool TestPaint(void* manager, UINT, WPARAM, LPARAM, LRESULT&) {
    PaintBackgroundHook(&title, dc);
    PaintBackgroundHook(&navigation, dc);
    PaintButtonImageHook(&home, dc);
    const DWORD baseColor = 0xff363636;
    const DWORD expected = g_dark.load() && matchingLayout ? kTextColor : baseColor;
    assert(TitleTextColor(manager, RECT{20, 5, 90, 25}, baseColor) == expected);
    assert(TitleTextColor(manager, RECT{20, 40, 90, 60}, baseColor) == baseColor);
    return true;
}

int main() {
    g_findControl = FindTestControl;
    g_setNativePosition = CheckedSetWindowPos;
    g_moveNativeWindow = MoveWindow;
    g_getPaintWindow = [](const void*) -> HWND { return nullptr; };
    g_isVisible = [](const void* p) { return static_cast<const Control*>(p)->visible; };
    g_setVisible = [](void* p, bool visible) { static_cast<Control*>(p)->visible = visible; };
    g_getFixedHeight = [](const void* p) { return static_cast<const Control*>(p)->fixedHeight; };
    g_setFixedHeight = [](void* p, int height) { static_cast<Control*>(p)->fixedHeight = height; };
    g_setPosition = [](void* p, RECT bounds, bool) { static_cast<Control*>(p)->bounds = bounds; };
    g_needUpdate = [](void*) {
        if (!layoutWindow) return;
        RECT client;
        GetClientRect(layoutWindow, &client);
        // Model a renderer that reserves a bar's fixed height even when hidden.
        const int top = MulDiv(title.fixedHeight, layoutScalePercent, 100);
        const int bottom = sideNavigation.visible ? 0 :
            MulDiv(navigation.fixedHeight, layoutScalePercent, 100);
        title.bounds = RECT{0, 0, client.right, top};
        navigation.bounds = RECT{0, client.bottom - bottom, client.right, client.bottom};
        sideNavigation.bounds = RECT{client.right - 40, top, client.right, client.bottom};
        // The internal Canvas shares layout space with another flexible control.
        canvas.bounds = RECT{0, top, client.right, top + (client.bottom - bottom - top) / 2};
    };
    g_getPosition = [](const void* p) -> const RECT& { return static_cast<const Control*>(p)->bounds; };
    g_paintBackground = [](void* p, HDC dest) {
        ++originalPaintCalls;
        FillRect(dest, &g_getPosition(p), static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    };
    g_paintButtonImage = [](void*, HDC) { ++originalPaintCalls; };
    g_getButtonState = [](void*) -> UINT { return 0; };
    g_isEnabled = [](const void*) { return true; };
    g_messageHandler = TestPaint;

    Gdiplus::GdiplusStartupInput input;
    assert(Gdiplus::GdiplusStartup(&g_gdiplus, &input, nullptr) == Gdiplus::Ok);
    dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 180;
    info.bmiHeader.biHeight = -120;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    assert(bitmap);
    auto oldBitmap = SelectObject(dc, bitmap);
    LRESULT result = 0;
    g_dark.store(true);
    assert(MessageHandlerHook(&title, WM_PAINT, 0, 0, result));
    assert(GetPixel(dc, 0, 0) == RGB(32, 32, 32));
    assert(GetPixel(dc, 0, 119) == RGB(32, 32, 32));
    bool glyphPresent = false;
    for (int y = 90; y < 110; ++y) {
        for (int x = 80; x < 100; ++x) {
            glyphPresent |= GetRValue(GetPixel(dc, x, y)) > 80;
        }
    }
    assert(glyphPresent && originalPaintCalls == 0);

    g_dark.store(false);
    assert(MessageHandlerHook(&title, WM_PAINT, 0, 0, result));
    assert(GetPixel(dc, 0, 0) == RGB(255, 255, 255));
    assert(GetPixel(dc, 0, 119) == RGB(255, 255, 255));
    assert(originalPaintCalls == 3);

    g_dark.store(true);
    matchingLayout = false;
    assert(MessageHandlerHook(&title, WM_PAINT, 0, 0, result));
    assert(originalPaintCalls == 6);
    assert(g_paintContext == nullptr);

    assert(ParseThemeMode(nullptr) == ThemeMode::System);
    assert(ParseThemeMode(L"system") == ThemeMode::System);
    assert(ParseThemeMode(L"light") == ThemeMode::Light);
    assert(ParseThemeMode(L"dark") == ThemeMode::Dark);
    assert(ParseThemeMode(L"custom") == ThemeMode::Custom);
    assert(ParseColor(L" #12abEF ", 0) == 0xff12abef);
    assert(ParseColor(L"123456", 0) == 0xff123456);
    assert(ParseColor(L"#000000", 1) == 0xff000000);
    assert(ParseColor(L"bad-value", kDarkBackground) == kDarkBackground);
    assert(ParseColor(L"", kDarkBackground) == kDarkBackground);
    assert(ParseColor(nullptr, kDarkBackground) == kDarkBackground);
    for (bool dark : {false, true}) {
        const auto system = ResolvePalette(BarTheme{}, dark);
        assert(system.customPaint == dark);
        const auto light = ResolvePalette(BarTheme{ThemeMode::Light}, dark);
        assert(!light.customPaint && light.background == 0xffffffff);
        const auto forcedDark = ResolvePalette(BarTheme{ThemeMode::Dark}, dark);
        assert(forcedDark.customPaint && forcedDark.background == kDarkBackground);
        const auto custom =
            ResolvePalette(BarTheme{ThemeMode::Custom, 0xff123456, 0xfffedcba}, dark);
        assert(custom.customPaint && custom.background == 0xff123456 &&
               custom.foreground == 0xfffedcba);
    }

    // Verify reversal is continuous and duration/ends are bounded.
    assert(AdvanceVisibility(1, false, 250, 120, 500) == 0.5f);
    assert(AdvanceVisibility(0.5f, true, 30, 120, 500) == 0.75f);
    assert(AdvanceVisibility(0.75f, true, 1000, 120, 500) == 1);
    assert(AdvanceVisibility(1, false, 1000, 120, 500) == 0);
    assert(EaseVisibility(0.5f) == 0.5f);

    WindowState state{};
    state.manager = &title;
    state.window = CreateWindowExW(0, L"STATIC", L"Region test", WS_POPUP,
                                    0, 0, 180, 120, nullptr, nullptr, nullptr, nullptr);
    assert(state.window);
    HRGN actual = CreateRectRgn(0, 0, 0, 0);
    UpdateTransparentRegion(state, 0.5f);
    assert(GetWindowRgn(state.window, actual) != ERROR);
    assert(!PtInRegion(actual, 10, 5));
    assert(PtInRegion(actual, 10, 20));
    assert(PtInRegion(actual, 10, 60));
    assert(!PtInRegion(actual, 10, 110));
    UpdateTransparentRegion(state, 0);
    GetWindowRgn(state.window, actual);
    assert(!PtInRegion(actual, 10, 20));
    assert(!PtInRegion(actual, 10, 100));
    assert(PtInRegion(actual, 10, 60));
    UpdateTransparentRegion(state, 1);
    assert(!state.regionApplied && !state.originalRegion);
    assert(GetWindowRgn(state.window, actual) == ERROR);

    // Inner bars leave a non-client perimeter. Hide that perimeter as well.
    title.bounds = RECT{2, 3, 178, 35};
    navigation.bounds = RECT{2, 79, 178, 119};
    UpdateTransparentRegion(state, 0);
    GetWindowRgn(state.window, actual);
    assert(!PtInRegion(actual, 0, 20));
    assert(!PtInRegion(actual, 179, 20));
    assert(!PtInRegion(actual, 0, 100));
    assert(!PtInRegion(actual, 179, 119));
    RECT outline;
    GetRgnBox(actual, &outline);
    assert(outline.top == 35 && outline.bottom == 79);
    assert(PtInRegion(actual, 90, 50));
    RestoreWindowRegion(state);
    title.bounds = RECT{0, 0, 180, 32};
    navigation.bounds = RECT{0, 80, 180, 120};

    HRGN rounded = CreateRoundRectRgn(0, 0, 180, 120, 12, 12);
    HRGN expected = CreateRectRgn(0, 0, 0, 0);
    CombineRgn(expected, rounded, nullptr, RGN_COPY);
    assert(SetWindowRgn(state.window, rounded, FALSE));
    UpdateTransparentRegion(state, 0);
    RestoreWindowRegion(state);
    GetWindowRgn(state.window, actual);
    assert(EqualRgn(expected, actual));
    DeleteObject(expected);
    DeleteObject(actual);
    DestroyWindow(state.window);

    // Exercise real HWND geometry with a native resize frame.
    WindowState geometry{};
    geometry.sessionNeedsRefresh = false;
    geometry.readyAfter = 0;
    matchingLayout = true;
    geometry.manager = &title;
    geometry.window = CreateWindowExW(0, L"STATIC", L"Geometry test",
        WS_POPUP | WS_THICKFRAME, 50, 60, 280, 260,
        nullptr, nullptr, nullptr, nullptr);
    assert(geometry.window);
    layoutWindow = geometry.window;
    g_needUpdate(geometry.manager);
    RECT expanded;
    GetWindowRect(geometry.window, &expanded);
    const RECT viewport = PhoneViewport(geometry);
    const int phoneHeight = viewport.bottom - viewport.top;
    assert(canvas.bounds.bottom - canvas.bounds.top == phoneHeight / 2);
    POINT phoneOrigin{viewport.left, viewport.top};
    ClientToScreen(geometry.window, &phoneOrigin);
    nativeVideo = CreateWindowExW(0, L"BUTTON", L"Loading..", WS_CHILD | WS_VISIBLE,
        viewport.left, viewport.top,
        viewport.right - viewport.left, phoneHeight,
        geometry.window, nullptr, nullptr, nullptr);
    assert(nativeVideo);
    g_windows.emplace(geometry.window, &geometry);
    g_getPaintWindow = [](const void*) { return layoutWindow; };
    const LONG_PTR styleBefore = GetWindowLongPtrW(geometry.window, GWL_STYLE);
    const bool wasVisible = BeginGeometryChange(geometry);
    const int changesBeforeNestedTick = bufferedGeometryChanges;
    TickWindow(geometry);
    assert(!CompactWindow(geometry));
    assert(!RestoreCompactWindow(geometry));
    assert(bufferedGeometryChanges == changesBeforeNestedTick);
    const int paintsBefore = originalPaintCalls;
    LRESULT deferredResult = -1;
    assert(MessageHandlerHook(geometry.manager, WM_PAINT, 0, 0, deferredResult));
    assert(deferredResult == 0 && originalPaintCalls == paintsBefore);
    assert(GetWindowLongPtrW(geometry.window, GWL_STYLE) == styleBefore);
    EndGeometryChange(geometry, wasVisible);
    assert(GetWindowLongPtrW(geometry.window, GWL_STYLE) == styleBefore);
    g_getPaintWindow = [](const void*) -> HWND { return nullptr; };
    for (int cycle = 0; cycle < 3; ++cycle) {
        assert(CompactWindow(geometry));
        RECT compact;
        GetWindowRect(geometry.window, &compact);
        assert(compact.top == expanded.top + 32);
        assert(compact.bottom == expanded.bottom - 40);
        const RECT compactViewport = PhoneViewport(geometry);
        assert(compactViewport.bottom - compactViewport.top == phoneHeight);
        assert(canvas.bounds.bottom - canvas.bounds.top == phoneHeight / 2);
        POINT compactPhone{compactViewport.left, compactViewport.top};
        ClientToScreen(geometry.window, &compactPhone);
        assert(compactPhone.x == phoneOrigin.x && compactPhone.y == phoneOrigin.y);
        assert(!title.visible && !navigation.visible);
        assert(title.fixedHeight == 0 && navigation.fixedHeight == 0);
        assert(canvas.bounds.top == 0);
        assert(geometry.videoWindow == nativeVideo);
        RECT video;
        GetWindowRect(nativeVideo, &video);
        assert(video.top == phoneOrigin.y);
        assert(video.bottom - video.top == phoneHeight);
        MoveNativeWindowHook(nativeVideo, 0, 32, 90, 100, FALSE);
        GetWindowRect(nativeVideo, &video);
        assert(video.top == phoneOrigin.y);
        assert(video.bottom - video.top == phoneHeight);
        SetNativePositionHook(nativeVideo, nullptr, 0, 32, 90, 100,
                              SWP_NOZORDER | SWP_NOACTIVATE);
        GetWindowRect(nativeVideo, &video);
        assert(video.top == phoneOrigin.y);
        assert(video.bottom - video.top == phoneHeight);
        assert(RestoreCompactWindow(geometry));
        RECT restored;
        GetWindowRect(geometry.window, &restored);
        assert(EqualRect(&expanded, &restored));
        assert(title.visible && navigation.visible);
        assert(title.fixedHeight == 32 && navigation.fixedHeight == 40);
        GetWindowRect(nativeVideo, &video);
        assert(video.top == phoneOrigin.y);
        assert(video.bottom - video.top == phoneHeight);
    }
    // Posted DuiLib layout can run AFTER restore returns. It must not shrink
    // the native video surface, even if both positioning APIs run repeatedly.
    assert(WindhawkUtils::SetWindowSubclassFromAnyThread(
        geometry.window, WindowSubclass, reinterpret_cast<DWORD_PTR>(&geometry)));
    for (int cycle = 0; cycle < 100; ++cycle) {
        assert(CompactWindow(geometry));
        assert(RestoreCompactWindow(geometry));
        WINDOWPOS delayedParent{};
        delayedParent.cx = expanded.right - expanded.left;
        delayedParent.cy = expanded.bottom - expanded.top - 72;
        ConstrainCompactResize(geometry, delayedParent);
        assert(delayedParent.cy == expanded.bottom - expanded.top);
        SetWindowPos(geometry.window, nullptr, 0, 0, delayedParent.cx,
            expanded.bottom - expanded.top - 72,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        RECT delayedBounds;
        GetWindowRect(geometry.window, &delayedBounds);
        assert(EqualRect(&delayedBounds, &expanded));
        MoveNativeWindowHook(nativeVideo, viewport.left, viewport.top,
            viewport.right - viewport.left, phoneHeight - 72, FALSE);
        SetNativePositionHook(nativeVideo, nullptr, viewport.left, viewport.top,
            viewport.right - viewport.left, phoneHeight / 2,
            SWP_NOZORDER | SWP_NOACTIVATE);
        RECT video;
        GetWindowRect(nativeVideo, &video);
        if (video.bottom - video.top != phoneHeight) {
            printf("Delayed video layout regression at cycle %d: expected %d, actual %ld\n",
                cycle + 1, phoneHeight, video.bottom - video.top);
            return 3;
        }
        // SWP_NOSIZE/NOMOVE must not leave a surface at a stale rectangle.
        MoveWindow(nativeVideo, 0, 0, 20, 20, FALSE);
        SetNativePositionHook(nativeVideo, nullptr, 0, 0, 0, 0,
            SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        AlignVideoWindow(geometry);
        GetWindowRect(nativeVideo, &video);
        assert(video.top == phoneOrigin.y);
        assert(video.bottom - video.top == phoneHeight);
        MoveWindow(nativeVideo, 0, 0, 20, 20, FALSE);
        AlignVideoWindow(geometry);
        GetWindowRect(nativeVideo, &video);
        assert(video.top == phoneOrigin.y);
        assert(video.bottom - video.top == phoneHeight);
        // Footer coordinates can still refer to the previous compact client.
        OffsetRect(&navigation.bounds, 0, -72);
        const RECT lateViewport = PhoneViewport(geometry);
        assert(EqualRect(&lateViewport, &viewport));
        g_needUpdate(geometry.manager);
    }
    // Menu commands and hide/show used to discard the restored-height guard.
    // A late compact layout then became the baseline on every following cycle.
    for (int cycle = 0; cycle < 20; ++cycle) {
        assert(CompactWindow(geometry));
        SendMessageW(geometry.window, WM_SHOWWINDOW, FALSE, 0);
        SendMessageW(geometry.window, WM_SHOWWINDOW, TRUE, 0);
        assert(!geometry.compact && geometry.protectExpandedHeight);
        SendMessageW(geometry.window, WM_SYSCOMMAND, SC_KEYMENU, 0);
        assert(geometry.protectExpandedHeight);
        SetWindowPos(geometry.window, nullptr, 0, 0,
            expanded.right - expanded.left, expanded.bottom - expanded.top - 72,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        RECT afterShow;
        GetWindowRect(geometry.window, &afterShow);
        assert(EqualRect(&afterShow, &expanded));
        g_needUpdate(geometry.manager);
        geometry.readyAfter = 0;
        AlignVideoWindow(geometry);
        assert(!geometry.sessionNeedsRefresh);
        assert(CompactWindow(geometry));
        assert(RestoreCompactWindow(geometry));
        GetWindowRect(geometry.window, &afterShow);
        assert(EqualRect(&afterShow, &expanded));
    }
    geometry.moving = true;
    WINDOWPOS manualHeight{};
    manualHeight.cx = expanded.right - expanded.left;
    manualHeight.cy = expanded.bottom - expanded.top + 100;
    ConstrainCompactResize(geometry, manualHeight);
    assert(manualHeight.cy == expanded.bottom - expanded.top + 100);
    assert(!geometry.protectExpandedHeight);
    geometry.moving = false;
    WindhawkUtils::RemoveWindowSubclassFromAnyThread(geometry.window, WindowSubclass);
    // Some hosts round the requested compact height before accepting it.
    adjustedGeometry = &geometry;
    compactHeightAdjustment = -1;
    for (int cycle = 0; cycle < 10; ++cycle) {
        assert(CompactWindow(geometry));
        assert(RestoreCompactWindow(geometry));
        RECT restored;
        GetWindowRect(geometry.window, &restored);
        if (!EqualRect(&expanded, &restored)) {
            printf("Cumulative geometry regression at cycle %d: expected height %ld, actual %ld\n",
                cycle + 1, expanded.bottom - expanded.top, restored.bottom - restored.top);
            return 2;
        }
    }
    compactHeightAdjustment = 0;
    adjustedGeometry = nullptr;

    // Exercise physical bar scaling with native rounding of size and position.
    adjustedGeometry = &geometry;
    compactHeightAdjustment = -1;
    compactWidthAdjustment = -1;
    compactXAdjustment = 1;
    compactYAdjustment = 1;
    for (int percentage : {100, 125, 150, 175, 200}) {
        layoutScalePercent = percentage;
        g_needUpdate(geometry.manager);
        const RECT scaledViewport = PhoneViewport(geometry);
        MoveWindow(nativeVideo, scaledViewport.left, scaledViewport.top,
            scaledViewport.right - scaledViewport.left,
            scaledViewport.bottom - scaledViewport.top, FALSE);
        POINT expectedVideo{scaledViewport.left, scaledViewport.top};
        ClientToScreen(geometry.window, &expectedVideo);
        for (int cycle = 0; cycle < 100; ++cycle) {
            assert(CompactWindow(geometry));
            RECT video;
            GetWindowRect(nativeVideo, &video);
            assert(video.left == expectedVideo.x && video.top == expectedVideo.y);
            assert(RestoreCompactWindow(geometry));
            RECT restored;
            GetWindowRect(geometry.window, &restored);
            assert(EqualRect(&expanded, &restored));
        }
    }
    compactHeightAdjustment = 0;
    compactWidthAdjustment = 0;
    compactXAdjustment = 0;
    compactYAdjustment = 0;
    layoutScalePercent = 100;
    g_needUpdate(geometry.manager);
    MoveWindow(nativeVideo, viewport.left, viewport.top,
        viewport.right - viewport.left, phoneHeight, FALSE);
    assert(CompactWindow(geometry));
    failRestoreOnce = true;
    assert(!RestoreCompactWindow(geometry));
    assert(geometry.compact && geometry.hasGeometrySnapshot);
    assert(RestoreCompactWindow(geometry));
    RECT retried;
    GetWindowRect(geometry.window, &retried);
    assert(EqualRect(&expanded, &retried));
    adjustedGeometry = nullptr;
    assert(CompactWindow(geometry));
    RECT compact;
    GetWindowRect(geometry.window, &compact);
    WINDOWPOS automaticResize{};
    automaticResize.cx = compact.right - compact.left;
    automaticResize.cy = expanded.bottom - expanded.top;
    automaticResize.y = expanded.top;
    ConstrainCompactResize(geometry, automaticResize);
    assert(geometry.compact);
    assert(automaticResize.cy == compact.bottom - compact.top);
    assert(automaticResize.y == compact.top);

    SetWindowPos(geometry.window, nullptr, compact.left + 11, compact.top + 17,
        0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    assert(RestoreCompactWindow(geometry));
    RECT moved;
    GetWindowRect(geometry.window, &moved);
    OffsetRect(&expanded, 11, 17);
    assert(EqualRect(&moved, &expanded));

    // Expanded windows must accept the application's new video size.
    SetWindowPos(geometry.window, nullptr, moved.left, moved.top, 360, 500,
        SWP_NOZORDER | SWP_NOACTIVATE);
    g_needUpdate(geometry.manager);
    const RECT enlargedCanvas = PhoneViewport(geometry);
    const int enlargedWidth = enlargedCanvas.right - enlargedCanvas.left;
    const int enlargedHeight = enlargedCanvas.bottom - enlargedCanvas.top;
    assert(enlargedHeight > phoneHeight);
    assert(canvas.bounds.bottom - canvas.bounds.top == enlargedHeight / 2);
    MoveNativeWindowHook(nativeVideo, enlargedCanvas.left, enlargedCanvas.top,
        enlargedWidth, enlargedHeight / 2, FALSE);
    geometry.sessionNeedsRefresh = true;
    geometry.readyAfter = 0;
    AlignVideoWindow(geometry);
    RECT fullViewportVideo;
    GetWindowRect(nativeVideo, &fullViewportVideo);
    assert(fullViewportVideo.bottom - fullViewportVideo.top == enlargedHeight);
    assert(canvas.bounds.bottom - canvas.bounds.top == enlargedHeight / 2);
    MoveNativeWindowHook(nativeVideo, enlargedCanvas.left, enlargedCanvas.top,
        enlargedWidth, enlargedHeight, FALSE);
    AlignVideoWindow(geometry);
    RECT enlargedVideo;
    GetWindowRect(nativeVideo, &enlargedVideo);
    assert(enlargedVideo.right - enlargedVideo.left == enlargedWidth);
    assert(enlargedVideo.bottom - enlargedVideo.top == enlargedHeight);

    RECT enlargedWindow;
    GetWindowRect(geometry.window, &enlargedWindow);
    assert(CompactWindow(geometry));
    assert(geometry.videoBounds.right - geometry.videoBounds.left == enlargedWidth);
    assert(geometry.videoBounds.bottom - geometry.videoBounds.top == enlargedHeight);
    GetWindowRect(nativeVideo, &enlargedVideo);
    assert(enlargedVideo.right - enlargedVideo.left == enlargedWidth);
    assert(enlargedVideo.bottom - enlargedVideo.top == enlargedHeight);
    assert(RestoreCompactWindow(geometry));
    RECT restoredEnlarged;
    GetWindowRect(geometry.window, &restoredEnlarged);
    assert(EqualRect(&enlargedWindow, &restoredEnlarged));

    // Rotate while compact. Never restore portrait dimensions or its bottom
    // navigation while the application is committing a landscape layout.
    assert(CompactWindow(geometry));
    sideNavigation.visible = true;
    RECT nativeLandscape{80, 32, 580, 282};
    MoveNativeWindowHook(nativeVideo, nativeLandscape.left, nativeLandscape.top,
        500, 250, FALSE);
    RECT nativeActual;
    GetWindowRect(nativeVideo, &nativeActual);
    MapWindowPoints(nullptr, geometry.window, reinterpret_cast<POINT*>(&nativeActual), 2);
    assert(EqualRect(&nativeActual, &nativeLandscape));
    WINDOWPOS rotation{};
    rotation.cx = 700;
    rotation.cy = 320;
    ConstrainCompactResize(geometry, rotation);
    assert(rotation.cx == 700 && rotation.cy == 320);
    assert(!geometry.compact && geometry.landscape);
    assert(!geometry.protectExpandedHeight && !geometry.videoWindow);
    assert(title.visible && title.fixedHeight == 32);
    assert(!navigation.visible && navigation.fixedHeight == 40);
    SetWindowPos(geometry.window, nullptr, 0, 0, rotation.cx, rotation.cy,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    g_needUpdate(geometry.manager);
    SetNativePositionHook(nativeVideo, nullptr, nativeLandscape.left, nativeLandscape.top,
        500, 250, SWP_NOZORDER | SWP_NOACTIVATE);
    RECT landscapeExpanded, landscapeVideo;
    GetWindowRect(geometry.window, &landscapeExpanded);
    GetWindowRect(nativeVideo, &landscapeVideo);
    geometry.sessionNeedsRefresh = false;
    geometry.readyAfter = 0;
    for (int cycle = 0; cycle < 100; ++cycle) {
        assert(CompactWindow(geometry));
        RECT compactLandscape;
        GetWindowRect(geometry.window, &compactLandscape);
        assert(compactLandscape.top == landscapeExpanded.top + 32);
        assert(compactLandscape.bottom == landscapeExpanded.bottom);
        assert(!title.visible && title.fixedHeight == 0);
        assert(geometry.compactBottom == 0);
        GetWindowRect(nativeVideo, &nativeActual);
        assert(EqualRect(&nativeActual, &landscapeVideo));
        MoveNativeWindowHook(nativeVideo, 0, 0, 100, 100, FALSE);
        SetNativePositionHook(nativeVideo, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetWindowRect(nativeVideo, &nativeActual);
        assert(EqualRect(&nativeActual, &landscapeVideo));
        assert(RestoreCompactWindow(geometry));
        RECT restoredLandscape;
        GetWindowRect(geometry.window, &restoredLandscape);
        assert(EqualRect(&restoredLandscape, &landscapeExpanded));
        GetWindowRect(nativeVideo, &nativeActual);
        MapWindowPoints(nullptr, geometry.window, reinterpret_cast<POINT*>(&nativeActual), 2);
        assert(EqualRect(&nativeActual, &nativeLandscape));
        assert(!navigation.visible && sideNavigation.visible);
    }
    // Return to portrait and establish a fresh baseline at the native size.
    assert(CompactWindow(geometry));
    sideNavigation.visible = false;
    navigation.visible = true;
    rotation.cx = enlargedWindow.right - enlargedWindow.left;
    rotation.cy = enlargedWindow.bottom - enlargedWindow.top;
    ConstrainCompactResize(geometry, rotation);
    assert(!geometry.landscape && geometry.sessionNeedsRefresh);
    assert(navigation.visible && navigation.fixedHeight == 40);
    SetWindowPos(geometry.window, nullptr, 0, 0, rotation.cx, rotation.cy,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    g_needUpdate(geometry.manager);
    // The native rotation retained the compact window's desktop origin.
    // Subsequent hover cycles must preserve this new application-chosen origin.
    GetWindowRect(geometry.window, &enlargedWindow);
    assert(enlargedWindow.right - enlargedWindow.left == rotation.cx);
    assert(enlargedWindow.bottom - enlargedWindow.top == rotation.cy);
    geometry.readyAfter = 0;
    AlignVideoWindow(geometry);
    assert(geometry.videoWindow == nativeVideo && !geometry.sessionNeedsRefresh);
    assert(CompactWindow(geometry));
    assert(RestoreCompactWindow(geometry));
    GetWindowRect(geometry.window, &restoredEnlarged);
    assert(EqualRect(&enlargedWindow, &restoredEnlarged));

    // Disconnect while compact, then reconnect with a new, undersized video child.
    assert(WindhawkUtils::SetWindowSubclassFromAnyThread(
        geometry.window, WindowSubclass, reinterpret_cast<DWORD_PTR>(&geometry)));
    assert(CompactWindow(geometry));
    SendMessageW(geometry.window, WM_SHOWWINDOW, FALSE, 0);
    assert(!geometry.compact && geometry.sessionNeedsRefresh);
    assert(!geometry.videoWindow && !geometry.canvas);
    assert(title.visible && navigation.visible);
    RECT disconnected;
    GetWindowRect(geometry.window, &disconnected);
    assert(EqualRect(&disconnected, &enlargedWindow));
    DestroyWindow(nativeVideo);
    nativeVideo = CreateWindowExW(0, L"BUTTON", L"Loading..", WS_CHILD | WS_VISIBLE,
        enlargedCanvas.left, enlargedCanvas.top, enlargedWidth - 40, enlargedHeight - 72,
        geometry.window, nullptr, nullptr, nullptr);
    assert(nativeVideo);
    SendMessageW(geometry.window, WM_SHOWWINDOW, TRUE, 0);
    assert(geometry.sessionNeedsRefresh);
    assert(!CompactWindow(geometry));
    geometry.readyAfter = 0;
    AlignVideoWindow(geometry);
    assert(!geometry.sessionNeedsRefresh);
    assert(geometry.videoWindow == nativeVideo);
    RECT reconnected;
    GetWindowRect(nativeVideo, &reconnected);
    assert(reconnected.right - reconnected.left == enlargedWidth);
    assert(reconnected.bottom - reconnected.top == enlargedHeight);
    assert(CompactWindow(geometry));
    GetWindowRect(nativeVideo, &reconnected);
    assert(reconnected.bottom - reconnected.top == enlargedHeight);
    assert(RestoreCompactWindow(geometry));
    GetWindowRect(geometry.window, &reconnected);
    assert(EqualRect(&reconnected, &enlargedWindow));
    WindhawkUtils::RemoveWindowSubclassFromAnyThread(geometry.window, WindowSubclass);
    assert(bufferedGeometryChanges >= 8);
    assert(!IsWindowVisible(geometry.window));
    g_windows.erase(geometry.window);
    DestroyWindow(nativeVideo);
    nativeVideo = nullptr;
    DestroyWindow(geometry.window);
    layoutWindow = nullptr;
    title.bounds = RECT{0, 0, 180, 32};
    navigation.bounds = RECT{0, 80, 180, 120};
    canvas.bounds = RECT{0, 32, 180, 80};

    g_paintContainer = [](void* p, HDC dest, const RECT&) {
        FillRect(dest, &g_getPosition(p), static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    };
    PaintContext context{};
    context.title = &title;
    context.navigation = &navigation;
    context.opacity = 0.5f;
    g_paintContext = &context;
    RECT surface{0, 0, 180, 120};
    FillRect(dc, &surface, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    PaintContainerHook(&title, dc, surface);
    assert(GetRValue(GetPixel(dc, 10, 10)) >= 126 &&
           GetRValue(GetPixel(dc, 10, 10)) <= 129);
    assert(GetPixel(dc, 10, 50) == RGB(0, 0, 0));
    context.opacity = 0;
    PaintContainerHook(&navigation, dc, surface);
    assert(GetPixel(dc, 10, 100) == RGB(0, 0, 0));
    context.opacity = 1;
    PaintContainerHook(&navigation, dc, surface);
    assert(GetPixel(dc, 10, 100) == RGB(255, 255, 255));
    g_paintContext = nullptr;

    PaintContext colors{};
    colors.manager = &title;
    colors.title = &title;
    colors.navigation = &navigation;
    colors.buttons[7] = &home;
    colors.topPalette =
        ResolvePalette(BarTheme{ThemeMode::Custom, 0xff123456, 0xfffedcba}, false);
    colors.bottomPalette = ResolvePalette(BarTheme{ThemeMode::Dark}, false);
    g_paintContext = &colors;
    PaintBackgroundHook(&title, dc);
    PaintBackgroundHook(&navigation, dc);
    assert(GetPixel(dc, 1, 1) == RGB(0x12, 0x34, 0x56));
    assert(GetPixel(dc, 1, 100) == RGB(32, 32, 32));
    assert(TitleTextColor(&title, RECT{20, 5, 90, 25}, 0) == 0xfffedcba);
    assert(TitleTextColor(&title, RECT{20, 40, 90, 60}, 0xff010203) == 0xff010203);
    const int beforeCustomGlyph = originalPaintCalls;
    PaintButtonImageHook(&home, dc);
    assert(originalPaintCalls == beforeCustomGlyph);
    colors.bottomPalette = ResolvePalette(BarTheme{ThemeMode::Light}, true);
    PaintBackgroundHook(&navigation, dc);
    assert(GetPixel(dc, 1, 100) == RGB(255, 255, 255));
    PaintButtonImageHook(&home, dc);
    assert(originalPaintCalls == beforeCustomGlyph + 2);
    g_paintContext = nullptr;

    // Check the native snapshot surface and its input/cleanup behavior without showing it.
    lstrcpyW(g_coverClassName, L"MirrorThemeTransitionCoverTest");
    WNDCLASSW coverClass{};
    coverClass.lpfnWndProc = TransitionCoverProc;
    coverClass.hInstance = GetModuleHandleW(nullptr);
    coverClass.lpszClassName = g_coverClassName;
    g_coverClass = RegisterClassW(&coverClass);
    assert(g_coverClass);
    HWND coverOwner = CreateWindowExW(0, L"STATIC", L"Cover owner", WS_POPUP,
        0, 0, 180, 120, nullptr, nullptr, nullptr, nullptr);
    HWND cover = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        g_coverClassName, L"", WS_POPUP, 0, 0, 180, 120,
        coverOwner, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(cover && coverOwner);
    POINT coverPosition{}, sourcePosition{};
    SIZE coverSize{180, 120};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, 0};
    assert(UpdateLayeredWindow(cover, nullptr, &coverPosition, &coverSize,
        dc, &sourcePosition, 0, &blend, ULW_ALPHA));
    assert(!IsWindowVisible(cover));
    assert(TransitionCoverProc(cover, WM_NCHITTEST, 0, 0) == HTTRANSPARENT);
    assert(TransitionCoverProc(cover, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE);
    auto* coverState = new WindowState{};
    coverState->window = coverOwner;
    coverState->transitionCover = cover;
    coverState->coverUntil = GetTickCount64() + 100;
    DismissTransitionCover(*coverState);
    assert(coverState->coverUntil == 0 && !IsWindowVisible(cover));
    ForgetWindow(coverState);
    assert(!IsWindow(cover));
    DestroyWindow(coverOwner);

    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseResources();
    puts("Landscape rotation and 100 native-layout cycles, 100 delayed-layout hover cycles, 500 scaled round trips, resize, reconnect and color checks passed.");
}

