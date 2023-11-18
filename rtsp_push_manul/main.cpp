#include <iostream>
#include "dlog.h"
#include "pushwork.h"
#include "messagequeue.h"
//
#define RTSP_URL "rtsp://192.168.1.107/live/livestream"
//ffmpeg -re -i 1920x832_25fps.flv -vcodec copy -acodec -f flv -y rtsp://192.168.1.104/live/livestream
int main()
{
    init_logger("rstp_push.log",S_INFO);
    MessageQueue *msg_queue = new MessageQueue();
{    PushWork pushwork(msg_queue);

    if(!msg_queue)
    {
        LogError("new MessageQueue failed");
        return -1;
    }
    Properties properties;
    //音频test模式
    properties.SetProperty("audio_test",1);  //音频测试模式
    properties.SetProperty("input_pcm_name","48000_2_s16le.pcm");
    //麦克风属性
    properties.SetProperty("mic_sample_fmt",AV_SAMPLE_FMT_S16);
    properties.SetProperty("mic_sample_rate",48000);
    properties.SetProperty("mic_channels",2);

    //视频test模式
    properties.SetProperty("video_test",1);
    properties.SetProperty("input_yuv_name","720x480_25fps_420p.yuv");

    //桌面录制模式
    properties.SetProperty("desktop_x",0);
    properties.SetProperty("desktop_y",0);
    properties.SetProperty("desktop_width",720);//测试模式时和yuv文件的宽度一致
    properties.SetProperty("desktop_height",480);//测试模式时和yuv文件的高度一致
    properties.SetProperty("desktop_fps",25); //测试模式和yuv文件的帧率一致

    //视频编码属性
    properties.SetProperty("video_bitrate",512*1024); //设置码率

    //配置rtsp
    //1.url
    //2.udp
    properties.SetProperty("rtsp_url",RTSP_URL);
    properties.SetProperty("rtsp_transport","udp");
    properties.SetProperty("timeout",3000);
    properties.SetProperty("rtsp_max_queue_duration",500);
    if(pushwork.Init(properties)!=RET_OK)
    {
        LogError("Pushwork init failed");
        return -1;
    }
    int count=0;
    AVMessage msg;
    int ret = 0;
    while(1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        ret = msg_queue->msg_queue_get(&msg,1000);
        if(1 == ret) //读到消息
        {
            switch (msg.what) {
            case MSG_RTSP_ERROR:
                LogError("MSG_RTSP_ERROR error:%d",msg.arg1);
                break;
            case MSG_RTSP_QUEUE_DURATION:
                LogError("MSG_RTSP_QUEUE_DURATION a:%d,v:%d",msg.arg1,msg.arg2);
                break;
            default:
                break;
            }
        }
        if(count++>100)
            break;
    }
    msg_queue->msg_queue_abort();
}
    delete msg_queue;
    LogInfo("msg_queue delete");
    LogInfo("main finish");
    return 0;
}
