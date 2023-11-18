#include "videocapturer.h"
#include "dlog.h"
VIdeoCapturer::VIdeoCapturer()
{

}

VIdeoCapturer::~VIdeoCapturer()
{
    if(_yuv_buf)
        delete [] _yuv_buf;
    closeYUVFile();
}

RET_CODE VIdeoCapturer::Init(const Properties properties)
{
    _video_test = properties.GetProperty("video_test",0);
    _input_yuv_name = properties.GetProperty("input_yuc_name","720x480_25fps_420p.yuv");
    _x = properties.GetProperty("x",0);
    _y = properties.GetProperty("y",0);
    _width = properties.GetProperty("width",1920);
    _height = properties.GetProperty("height",1080);
    _pixel_format = properties.GetProperty("pixel_format",0);
    _fps = properties.GetProperty("fps",25);
    _frame_duration = 1000.0/_fps;

    if(openYUVFile(_input_yuv_name.c_str())!=0)
    {
        LogError("open YUV file %s failed",_input_yuv_name.c_str());
        return RET_FAIL;
    }
    return RET_OK;
}

void VIdeoCapturer::Loop()
{
    LogInfo("video loop!");

//    _yuv_buf_size = _width *_height *1.5;
    _yuv_buf_size = (_width+_width%2) * (_height+_height%2) * 1.5; //使宽高为偶数
    _yuv_buf = new uint8_t[_yuv_buf_size];
    _yuv_total_duration = 0;
    _yuv_start_time = TimesUtil::GetTimeMillisecond(); //获取当前时间
    LogInfo("into loop while");
    while(1)
    {
        if(request_abort)
            break;
        if(readYUVFile(_yuv_buf,_yuv_buf_size)==0)
        {
            if(!_is_first_frame)
            {
                _is_first_frame = true;
                LogInfo("%s:t%u",AVPublishTime::GetInstance()->getVInTag(),
                        AVPublishTime::GetInstance()->getCurrenTime());
            }
            if(callback_object)
            {
                callback_object(_yuv_buf,_yuv_buf_size);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    request_abort = false;
    closeYUVFile();
}

void VIdeoCapturer::AddCallback(std::function<void (uint8_t *, int32_t)> callback)
{
    callback_object = callback;
}

int VIdeoCapturer::openYUVFile(const char *file_name)
{
    _yuv_fp = fopen(file_name,"rb");
    if(!_yuv_fp)
        return -1;
    return 0;
}

int VIdeoCapturer::readYUVFile(uint8_t *yuv_buf, int32_t yuv_buf_size)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();//单位毫秒
    int64_t dif = cur_time - _yuv_start_time;
    if((int64_t)_yuv_total_duration > dif)
        return -1;
    //该读取数据了
    size_t ret = fread(yuv_buf,1,yuv_buf_size,_yuv_fp);
    if(ret != yuv_buf_size)
    {
        //从文件头部开始读取
        ret = fseek(_yuv_fp,0,SEEK_SET);
        ret = fread(yuv_buf,1,yuv_buf_size,_yuv_fp);
        if(ret != yuv_buf_size)
            return -1;
    }
//    LogDebug("yuv_total_duration:%lldms,%lldms",(int64_t)_yuv_total_duration,dif);
    _yuv_total_duration += _frame_duration;
    return 0;
}

int VIdeoCapturer::closeYUVFile()
{
    if(_yuv_fp)
        fclose(_yuv_fp);
    return 0;
}
