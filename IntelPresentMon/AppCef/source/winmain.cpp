// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "NanoCefBrowserClient.h"
#include "NanoCefProcessHandler.h"
#include "../resource.h"
#include <Core/source/infra/Logging.h>
#include <Core/source/infra/util/FolderResolver.h>
#include <Core/source/cli/CliOptions.h>
#include <CommonUtilities/log/IdentificationTable.h>
#include <CommonUtilities/generated/build_id.h>
#include <dwmapi.h>

#pragma comment(lib, "Dwmapi.lib")

using namespace p2c;
using namespace pmon::util;
using p2c::cli::Options;
namespace ccef = client::cef;
using namespace std::chrono_literals;

// globals
constexpr const char* BrowserWindowClassName = "BrowserWindowClass";
constexpr const char* MessageWindowClassName = "MessageWindowClass";
constexpr const int quitCefCode = 0xABAD1DEA;
CefRefPtr<client::cef::NanoCefBrowserClient> pBrowserClient;
HWND hwndAppMsg = nullptr;

LRESULT CALLBACK BrowserWindowWndProc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_CLOSE:
        if (pBrowserClient) {
            if (auto lResult = pBrowserClient->HandleCloseMessage()) {
                return *lResult;
            }
        }
        break;
    case WM_CREATE:
    {
        pBrowserClient = new ccef::NanoCefBrowserClient{};

        RECT rect = { 0 };
        GetClientRect(window_handle, &rect);
        CefRect cefRect;
        cefRect.x = rect.left;
        cefRect.y = rect.top;
        cefRect.width = rect.right - rect.left;
        cefRect.height = rect.bottom - rect.top;

        CefWindowInfo info;
        info.SetAsChild(window_handle, cefRect);

        CefBrowserSettings settings;
        // this special url (domain+schema) triggers load-from-disk behavior
        std::string url = "https://app/index.html";
        if (Options::Get().url) {
            url = *Options::Get().url;
        }
        CefBrowserHost::CreateBrowser(
            info, pBrowserClient.get(), url,
            settings, {}, {}
        );
        break;
    }
    case WM_SIZE:
        // from the cefclient example, do not allow the window to be resized to 0x0 or the layout will break;
        // also be aware that if the size gets too small, GPU acceleration disables
        if ((w_param != SIZE_MINIMIZED)
            && (pBrowserClient.get())
            && (pBrowserClient->GetBrowser()))
        {
            CefWindowHandle hwnd(pBrowserClient->GetBrowser()->GetHost()->GetWindowHandle());
            if (hwnd)
            {
                RECT rect = { 0 };
                GetClientRect(window_handle, &rect);
                HDWP hdwp = BeginDeferWindowPos(1);
                hdwp = DeferWindowPos(hdwp, hwnd, NULL, rect.left,
                    rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
                EndDeferWindowPos(hdwp);
            }
        }
        break;
    case WM_ERASEBKGND:
        if (pBrowserClient.get()
            && pBrowserClient->GetBrowser()
            && pBrowserClient->GetBrowser()->GetHost()->GetWindowHandle() != nullptr)
        {
            return 1;
        }
        break;
    case WM_ENTERMENULOOP:
        if (!w_param)
        {
            CefSetOSModalLoop(true);
        }
        break;
    case WM_EXITMENULOOP:
        if (!w_param)
        {
            CefSetOSModalLoop(false);
        }
        break;
    }
    return DefWindowProc(window_handle, message, w_param, l_param);
}

LRESULT CALLBACK MessageWindowWndProc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_COMMAND:
        if (w_param == quitCefCode)
        {
            PostQuitMessage(0);
        }
        break;
    }
    return DefWindowProc(window_handle, message, w_param, l_param);
}

#define PROGRAM_NAME "Intel PresentMon"

HWND CreateBrowserWindow(HINSTANCE instance_handle, int show_minimize_or_maximize)
{
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = BrowserWindowWndProc;
    wcex.hInstance = instance_handle;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.lpszClassName = BrowserWindowClassName;
    wcex.hIcon = static_cast<HICON>(LoadImage(
        instance_handle, MAKEINTRESOURCE(IDI_ICON1),
        IMAGE_ICON, 32, 32, 0
    ));
    RegisterClassEx(&wcex);

    HWND hwnd = CreateWindow(
        BrowserWindowClassName, PROGRAM_NAME,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 200, 20,
        1360, 1020, nullptr, nullptr, instance_handle, nullptr
    );

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));

    ShowWindow(hwnd, show_minimize_or_maximize);
    UpdateWindow(hwnd);

    return hwnd;
}

