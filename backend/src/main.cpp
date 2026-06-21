/////////////////////////////////////////////////////////
// File: main.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Lymalinkd Service entry
/////////////////////////////////////////////////////////

#include "Lymalinkd.h"
#include "tools/Logger.h"

#include <chrono>
#include <thread>
#if defined(_WIN32)
    #include <QCoreApplication>
    #include <windows.h>
#endif

/////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    QCoreApplication app(argc, argv);
    HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, L"Local\\LymalinkdSession");
    if (instanceMutex == nullptr)
    {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(instanceMutex);
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif

    Logger::Instance().Init();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    Lymalinkd lymalinkd;
    Error err = lymalinkd.Main();

#if defined(_WIN32)
    CloseHandle(instanceMutex);
#endif
    return err == Error::NoError ? 0 : 1;
}
