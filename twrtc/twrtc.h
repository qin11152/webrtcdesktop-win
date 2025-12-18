#pragma once

#include <QtWidgets/QWidget>
#include "ui_twrtc.h"

class twrtc : public QWidget
{
    Q_OBJECT

public:
    twrtc(QWidget *parent = nullptr);
    ~twrtc();

private:
    Ui::twrtcClass ui;
};

