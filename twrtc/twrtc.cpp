#include "twrtc.h"

twrtc::twrtc(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    conductor_ = rtc::make_ref_counted<Conductor>(&peer_connection_client_);
}

twrtc::~twrtc()
{}

