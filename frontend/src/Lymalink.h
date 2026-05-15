/////////////////////////////////////////////////////////
// File: Lymalink.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Lymalink backend orchestrator 
/////////////////////////////////////////////////////////

#pragma once

#include <QObject>

class Lymalink : public QObject
{
    Q_OBJECT

public:
    explicit Lymalink(QObject *parent = nullptr);
    ~Lymalink();

signals:
    
private:
    
};
