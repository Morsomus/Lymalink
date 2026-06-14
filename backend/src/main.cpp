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

/////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    Logger::Instance().Init();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    Lymalinkd lymalinkd;
    Error err = lymalinkd.Main();

    return err == Error::NoError ? 0 : 1;
}
