#ifndef COROUTINE_CPP_H
#define COROUTINE_CPP_H

#include <functional>
#include <memory>
#include <vector>
#include <ucontext.h>

enum CoroutineStatus {  // 协程状态码
    COROUTINE_DEAD,
    COROUTINE_READY,
    COROUTINE_RUNNING,
    COROUTINE_SUSPEND
};

using namespace std;

class Schedule;  //调度类

using CoroutineFunc =  function<void(Schedule*, void*)>;

class Schedule {
public:
    Schedule();
    ~Schedule();


    /*  功能：在调度器中创建一个新的协程
        参数： 
            func定义协程的行为
            ud指向用户自定义数据的指针，这些数据将传递给协程函数
        返回：新创建的协程的ID
        解释：这个函数将初始化一个新的Coroutine对象，分配一个ID，存储在vector中
    */
    int create(CoroutineFunc func, void* ud);

    /*  功能：当前运行的协程让出执行权，返回主上下文
        解释：这个函数保存当前协程的堆栈状态，将其状态更新为 SUSPEND，并切换上下文回到调度器的主上下文
    */
    void yield();

    /*  功能：恢复‘id’标识的协程的执行
        参数：id，要恢复执行的协程的ID
        解释：这个函数确保协程处于READY或SUSPEND状态，它将设置协程的上下文切换并切换到该上下文，让其成为当前运行的协程
    */
    void resume(int id);

    /*  功能：查询‘id’标识的协程的状态
        参数：id:要查询状态的协程的ID
        返回值：协程的状态：（COROUTINE_DEAD 0
                            COROUTINE_READY 1
                            COROUTINE_RUNNING 2
                            COROUTINE_SUSPEND 3）
        解释：这个函数返回指定协程的状态，确保其在协程列表的范围内
    */
    int status(int id) const;

    /*  功能：获取当前运行协程的ID
        返回值：返回当前运行的协程的id，如果没有协程正在运行返回-1
    */
    int runningid() const;

private:
    struct Coroutine;  // 结构体用来表示单个协程
    static void mainfunc(uint32_t low32,uint32_t hi32);
    void save_stack(Coroutine* C, char* top);  // C保存堆栈的协程对象的指针, top当前堆栈顶部指针
                                               // 在协程暂停或切换时保存当前的堆栈状态，恢复时可以从暂停的地方继续执行
                                        
    char stack_[1024*1024];     // 调度器的堆栈空间
    ucontext_t main_;   // ucontext_t类型变量，保存主上下文（调度器的主执行环境）
    int nco_;           // 当前协程数量
    int cap_;           // 调度器可容纳协程的数量，随着协程数量的增加，这个容量会动态扩展
    int runningid_;     // 正在运行的协程的id，如果没有协程在运行，则为-1
    vector<unique_ptr<Coroutine>> co_;      // co_为一个vector，每一个元素都是指向Coroutine对象的unique_ptr，使用智能指针确保内存管理自动且安全

};


#endif