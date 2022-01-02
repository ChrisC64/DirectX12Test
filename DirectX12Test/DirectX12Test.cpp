#include <iostream>
#include <thread>
#include <mutex>
#include <filesystem>

import Window;
import UI;
std::vector<std::thread> ThreadPool;
std::mutex threadMu;
void doWork()
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::lock_guard<std::mutex> lock(threadMu);
    std::cout << "This thread " << std::this_thread::get_id() << " has been sleeping for 3 seconds, now we will leave!\n";
}

int main()
{
    Application::LSWindow window;
    window.initWIndow(800, 600, L"DX 12 Test");

    auto text = UI::LSText(L"Hello LS Text!", window.getWriteFactory());
    text.m_id = 0;
    auto text2 = UI::LSText(L"Another Textbox appeared!!", window.getWriteFactory(), {100, 100, 200, 200});
    text2.m_id = 1;
    window.addText(text);
    window.addText(text2);

    window.run();
}