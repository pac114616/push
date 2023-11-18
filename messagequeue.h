#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H
#include <mutex>
#include <condition_variable>
#include <list>
extern "C"{
#include "libavcodec/avcodec.h"
}
#define MSG_FLUSH   1
#define MSG_RTSP_ERROR  100
#define MSG_RTSP_QUEUE_DURATION 101

typedef struct AVMessage
{
    int what; //消息类型
    int arg1;
    int arg2;
    void *obj;  //如果两个参数不够用，则传入结构体
    void (*free_l)(void *obj);  //释放结构体
}AVMessage;
static void msg_obj_free_l(void *obj)
{
    av_free(obj);
}
class MessageQueue
{
public:
    MessageQueue() {}
    ~MessageQueue(){
        msg_queue_flush();
    }
    int msg_queue_put(AVMessage *msg)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        int ret = msg_queue_put_private(msg);
        if(ret == 0)
            _cond.notify_one(); //正常插入队列才会notify
        return ret;
    }
    inline void msg_init_msg(AVMessage *msg)
    {
        memset(msg,0,sizeof(AVMessage));
    }
    //返回值-1代表abort 0代表没有消息；1代表读取到了消息
    //其中msg不能只传指针进来，还需对该指针分配内存再传进来，不为空
    //timeout:-1代表阻塞等待；0代表非阻塞等待；>0代表有超时等待
    int msg_queue_get(AVMessage *msg,int timeout)
    {
        if(!msg)
        {
            return -2;
        }
        std::unique_lock<std::mutex> lock(_mutex);
        AVMessage *msg1;
        int ret;
        while(1)
        {
            if(_abort_request)
            {
                ret = -1;
                break;
            }
            if(!_queue.empty())
            {
                //队列不为空读取message
                msg1 = _queue.front();
                *msg = *msg1; //拷贝回去
                _queue.pop_front();
                av_free(msg1);
                ret = 1;
                break;
            }else if(0 == timeout)
            {
                ret = 0;        //没有消息
                break;
            }else if(timeout<0)
            {
                //死等待，等到队列不为空或者abort请求才退出wait
                _cond.wait(lock,[this]{
                    return !_queue.empty()|_abort_request;
                });
            }else if(timeout>0)
            {//等到超时
                _cond.wait_for(lock,std::chrono::milliseconds(timeout),[this]{
//                    return true; return true没有超时效果
                    return !_queue.empty()|_abort_request;
                });
                if(_queue.empty())
                 {
                    ret = 0;
                    break;
                }
            }else
            {
                ret = -2;
                break;
            }

        }
        return ret;
    }//把所有该类型的消息全部删除
    void msg_queue_remove(int what)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        while(!_abort_request&&!_queue.empty())
        {
            std::list<AVMessage *>::iterator it;
            AVMessage *msg = NULL;
            for(it = _queue.begin();it!=_queue.end();it++)
            {
                if((*it)->what == what)
                {
                    msg = *it;
                    break;
                }
            }
            if(msg)
            {
                if(msg->obj&&msg->free_l)
                {
                    msg->free_l(msg->obj);
                }
                av_free(msg);
                _queue.remove(msg);
            }else
            {
                break;
            }
        }
    }
    //只有消息类型
    void notify_msg1(int what)
    {
        AVMessage msg;
        msg_init_msg(&msg);
        msg.what = what;
        msg_queue_put(&msg);
    }
    void notify_msg2(int what,int arg1)
    {
        AVMessage msg;
        msg_init_msg(&msg);
        msg.what = what;
        msg.arg1 = arg1;
        msg_queue_put(&msg);
    }
    void notify_msg3(int what,int arg1,int arg2)
    {
        AVMessage msg;
        msg_init_msg(&msg);
        msg.what = what;
        msg.arg1 = arg1;
        msg.arg2 = arg2;
        msg_queue_put(&msg);
    }
    void notify_msg4(int what,int arg1,int arg2,void *obj,int obj_len)
    {
        AVMessage msg;
        msg_init_msg(&msg);
        msg.what = what;
        msg.arg1 = arg1;
        msg.arg2 = arg2;
        msg.obj = av_malloc(obj_len);
        msg.free_l = msg_obj_free_l;
        memcpy(msg.obj,obj,obj_len);

        msg_queue_put(&msg);
    }
    void msg_queue_abort()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _abort_request = 1;
    }
    void msg_queue_flush()
    {
        std::lock_guard<std::mutex>lock(_mutex);
        while(!_queue.empty())
        {
            AVMessage *msg = _queue.front();
            if(msg->obj&&msg->free_l)
                msg->free_l(msg->obj);
            _queue.pop_front();
            av_free(msg);
        }
    }
    void msg_destory_(MessageQueue *q)
    {
        msg_queue_flush();
    }
private:
    int _abort_request = 0;
    std::mutex _mutex;
    std::condition_variable _cond;
    std::list<AVMessage *> _queue;


    int msg_queue_put_private(AVMessage *msg)
    {
        //重新new一个
        if(_abort_request)
            return -1;
        AVMessage *msg1 = (AVMessage *)av_malloc(sizeof(AVMessage));
        if(!msg1)
            return -1;
        *msg1 = *msg; //赋值
        _queue.push_back(msg1);
        return 0;
    }
};
#endif // MESSAGEQUEUE_H
