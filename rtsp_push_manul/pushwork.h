#ifndef PUSHWORK_H
#define PUSHWORK_H
#include "audiocapture.h"
#include "videocapturer.h"
#include "aacencoder.h"
#include "h264encoder.h"
#include "rtsppush.h"
#include "messagequeue.h"
#include "audioresample.h"
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}
class PushWork
{
public:
    PushWork(MessageQueue *msg_queue);
    ~PushWork();
    RET_CODE Init(const Properties &properties);
    void PcmCallback(uint8_t *pcm,int32_t size);
    void YUVCallback(uint8_t *pcm,int32_t size);
    void getAndPutAudioAVPacket();
    void getAndPutVideoAVPacket();
    void DeInit();
private:
    AudioCapture *_audio_capturer = NULL;
    int _audio_test = 0;
    std::string _input_pcm_name;
    uint8_t *_fltp_buf = NULL;
    // 麦克风采样属性
    int mic_sample_rate_ = 48000;
    int mic_sample_fmt_ = AV_SAMPLE_FMT_S16;
    int mic_channels_ = 2;

    AACEncoder *_audio_encoder = NULL;
    // 音频编码参数
    int _audio_sample_rate = 48000;
    int _audio_bitrate = 128*1024;
    int _audio_channels = 2;
    int _audio_sample_fmt ; // 具体由编码器决定，从编码器读取相应的信息
    int _audio_ch_layout;    // 由audio_channels_决定

    //视频test模式
    int video_test;
    std::string _input_yuv_name;
    //桌面录制属性
    VIdeoCapturer *_video_capturer = NULL;
    H264Encoder *_video_encoder = NULL;
    int _desktop_x = 0;
    int _desktop_y = 0;
    int _desktop_width = 1920;
    int _desktop_height = 1080;
    int _desktop_fps = 25;
    int _video_width = 1920;
    int _video_height =  1080;
    int _video_fps = 25;
    int _video_gop;
    int _video_b_frames;
    int _video_bitrate;

    //dump数据
    FILE *_pcm_s16le_fp = NULL;
    FILE *_aac_fp = NULL;
    FILE *_h264_fp = NULL;
    AVFrame *_audio_frame = NULL;
    AVFrame *_video_frame = NULL;
    int _fltp_buf_size;

    //重采样
    AudioResample *_audio_resample = NULL;
    AVCodecContext *_audio_ctx = NULL;
    AVCodecContext *_video_ctx = NULL;
    //rtsp
    std::string _rtsp_url;
    RtspPush *_rtsp_push = NULL;
    std::string _rtsp_transport = "";
    int _rtsp_timeout = 5000;
    int _rtsp_max_queue_duration = 500;
    MessageQueue *_msg_queue = NULL;
};

#endif // PUSHWORK_H
