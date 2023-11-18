#ifndef VIDEOCAPTURER_H
#define VIDEOCAPTURER_H
#include <functional>
#include "mediabase.h"
#include <iostream>
#include "timesutil.h"
#include "commonlooper.h"
#include "avpublishtime.h"
class VIdeoCapturer:public CommonLooper
{
public:
    VIdeoCapturer();
    virtual ~VIdeoCapturer();
    RET_CODE Init(const Properties properties);

    virtual void Loop();
    void AddCallback(std::function<void(uint8_t *,int32_t)> callback);
private:
    int _video_test = 0;
    std::string _input_yuv_name;
    int _x;
    int _y;
    int _width = 0;
    int _height = 0;
    int _pixel_format = 0;
    int _fps;
    double _frame_duration = 40;
    //本地文件测试
    int openYUVFile(const char *file_name);
    int readYUVFile(uint8_t *yuv_buf,int32_t yuv_buf_size);
    int closeYUVFile();
    int64_t _yuv_start_time = 0; //起始时间
    double _yuv_total_duration = 0; //pcm累计读取时间
    FILE *_yuv_fp = NULL;
    int _yuv_buf_size = 0;
    uint8_t *_yuv_buf;
    function<void(uint8_t*,int32_t)>callback_object = NULL;
    bool _is_first_frame = false;
};

#endif // VIDEOCAPTURER_H
