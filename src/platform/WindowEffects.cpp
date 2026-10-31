/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "WindowEffects.h"

#include <QOperatingSystemVersion>
#include <QWidget>

#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWCP_DEFAULT
#define DWMWCP_DEFAULT 0
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMSBT_TABBEDWINDOW
#define DWMSBT_TABBEDWINDOW 4
#endif

namespace {

bool isWindows11OrGreater()
{
    const QOperatingSystemVersion os = QOperatingSystemVersion::current();
    return os.type() == QOperatingSystemVersion::Windows
        && os.majorVersion() == 10
        && os.microVersion() >= 22000;
}

HWND nativeHandle(QWidget *window)
{
    return window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
}

struct AccentPolicy {
    int accentState;
    int accentFlags;
    DWORD gradientColor;
    int animationId;
};

struct CompositionAttributeData {
    DWORD attribute;
    PVOID data;
    SIZE_T sizeOfData;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI *)(HWND, CompositionAttributeData *);

void setLegacyAccent(HWND hwnd, bool enabled)
{
    static SetWindowCompositionAttributeFn setAttribute = []() -> SetWindowCompositionAttributeFn {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) {
            return nullptr;
        }
        FARPROC address = GetProcAddress(user32, "SetWindowCompositionAttribute");
        SetWindowCompositionAttributeFn function = nullptr;
        static_assert(sizeof(function) == sizeof(address),
                      "Unexpected Windows function pointer size");
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }();

    if (!hwnd || !setAttribute) {
        return;
    }

    AccentPolicy policy{};
    policy.accentState = enabled ? 4 : 0; // ACCENT_ENABLE_ACRYLICBLURBEHIND / DISABLED
    policy.accentFlags = enabled ? 2 : 0;
    policy.gradientColor = enabled ? 0xB017110C : 0; // AABBGGRR, dark blue-grey tint

    CompositionAttributeData data{};
    data.attribute = 19; // WCA_ACCENT_POLICY
    data.data = &policy;
    data.sizeOfData = sizeof(policy);
    setAttribute(hwnd, &data);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void setDarkMode(HWND hwnd)
{
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enabled, sizeof(enabled));
}

void configureBackdrop(HWND hwnd, bool acrylic)
{
    if (!hwnd) {
        return;
    }

    const MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    setDarkMode(hwnd);

    bool usedSystemBackdrop = false;
    if (isWindows11OrGreater()) {
        const int backdrop = acrylic ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
        const HRESULT result = DwmSetWindowAttribute(
            hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        usedSystemBackdrop = SUCCEEDED(result);
    }

    // Windows 10, or a Windows 11 build where the backdrop attribute is not
    // available, uses the older accent blur API instead.
    setLegacyAccent(hwnd, acrylic && !usedSystemBackdrop);
}

void configureCorners(HWND hwnd, bool rounded)
{
    if (!hwnd || !isWindows11OrGreater()) {
        return;
    }

    const int preference = rounded ? DWMWCP_ROUND : DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
}

} // namespace
#endif

namespace WindowEffects {

bool isAvailable()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void apply(QWidget *window, bool acrylic, bool roundedCorners)
{
#ifdef Q_OS_WIN
    HWND hwnd = nativeHandle(window);
    configureBackdrop(hwnd, acrylic);
    configureCorners(hwnd, roundedCorners);
#else
    Q_UNUSED(window)
    Q_UNUSED(acrylic)
    Q_UNUSED(roundedCorners)
#endif
}

void setAcrylic(QWidget *window, bool enabled)
{
#ifdef Q_OS_WIN
    configureBackdrop(nativeHandle(window), enabled);
#else
    Q_UNUSED(window)
    Q_UNUSED(enabled)
#endif
}

void setRoundedCorners(QWidget *window, bool rounded)
{
#ifdef Q_OS_WIN
    configureCorners(nativeHandle(window), rounded);
#else
    Q_UNUSED(window)
    Q_UNUSED(rounded)
#endif
}

} // namespace WindowEffects
