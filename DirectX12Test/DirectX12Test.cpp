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
    Application::App app(1280, 720, L"DX 12 Test");

    app.Run();
}