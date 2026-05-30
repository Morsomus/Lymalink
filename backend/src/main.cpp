/////////////////////////////////////////////////////////
// File: main.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Lymalinkd Service entry
/////////////////////////////////////////////////////////

#include "Lymalinkd.h"
#include "tools/Logger.h"

/////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Temporary log while dev
    Logger::Instance().Init();

    Lymalinkd lymalinkd;
    Error err = lymalinkd.Main();

    return err == Error::NoError ? 0 : 1;
}
