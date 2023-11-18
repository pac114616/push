#ifndef RTSPPUSH_H
#define RTSPPUSH_H
#include "mediabase.h"
#include "packetqueue.h"
//#include <iostream>
#include "commonlooper.h"
#include "messagequeue.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/imgutils.h>
}
class RtspPush:public CommonLooper
{
public:
    RtspPush(MessageQueue *msg_queue);
    virtual ~RtspPush();
    RET_CODE Init(const Properties &properties);
    void DeInit();
    RET_CODE pushData(AVPacket *pkt,Media_type media_type);
    //连接服务器，如果连接成功启动线程
    RET_CODE pushConnect();
    //如果有视频成分
    RET_CODE configVideoStream(const AVCodecContext *ctx);
    //如果有音频成分
    RET_CODE configAudioStream(const AVCodecContext *ctx);
    virtual void Loop();
    bool isTimeout();
    void restTimeout();
    int getTimeout();
    int64_t getBlockTime();
private:
    int64_t _pre_debug_time = 0;
    int64_t _debug_interval = 2000;
    void debugQueue(int64_t interval);     //按时间间隔打印packetqueue的状况
    //检测队列缓存情况
    void checkPacketQueueDuration();
    int sendPacket(AVPacket *pkt,Media_type mediaType);
    //整个输出流的上下文
    AVFormatContext *_fmt_ctx = NULL;
    //视频编码器上下文
    AVCodecContext *_video_ctx = NULL;
    //音频编码器上下文
    AVCodecContext *_audio_ctx = NULL;

    //输出视频流
    AVStream *_video_stream = NULL;
    int _video_index = -1;
    //输出音频流
    AVStream *_audio_stream = NULL;
    int _audio_index = -1;
    std::string _url = "";
    std::string _rtsp_transport = "";
    PacketQueue *_queue = NULL;
    double _audio_frame_duration = 23.21995649;
    double _video_frame_duration = 40; //40ms 视频帧率为25 1000ms/25
    int _request_abort = 0;
    //队列最大限制时长
    int _max_queue_duration = 500;//默认100ms
    //处理超时
    int64_t _pre_time = 0; //进入ffmpeg api之前的时间
    int _timeout;
    MessageQueue *_msg_queue = NULL;

};
/*
 * 创建一个RTSP Push
 * init udp tcp 超时时间设置，队列最大时长设置,创建输出流上下文
 * 有视频：ConfigVideoStream 创建视频流 avformat_new_stream 有音频ConfigAudioStream 创建音频流 avformat_new_stream
 * 调用Connect 连接服务器(av_format_write_header)、创建启动线程
 * 真正往服务器发送音频包在Loop中实现：取音视频包 push放音视频包
 *
 * 补充超时处理问题
 */
#endif // RTSPPUSH_H
