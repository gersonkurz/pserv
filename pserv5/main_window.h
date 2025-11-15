#pragma once
#include <Windows.h>

namespace pserv {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Initialize(HINSTANCE hInstance);
    void Show(int nCmdShow);
    int MessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HWND m_hWnd{nullptr};
    HINSTANCE m_hInstance{nullptr};
};

} // namespace pserv