HWND CreateMessageWindow(HINSTANCE instance_handle)
{
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = MessageWindowWndProc;
    wcex.hInstance = instance_handle;
    wcex.lpszClassName = MessageWindowClassName;
    RegisterClassEx(&wcex);
    return CreateWindow(MessageWindowClassName, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, instance_handle, 0);
}

void AppQuitMessageLoop()
{
    if (hwndAppMsg != nullptr)
    {
        PostMessage(hwndAppMsg, WM_COMMAND, quitCefCode, 0);
    }
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef NDEBUG
    std::wstring _path = std::filesystem::current_path();
    if (_path.find(L"\\Program Files\\", 0) == -1LL) // -1 means no offset was found
    {
        wchar_t msg[512]{};
        (void)_snwprintf_s(msg, _countof(msg),
                           L"Unable to start " PROGRAM_NAME " due to path not being in privileged location.\n"
                           "Path: \"%s\"\n"
                           "You must copy installation files to privileged location such as \"C:\\Program Files\".", _path.c_str());
        MessageBoxW(nullptr, msg, L"" PROGRAM_NAME " Startup Error", MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
        return -1;
    }
#endif
#undef PROGRAM_NAME

#ifdef NDEBUG
    constexpr bool is_debug = false;
#else
    constexpr bool is_debug = true;
#endif
    // create logging system and ensure cleanup before main ext
    LogChannelManager zLogMan_;
    // parse the command line arguments and make them globally available
    if (auto err = Options::Init(__argc, __argv, true)) {
        if (*err == 0) {
            MessageBoxA(nullptr, Options::GetDiagnostics().c_str(), "Command Line Help",
                MB_ICONINFORMATION | MB_APPLMODAL | MB_SETFOREGROUND);
        }
        else {
            MessageBoxA(nullptr, Options::GetDiagnostics().c_str(), "Command Line Parse Error",
                MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
        }
        return *err;
    }
    const auto& opt = Options::Get();
    // wait for debugger connection
    if (opt.cefType && *opt.cefType == "renderer" && opt.debugWaitRender) {
        while (!IsDebuggerPresent()) {
            std::this_thread::sleep_for(20ms);
        }
        DebugBreak();
    }
    // name this process / thread
    log::IdentificationTable::AddThisProcess(str::ToWide(Options::Get().cefType.AsOptional().value_or("main-client")));
    log::IdentificationTable::AddThisThread(L"main");
    // configure the logging system (partially based on command line options)
    ConfigureLogging();

    using namespace client;
    try {
        CefMainArgs main_args{ hInstance };
        CefRefPtr<ccef::NanoCefProcessHandler> app = new ccef::NanoCefProcessHandler{};

        if (const auto code = CefExecuteProcess(main_args, app.get(), nullptr); code >= 0) {
            return (int)code;
        }

        // code from here on is only executed by the root process (browser window process)

        pmlog_info(std::format(L"== client section starting build#{} clean:{} ==", PM_BID_GIT_HASH_SHORT, !PM_BID_DIRTY));

        {
            auto& folderResolver = infra::util::FolderResolver::Get();
            CefSettings settings;
            settings.multi_threaded_message_loop = true;
            settings.remote_debugging_port = is_debug ? 9009 : 0;
            settings.background_color = { 0x000000 };
            CefString(&settings.cache_path).FromWString(folderResolver.Resolve(infra::util::FolderResolver::Folder::App, L"cef-cache"));
            if (opt.logFolder) {
                CefString(&settings.log_file).FromString(*opt.logFolder + "\\cef-debug.log");
            }
            else {
                CefString(&settings.log_file).FromWString(folderResolver.Resolve(infra::util::FolderResolver::Folder::App, L"logs\\cef-debug.log"));
            }
            settings.log_severity = is_debug ? cef_log_severity_t::LOGSEVERITY_DEFAULT : cef_log_severity_t::LOGSEVERITY_ERROR;
            CefInitialize(main_args, settings, app.get(), nullptr);
        }
        auto hwndBrowser = CreateBrowserWindow(hInstance, nCmdShow);
        hwndAppMsg = CreateMessageWindow(hInstance);


        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        DestroyWindow(hwndAppMsg);
        CefShutdown();
        DestroyWindow(hwndBrowser);

        UnregisterClass(BrowserWindowClassName, hInstance);
        UnregisterClass(MessageWindowClassName, hInstance);

        pmlog_info(L"== client process exiting ==");

        return (int)msg.wParam;
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Fatal Error", MB_ICONERROR|MB_APPLMODAL|MB_SETFOREGROUND);
        return -1;
    }
    catch (...) {
        MessageBoxA(nullptr, "Unidentified exception was thrown", "Fatal Error",
            MB_ICONERROR|MB_APPLMODAL|MB_SETFOREGROUND);
        return -1;
    }
}