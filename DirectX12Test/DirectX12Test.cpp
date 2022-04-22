#include <iostream>
#include <thread>
#include <mutex>
#include <filesystem>

import Window;
import UI;
import Application;
import DX12Device;

int main()
{
    Application::LSWindow window;
    window.initWIndow(800, 600, L"DX 12 Test");
    Application::App app(std::move(window));

    app.Run();
}