#pragma once

#include "module/demo/conductor.h"
#include "module/demo/peer_connection_client.h"

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
    webrtc::scoped_refptr<Conductor> conductor_;
    PeerConnectionClient peer_connection_client_;
};

