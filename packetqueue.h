#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H
#include <mutex>
#include <condition_variable>
#include <queue>
#include "mediabase.h"
#include "dlog.h"
extern "C"{
#include "libavcodec/avcodec.h"
}
typedef struct packet_queue_stats
{
    int audio_nb_packets; //音频包数量
    int video_nb_packets; //视频包数量
    int audio_size;       //音频总大小
    int video_size;       //视频总大小
    int64_t audio_duration;//音频持续时长
    int64_t video_duration;//视频持续时长
}packet_queue_stats;
typedef struct MyAVPacket
{
    AVPacket *pkt;
    Media_type media_type;

}MyAVPacket;
class PacketQueue
{
public:
    PacketQueue(double audio_frame_duration,double video_frame_duration):
        _audio_frame_duration(audio_frame_duration),
        _video_frame_duration(video_frame_duration)
    {
        if(_audio_frame_duration<0)
            _audio_frame_duration = 0;
        if(_video_frame_duration<0)
            _video_frame_duration = 0;
        memset(&_stats,0,sizeof(packet_queue_stats));
    }
    ~PacketQueue()
    {

    }
    //插入packet需要指明音视频类型 返回0是正常的
    int Push(AVPacket *pkt,Media_type media_type)
    {
        if(!pkt)
        {
            LogError("pkt is null");
            return -1;
        }
        if(media_type == E__MEDIA_UNKNOWN)
        {
            LogError("media_type is unknown");
            return -1;
        }
        std::lock_guard<std::mutex>lock(_mutex);
        int ret = pushPrivate(pkt,media_type);
        if(ret<0)
        {
            LogError("push private failed");
            return -1;
        }else
        {
            _cond.notify_one(); //通知等待获取数据
            return 0;
        }
    }
    int pushPrivate(AVPacket *pkt,Media_type media_type)
    {
        if(_abort_request)
        {
            LogWarn("abort request");
            return -1;
        }
        MyAVPacket *myPkt = (MyAVPacket *)malloc(sizeof(MyAVPacket));
        if(!myPkt)
        {
            LogError("malloc MyAvPacket failed");
            return -1;
        }
        myPkt->pkt = pkt;
        myPkt->media_type = media_type;
        if(E_AUDIO_TYPE == myPkt->media_type)
        {
            _stats.audio_nb_packets++;  //包数量
            _stats.audio_size +=pkt->size;
            //持续时长统计，不能用pkt->duration
            _audio_back_pts = pkt->pts;
            if(_audio_first_packet)
            {
                _audio_front_pts = pkt->pts;
                _audio_first_packet = 0;
            }
        }
        if(E_VIDEO_TYPE == myPkt->media_type)
        {
            _stats.video_nb_packets++;
            _stats.video_size += pkt->size;
            _video_back_pts = pkt->pts;
            //
            if(_video_first_packet)
            {
                _video_front_pts = pkt->pts;
                _video_first_packet = 0;
            }
        }
       _queue.push(myPkt); //一定要Push
       return 0;
    }
    //取出packet,并也取出对应的类型
    //返回值：-1 abort, 1 获取到消息
    int Pop(AVPacket **pkt,Media_type &media_type)
    {
        if(!pkt)
        {
            LogError("when pop pkt is null");
            return -1;
        }
        std::unique_lock<std::mutex> lock(_mutex);
        if(_abort_request)
        {
            LogWarn("abort request");
            return -1;
        }
        if(_queue.empty()) //等待
        {
            _cond.wait(lock,[this] //闭包函数语法
            {
                //如果返回false,继续wait，如果返回true退出wait
                return !_queue.empty() | _abort_request;
            });
        }
        if(_abort_request)
        {
            LogWarn("abort request");
            return -1;
        }
        //真正活动
        MyAVPacket *mypkt = _queue.front(); //数据还没有出队列
        *pkt = mypkt->pkt;
        media_type = mypkt->media_type;
        if(E_AUDIO_TYPE == media_type)
        {
            _stats.audio_nb_packets--;
            _stats.audio_size -= mypkt->pkt->size;
            _audio_front_pts = mypkt->pkt->pts;
        }
        if(E_VIDEO_TYPE == media_type)
        {
            _stats.video_nb_packets--;
            _stats.video_size -= mypkt->pkt->size;
            _video_front_pts = mypkt->pkt->pts; //尝试
        }
        _queue.pop();
        free(mypkt);
        return 1;
    }
    //带超时时间
    //返回值:-1 abort,0 没有消息 ，1有消息
    int PopWithTimeout(AVPacket **pkt,Media_type &media_type,int timeout)
    {
        if(timeout < 0 )
            return Pop(pkt,media_type);
        std::unique_lock<std::mutex> lock(_mutex);
        if(_abort_request)
        {
            LogWarn("abort request");
            return -1;
        }
        if(_queue.empty()) //等待
        {
            _cond.wait_for(lock,std::chrono::milliseconds(timeout),[this] //闭包函数语法
            {
                //如果返回false,继续wait，如果返回true退出wait
                return !_queue.empty() | _abort_request;
//                return true; 不能用return true
            });
        }
        if(_abort_request)
        {
            LogWarn("abort request");
            return -1;
        }
        if(_queue.empty())
        {
            return -1;
        }
        //真正活动
        MyAVPacket *mypkt = _queue.front(); //数据还没有出队列
        *pkt = mypkt->pkt;
        media_type = mypkt->media_type;
        if(E_AUDIO_TYPE == media_type)
        {
            _stats.audio_nb_packets--;
            _stats.audio_size -= mypkt->pkt->size;
            _audio_front_pts = mypkt->pkt->pts;
        }
        if(E_VIDEO_TYPE == media_type)
        {
            _stats.video_nb_packets--;
            _stats.video_size -= mypkt->pkt->size;
            _video_front_pts = mypkt->pkt->pts; //尝试
        }
        _queue.pop();
        free(mypkt);
        return 1;
    }
    bool Empty()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }
    //唤醒在等待的线程
    void Abort()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _abort_request = true;
        _cond.notify_all(); //唤醒所有
    }
    //all 为true则清空队列;
    //all为false:drop数据直到遇到I帧，最大保留remain_max_duration
    int Drop(bool all,int64_t remain_max_duration)
    {
        std::lock_guard<std::mutex>lock(_mutex);
        while(!_queue.empty())
        {
            MyAVPacket *mypkt = _queue.front();
            Media_type media_type = mypkt->media_type;
            //判断是不是I帧
            if(!all && mypkt->media_type == E_VIDEO_TYPE && (mypkt->pkt->flags &AV_PKT_FLAG_KEY))
            {
                int64_t duration = _video_back_pts - _video_front_pts;
                //也参考帧持续，*帧数   *2是经验 <0 pts回绕
                if(duration<0 || duration>_video_frame_duration*_stats.video_nb_packets*2)
                {
                    duration = _video_frame_duration*_stats.video_nb_packets;
                }
                LogInfo("video duration:%lld",duration);
                if(duration <= remain_max_duration)
                    break;//说明可以break
            }
            if(E_AUDIO_TYPE == media_type)
            {
                _stats.audio_nb_packets--;
                _stats.audio_size -= mypkt->pkt->size;
                _audio_front_pts = mypkt->pkt->pts;
            }
            if(E_VIDEO_TYPE == media_type)
            {
                _stats.video_nb_packets--;
                _stats.video_size -= mypkt->pkt->size;
                _video_front_pts = mypkt->pkt->pts; //尝试
            }
            av_packet_free(&mypkt->pkt);  //先释放AVPacket
            _queue.pop();
            free(mypkt);                  //再释放mypkt
        }
        return 0;
    }

    //获取音频持续时间
    int64_t getAudioDuration()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        int64_t duration = _audio_back_pts - _audio_front_pts;
        //也参考帧持续，*帧数   *2是经验 <0 pts回绕
        if(duration<0 || duration>_audio_frame_duration*_stats.audio_nb_packets*2)
        {
            duration = _audio_frame_duration*_stats.audio_nb_packets;
        }else
        {
            duration += _audio_frame_duration;
        }
        return duration;

    }

    //获取视频持续时间
    int64_t getVideoDuration()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        int64_t duration = _video_back_pts - _video_front_pts;
        //也参考帧持续，*帧数   *2是经验 <0 pts回绕
        if(duration<0 || duration>_video_frame_duration*_stats.video_nb_packets*2)
        {
            duration = _video_frame_duration*_stats.video_nb_packets;
        }else
        {
            duration += _video_frame_duration;
        }
        return duration;
    }
    //获取音频包持续时间
    int getAudioPackets()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _stats.audio_nb_packets;
    }
    //获取视频包持续时间
    int64_t getVideoPackets()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _stats.video_nb_packets;
    }
    void getStats(packet_queue_stats *stats)
    {
        if(!stats)
        {
            LogError("status error");
            return;
        }
        std::lock_guard<std::mutex> lock(_mutex);
        int64_t audio_duration = _audio_back_pts - _audio_front_pts;
        if(audio_duration<0 || audio_duration>_audio_frame_duration*_stats.audio_nb_packets*2)
        {
            audio_duration = _audio_frame_duration*_stats.audio_nb_packets;
        }else
        {
            audio_duration += _audio_frame_duration;
        }
        int64_t video_duration = _video_back_pts - _video_front_pts;
        //也参考帧持续，*帧数   *2是经验 <0 pts回绕
        if(video_duration<0 || video_duration>_video_frame_duration*_stats.video_nb_packets*2)
        {
            video_duration = _video_frame_duration*_stats.video_nb_packets;
        }else
        {
            video_duration += _video_frame_duration; //叠加一帧，第一帧差值为0
        }
        stats->audio_duration = audio_duration;
        stats->video_duration = video_duration;
        stats->audio_nb_packets = _stats.audio_nb_packets;
        stats->video_nb_packets = _stats.video_nb_packets;
        stats->audio_size = _stats.audio_size;
        stats->video_size = _stats.video_size;
    }
private:
    std::mutex _mutex;
    std::condition_variable _cond;
    std::queue<MyAVPacket *>_queue;
    packet_queue_stats _stats;
    bool _abort_request = false;
    double _audio_frame_duration = 23.21995649;
    double _video_frame_duration = 40; //40ms 视频帧率为25 1000ms/25
    //pts记录
    int64_t _audio_front_pts = 0;
    int64_t _audio_back_pts = 0;
    int _audio_first_packet = 1;
    int _video_first_packet = 1;
    int64_t _video_front_pts = 0;
    int64_t _video_back_pts = 0;
};

#endif // PACKETQUEUE_H
