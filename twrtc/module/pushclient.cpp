#include "pushclient.h"

#include "api/audio_options.h"
#include "media/engine/webrtc_media_engine.h"
#include "media/engine/webrtc_video_engine.h"
#include "media/engine/webrtc_voice_engine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture_factory.h"
#include "api/rtp_transceiver_direction.h"
#include "api/jsep.h"
#include "rtc_base/ssl_adapter.h"
#include "libyuv.h"
#include "api/video/i420_buffer.h"
#include "api/enable_media.h"
#include "api/environment/environment.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "rtc_base/time_utils.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/stats/rtcstats_objects.h"
#include "api/peer_connection_interface.h"

// 简化版 Observer 实现：CreateSessionDescriptionObserver/SetSessionDescriptionObserver
namespace webrtc
{
    class CreateSessionDescriptionObserverq : public webrtc::CreateSessionDescriptionObserver
    {
    public:
        using OnSuccessFn = std::function<void(SessionDescriptionInterface *)>;
        using OnFailureFn = std::function<void(RTCError)>;
        CreateSessionDescriptionObserverq(OnSuccessFn ok, OnFailureFn fail)
            : ok_(std::move(ok)), fail_(std::move(fail)) {}
        void OnSuccess(SessionDescriptionInterface *desc) override { ok_(desc); }
        void OnFailure(RTCError error) override { fail_(error); }

    private:
        OnSuccessFn ok_;
        OnFailureFn fail_;
    };

    class SetSessionDescriptionObserverq : public webrtc::SetSessionDescriptionObserver
    {
    public:
        void OnSuccess() override { RTC_LOG(LS_INFO) << "SetDescription OK"; }
        void OnFailure(RTCError error) override { RTC_LOG(LS_ERROR) << "SetDescription failed: " << error.message(); }
    };
} // namespace webrtc

webrtc::scoped_refptr<CapturerTrackSource> CapturerTrackSource::Create(int target_fps, bool capture_cursor)
{
    return nullptr;
    // auto src = webrtc::make_ref_counted<CapturerTrackSource>();
    // auto src = webrtc::scoped_refptr<CapturerTrackSource>(new CapturerTrackSource());
    // // 这里用 ScreenCapturer；如果要窗口捕获，改成 CreateWindowCapturer 并传 window id
    // webrtc::DesktopCaptureOptions options = webrtc::DesktopCaptureOptions::CreateDefault();

    // src->capturer_ = (webrtc::DesktopCapturer::CreateScreenCapturer(options));
    // if (!src->capturer_)
    // {
    //     RTC_LOG(LS_ERROR) << "Failed to create screen capturer";
    //     return nullptr;
    // }

    // auto list = webrtc::DesktopCapturer::SourceList{};
    // src->capturer_->GetSourceList(&list);
    // if (!list.empty())
    // {
    //     src->capturer_->SelectSource(list[0].id);
    // }

    // return src;
}

CapturerTrackSource::CapturerTrackSource()
    : webrtc::VideoTrackSource(/*remote*/ false), running_(false)
{
}

void CapturerTrackSource::Start()
{
    running_ = true;
    cap_thread_ = std::thread([this]()
                              { StartCaptureLoop(25, true); });
}

