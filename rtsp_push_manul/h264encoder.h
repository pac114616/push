#ifndef H264ENCODER_H
#define H264ENCODER_H
#include "mediabase.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/imgutils.h>

}
class H264Encoder
{
public:
    H264Encoder();
    ~H264Encoder();
    virtual int Init(const Properties &properties);
    AVCodecContext *getCtx();
    RET_CODE Encode(uint8_t *yuv,int size,int64_t pts);
    inline uint8_t *get_sps_data(){
        return (uint8_t *)_sps.c_str();
    }
    inline int get_sps_size()
    {
        return _sps.size();
    }
    inline uint8_t *get_pps_data()
    {
        return (uint8_t *)_pps.c_str();
    }
    inline int get_pps_size()
    {
        return _pps.size();
    }
    inline int get_fps_size()
    {
        return _fps;
    }
    AVCodecContext *getCodecContext()
    {
        return _ctx;
    }
private:
    AVCodec *_codec = NULL;
    AVCodecContext *_ctx = NULL;
    AVDictionary *_dict = NULL;
    std::string _sps;
    std::string _pps;
    std::string _codec_name;
    int _fps = 0;
    int b_frames = 0; //连续B帧数量，有B帧就会有延迟
    int _biterate = 0;
    int _gop = 0;
    int _pix_fmt = 0;
    bool _annexb = false;
    int _threads = 1;
    std::string _profile;
    std::string _level_id;

    AVFrame *_frame = NULL;
    int _width;
    int _height;

};

#endif // H264ENCODER_H
