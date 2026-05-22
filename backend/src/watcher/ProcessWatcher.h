/////////////////////////////////////////////////////////
// File: ProcessWatcher.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares ProcessWatcher class which
//              monitors selected targets activity
//              for playtime and scan triggers
/////////////////////////////////////////////////////////

// NOTE: THIS DOES NOT TRACK ACTUAL ACHIVEMENT CHANGES
// THAT NEEDS TO BE IMPLEMENTED ELSEWHERE
// LINUX MAY USE inotify WHEN WE HAVE RECEIVED CORRECT FILES TO TRACK

#pragma once

class ProcessWatcher
{
public:
    ProcessWatcher();
    ~ProcessWatcher();

private:

};
