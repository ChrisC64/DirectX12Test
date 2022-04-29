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
    Application::App app(800, 600, L"DX 12 Test");

    app.Run();
}