void CapturerTrackSource::StartCaptureLoop(int target_fps, bool capture_cursor)
{
    class Callback : public webrtc::DesktopCapturer::Callback
    {
    public:
        explicit Callback(CapturerTrackSource *src) : src_(src) {}
        void OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame) override
        {
            if (result != webrtc::DesktopCapturer::Result::SUCCESS || !frame)
                return;
            // 将 DesktopFrame 转为 I420 VideoFrame
            int width = frame->size().width();
            int height = frame->size().height();

            // DesktopFrame 为 BGRA，简单起见用 libyuv 做转换（libwebrtc 已内置）
            webrtc::scoped_refptr<webrtc::I420Buffer> i420 = webrtc::I420Buffer::Create(width, height);
            const uint8_t *src_bgra = frame->data();
            int src_stride_bgra = frame->stride();

            // BGRA -> I420
            libyuv::ARGBToI420(src_bgra, src_stride_bgra,
                               i420->MutableDataY(), i420->StrideY(),
                               i420->MutableDataU(), i420->StrideU(),
                               i420->MutableDataV(), i420->StrideV(),
                               width, height);

            webrtc::VideoFrame vf = webrtc::VideoFrame::Builder()
                                        .set_video_frame_buffer(i420)
                                        .set_timestamp_us(rtc::TimeMicros())
                                        .build();

            // src_->OnFrame(vf);
            src_->OnCapturedFrame(vf);
        }

    private:
        CapturerTrackSource *src_;
    };

    Callback cb(this);
    capturer_->Start(&cb);

    const int interval_ms = 1000 / std::max(1, target_fps);
    while (running_)
    {
        capturer_->CaptureFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

WebRTCPushClient::WebRTCPushClient(std::string id)
    : id{id}
{
    if (!signaling_thread_.get())
    {
        signaling_thread_ = rtc::Thread::CreateWithSocketServer();
        signaling_thread_->Start();
    }
}

WebRTCPushClient::~WebRTCPushClient()
{
    pc_ = nullptr;
    factory_ = nullptr;
    signaling_thread_->Stop();
    signaling_thread_ = nullptr;
    worker_thread_->Stop();
    worker_thread_ = nullptr;
    network_thread_->Stop();
    network_thread_ = nullptr;
}

bool WebRTCPushClient::Init(const std::vector<IceServerConfig> &ice_servers)
{
    webrtc::PeerConnectionFactoryDependencies deps;
    deps.signaling_thread = signaling_thread_.get();
    // deps.env = env_,
    deps.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    deps.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
    deps.video_encoder_factory =
        std::make_unique<webrtc::VideoEncoderFactoryTemplate<
            webrtc::LibvpxVp8EncoderTemplateAdapter,
            webrtc::LibvpxVp9EncoderTemplateAdapter,
            webrtc::OpenH264EncoderTemplateAdapter,
            webrtc::LibaomAv1EncoderTemplateAdapter>>();
    deps.video_decoder_factory =
        std::make_unique<webrtc::VideoDecoderFactoryTemplate<
            webrtc::LibvpxVp8DecoderTemplateAdapter,
            webrtc::LibvpxVp9DecoderTemplateAdapter,
            webrtc::OpenH264DecoderTemplateAdapter,
            webrtc::Dav1dDecoderTemplateAdapter>>();
    webrtc::EnableMedia(deps);
    factory_ =
        webrtc::CreateModularPeerConnectionFactory(std::move(deps));

    // 获取工厂音视频编解码能力
    webrtc::RtpCapabilities capabilities = factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_AUDIO);

    RTC_LOG(LS_INFO) << "=== Supported Video Sender Codecs ===";
    for (const auto &codec : capabilities.codecs)
    {
        RTC_LOG(LS_INFO) << "Codec: " << codec.name
                         << " | MIME: " << codec.mime_type()
                         << " | ClockRate: " << codec.clock_rate.value_or(0);
    }

    // webrtc::PeerConnectionInterface::RTCConfiguration config;
    // config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    // webrtc::PeerConnectionInterface::IceServer server;
    // server.uri = ice_servers[0].uri;
    // config.servers.push_back(server);

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "stun:stun.l.google.com:19302";
    config.servers.push_back(server);

    webrtc::PeerConnectionDependencies pc_dependencies(this);
    auto error_or_peer_connection =
        factory_->CreatePeerConnectionOrError(
            config, std::move(pc_dependencies));
    if (error_or_peer_connection.ok())
    {
        pc_ = std::move(error_or_peer_connection.value());
    }

    return true;
}

bool WebRTCPushClient::AddDesktopVideo(int fps, int max_bitrate_bps)
{
    auto source = CapturerTrackSource::Create(fps);
    if (!source)
    {
        printf("Failed to create DesktopCapturerSource\n");
        return false;
    }

    video_track_ = factory_->CreateVideoTrack(source, "desktop");
    if (!video_track_)
    {
        printf("Failed to create VideoTrack\n");
        return false;
    }

    webrtc::RtpTransceiverInit init;
    init.direction = webrtc::RtpTransceiverDirection::kSendOnly;
    auto transceiver_or = pc_->AddTransceiver(video_track_, init);
    if (!transceiver_or.ok())
    {
        RTC_LOG(LS_ERROR) << "AddTransceiver failed: " << transceiver_or.error().message();
        printf("AddTransceiver failed\n");
        return false;
    }
    auto transceiver = transceiver_or.value();
    video_sender_ = transceiver->sender();

    // 设置码率上限
    if (max_bitrate_bps > 0)
    {
        webrtc::RtpParameters params = video_sender_->GetParameters();
        if (!params.encodings.empty())
        {
            params.encodings[0].max_bitrate_bps = max_bitrate_bps;
            video_sender_->SetParameters(params);
        }
    }

    source->Start();
    return true;
}

bool WebRTCPushClient::SetRemoteAnswer(const std::string &sdp_answer)
{
    return true;
}

bool WebRTCPushClient::AddRemoteIce(const std::string &candidate_sdp, int sdp_mline_index, const std::string &sdp_mid)
{
    webrtc::SdpParseError err;
    std::unique_ptr<webrtc::IceCandidateInterface> cand(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate_sdp, &err));
    if (!cand)
    {
        RTC_LOG(LS_ERROR) << "Parse ICE failed: " << err.description;
        return false;
    }
    bool ok = pc_->AddIceCandidate(cand.get());
    RTC_LOG(LS_INFO) << "AddRemoteIce: " << ok;
    return ok;
}

bool WebRTCPushClient::SetMaxBitrate(int bps)
{
    if (!video_sender_)
        return false;
    auto params = video_sender_->GetParameters();
    if (params.encodings.empty())
        params.encodings.push_back(webrtc::RtpEncodingParameters());
    params.encodings[0].max_bitrate_bps = bps;
    return video_sender_->SetParameters(params).ok();
}

void WebRTCPushClient::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> &streams)
{
}

void WebRTCPushClient::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
}

void WebRTCPushClient::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
}

void PeerObserver::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state)
{
    RTC_LOG(LS_INFO) << "PeerConnection state: " << new_state;
    if (!owner_)
        return;
    if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected)
    {
    }
    else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected ||
             new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kFailed ||
             new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kClosed)
    {
    }
}
