#pragma once
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>

#include "api/peer_connection_interface.h"
#include "api/create_peerconnection_factory.h"
#include "api/video/video_frame.h"
#include "api/video_track_source_proxy_factory.h"
#include "api/video_track_source_constraints.h"
#include "api/media_stream_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_parameters.h"
#include "pc/session_description.h"
#include "pc/video_track_source.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/logging.h"
#include "media/base/adapted_video_track_source.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "api/peer_connection_interface.h"
#include "absl/types/optional.h"
#include "media/base/video_broadcaster.h"
// getStats
#include "api/stats/rtc_stats_report.h"
// 如果需要窗口捕获：#include "modules/desktop_capture/window_capturer.h"
// 如果需要窗口捕获：#include "modules/desktop_capture/window_capturer.h"

struct IceServerConfig
{
    std::string uri;      // e.g. "stun:stun.l.google.com:19302" or "turn:your.turn.server:3478?transport=tcp"
    std::string username; // TURN 用户名
    std::string password; // TURN 密码
};

struct SdpBundle
{
    std::string type; // "offer" 或 "answer"
    std::string sdp;
};

class SimpleSignaling
{
public:
    // 你可以把这三个回调接到你的 WebSocket/HTTP 信令
    std::function<void(const SdpBundle &, std::string id)> onLocalSdp;
    std::function<void(const std::string &)> onLocalIce;
};

class CapturerTrackSource : public webrtc::VideoTrackSource
{
public:
    static webrtc::scoped_refptr<CapturerTrackSource> Create(int target_fps = 30, bool capture_cursor = true);

    ~CapturerTrackSource() override
    {
        running_ = false;
        if (cap_thread_.joinable())
            cap_thread_.join();
    }

public:
    CapturerTrackSource();

    void OnCapturedFrame(const webrtc::VideoFrame &frame)
    {
        broadcaster_.OnFrame(frame);
    }

    void Start();

protected:
    // VideoTrackSource 接口
    webrtc::MediaSourceInterface::SourceState state() const override
    {
        return webrtc::MediaSourceInterface::SourceState::kLive;
    }
    bool remote() const override { return false; }

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                         const rtc::VideoSinkWants &wants) override
    {
        broadcaster_.AddOrUpdateSink(sink, wants);
    }

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
    {
        broadcaster_.RemoveSink(sink);
    }

private:
    void StartCaptureLoop(int target_fps, bool capture_cursor);

    std::unique_ptr<webrtc::DesktopCapturer> capturer_;
    std::atomic<bool> running_;
    std::thread cap_thread_;
    rtc::VideoBroadcaster broadcaster_;

    int m_iTargetFps{25};

    // 实现 VideoTrackSource 的纯虚函数 source()
public:
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override
    {
        return nullptr; // 如有需要可返回实际 VideoSourceInterface
    }
};

class WebRTCPushClient;

class PeerObserver : public webrtc::PeerConnectionObserver
{
public:
    PeerObserver(SimpleSignaling *sig, WebRTCPushClient *owner) : signaling_(sig), owner_(owner) {}
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override
    {
        RTC_LOG(LS_INFO) << "Signaling state: " << new_state;
    }
    void OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;

    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override
    {
        RTC_LOG(LS_INFO) << "ICE gathering: " << new_state;
    }
    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override
    {
        std::string s;
        candidate->ToString(&s);
        if (signaling_ && signaling_->onLocalIce)
            signaling_->onLocalIce(s);
        RTC_LOG(LS_INFO) << "Local ICE: " << s;
    }
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override
    {
        RTC_LOG(LS_INFO) << "ICE connection: " << new_state;
    }

    void OnDataChannel(
        webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}

private:
    SimpleSignaling *signaling_;
    WebRTCPushClient *owner_;
};

class WebRTCPushClient : public webrtc::PeerConnectionObserver
{
public:
    WebRTCPushClient(std::string id);
    ~WebRTCPushClient();
    std::string getId() const { return id; }
    // 初始化 PeerConnectionFactory 与 PeerConnection
    bool Init(const std::vector<IceServerConfig> &ice_servers);

    // 添加桌面捕获视频轨并设置编码参数
    bool AddDesktopVideo(int fps = 30, int max_bitrate_bps = 3'000'000);
    // 生成并发送 Offer（通过 SimpleSignaling 回调打印）
    bool CreateAndSendOffer(bool ice_restart = false);

    // 注入远端 Answer（从你的信令拿到字符串）
    bool SetRemoteAnswer(const std::string &sdp_answer);

    // 注入远端 ICE 候选（字符串形式）
    bool AddRemoteIce(const std::string &candidate_sdp, int sdp_mline_index = 0, const std::string &sdp_mid = "video");

    // 调整码率（在连接后可动态调用）
    bool SetMaxBitrate(int bps);

    SimpleSignaling signaling;
 protected:
    //
    // PeerConnectionObserver implementation.
    //

    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override {}
    void OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> &
            streams) override;
    void OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
    void OnRenegotiationNeeded() override {}
    void OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
    void OnIceConnectionReceivingChange(bool receiving) override {}

private:
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
    rtc::scoped_refptr<webrtc::RtpSenderInterface> video_sender_;

    std::unique_ptr<PeerObserver> observer_;

    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;
    std::string id{""};
};