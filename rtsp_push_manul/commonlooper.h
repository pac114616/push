#ifndef COMMONLOOPER_H
#define COMMONLOOPER_H
#include <thread>
#include "mediabase.h"

class CommonLooper
{
public:
    CommonLooper();
    virtual ~CommonLooper();
    virtual RET_CODE Start();   //开始线程 自定义返回值
    virtual void Stop();        //停止线程
    virtual void Loop() = 0;
    virtual bool GetRunning();
    virtual void SetRunning(bool running);
private:
    static void *trampoline(void *p);
protected:
    std::thread *_worker = NULL;//线程
    bool request_abort = false; //请求退出线程的标志
    bool _running = true;       //线程是否在运行


};

#endif // COMMONLOOPER_H
