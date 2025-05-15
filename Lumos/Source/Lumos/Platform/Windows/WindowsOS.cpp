#include "Precompiled.h"
#include "WindowsOS.h"
#include "WindowsPower.h"
#include "WindowsWindow.h"
#include "Core/CoreSystem.h"
#include "Core/OS/MemoryManager.h"
#include "Core/Application.h"
#include "Maths/Vector4.h"
#include <Windows.h>

#ifdef LUMOS_USE_GLFW_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32

#include "Platform/GLFW/GLFWWindow.h"
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#endif

#include <filesystem>
#include <shellapi.h>
#include <dwmapi.h>
#include <winuser.h>

extern Lumos::Application* Lumos::CreateApplication();

namespace Lumos
{
    void WindowsOS::Run()
    {
        auto power       = WindowsPower();
        auto percentage  = power.GetPowerPercentageLeft();
        auto secondsLeft = power.GetPowerSecondsLeft();
        auto state       = power.GetPowerState();

        LINFO("--------------------");
        LINFO(" System Information ");
        LINFO("--------------------");

        if(state != PowerState::POWERSTATE_NO_BATTERY)
            LINFO("Battery Info - Percentage : %d , Time Left %ds , State : %s", percentage, secondsLeft, PowerStateToString(state).c_str());
        else
            LINFO("Power - Outlet");

        auto systemInfo = MemoryManager::Get().GetSystemInfo();
        systemInfo.Log();

        auto& app = Lumos::Application::Get();
        app.Init();
        app.Run();
        app.Release();
    }

    void WindowsOS::Init()
    {
#ifdef LUMOS_USE_GLFW_WINDOWS
        GLFWWindow::MakeDefault();
#else
        WindowsWindow::MakeDefault();
#endif
    }

    SystemMemoryInfo MemoryManager::GetSystemInfo()
    {
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&status);

        SystemMemoryInfo result = {
            (int64_t)status.ullAvailPhys,
            (int64_t)status.ullTotalPhys,

            (int64_t)status.ullAvailVirtual,
            (int64_t)status.ullTotalVirtual
        };
        return result;
    }

    std::string WindowsOS::GetExecutablePath()
    {
        WCHAR path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);

        std::string convertedString = std::filesystem::path(path).string();
        std::replace(convertedString.begin(), convertedString.end(), '\\', '/');

        return convertedString;
    }

    void WindowsOS::OpenFileLocation(const std::string& path)
    {
        std::filesystem::path fsPath = path;
        ShellExecuteA(NULL, "open", std::filesystem::is_directory(fsPath) ? path.c_str() : fsPath.parent_path().string().c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    void WindowsOS::OpenFileExternal(const std::string& path)
    {
        ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    void WindowsOS::OpenURL(const std::string& url)
    {
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

#include <glfw/glfw3native.h>

    void WindowsOS::SetTitleBarColour(const Vec4& colour, bool dark)
    {
#if WINVER >= 0x0A00
        auto& app = Lumos::Application::Get();
        HWND hwnd = glfwGetWin32Window((GLFWwindow*)static_cast<GLFWwindow*>(app.GetWindow()->GetHandle()));

        COLORREF col = RGB(colour.x * 255, colour.y * 255, colour.z * 255);

        COLORREF CAPTION_COLOR = col;
        COLORREF BORDER_COLOR  = 0x201e1e;

        DwmSetWindowAttribute(hwnd, 34 /*DWMWINDOWATTRIBUTE::DWMWA_BORDER_COLOR*/, &BORDER_COLOR, sizeof(BORDER_COLOR));
        DwmSetWindowAttribute(hwnd, 35 /*DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR*/, &CAPTION_COLOR, sizeof(CAPTION_COLOR));
        SetWindowPos(hwnd, NULL, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE);
#endif
    }
}
