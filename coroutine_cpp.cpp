#include "coroutine_cpp.h"
#include <cassert>
#include <cstring>
#include <ucontext.h>

#define STACK_SIZE (1024 * 1024)
#define DEFAULT_COROUTINE 16

struct Schedule::Coroutine {
    CoroutineFunc func;     // 定义协程的执行函数，每个协程运行时候都会执行这个函数
    void* ud;               // 指向任意数据类型的指针，用户数据指针，通过这个指针协程函数可以访问和操作外部传递给它的数据
    ucontext_t ctx;         // ucontext_t是一个结构体，用来保存上下文（寄存器，堆栈指针）
    Schedule* sch;          // 指向该协程所属调度器的指针
    ptrdiff_t cap;
    ptrdiff_t size;
    int status;
    char* stack;

    // 构造函数
    Coroutine(Schedule* S, CoroutineFunc func, void* ud)
        : func(func), ud(ud), sch(S), cap(0), size(0), status(COROUTINE_READY), stack(nullptr) {}
    
    // 析构函数
    ~Coroutine() {
        delete[] stack;
    }
};

Schedule::Schedule()
    : nco_(0), cap_(DEFAULT_COROUTINE), runningid_(-1) {
        co_.resize(cap_);
    }

Schedule::~Schedule() {

}


// 创建新的协程
int Schedule::create(CoroutineFunc func, void* ud) {
    auto co = std::make_unique<Coroutine>(this, func, ud);  // 使用make_unique创建一个Coroutine对象然后返回指向该对象的unique_ptr
                                                            // Coroutine构造函数会使用这些函数初始化一个新的协程对象
                                                            // this指向当前的Schedule对象，func是协程函数，ud是用户数据
    // 如果当前协程数量超过容量，增加容量
    if (nco_ >= cap_) {
        cap_ *= 2;      // 容量翻倍
        co_.resize(cap_);
    }

    // 在当前容量范围内寻找一个空闲的位置存放新的协程
    for (int i = 0; i < cap_; i++) {
        int id = ( i + nco_) % cap_;  // 计算协程的id，环形缓冲区的计算方式
        if (!co_[id]) { // 如果当前位置为空闲
            co_[id] = std::move(co);  // 将新创建的协程放入这个位置
            nco_++;     // 增加当前协程数量
            return id;  // 返回当前协程的id
        }
    }

    // 如果没没找到空闲位置，因为我们确保有足够的容量来存放新的协程，理论上不会执行到这里
    assert(false);  // 触发断言失败，执行到这一步说明程序有问题，会终止报错
    return -1;      // 返回错误码
}

void Schedule::mainfunc(uint32_t low32, uint32_t hi32) {
    uintptr_t ptr = static_cast<uintptr_t>(low32) | (static_cast<uintptr_t>(hi32) << 32);
    Schedule* S = reinterpret_cast<Schedule*>(ptr);
    int id = S->runningid_;
    auto& C = S->co_[id];
    C->func(S, C->ud);
    C->status = COROUTINE_DEAD;
    S->nco_--;
    S->runningid_ = -1;
}

// 恢复id标识的协程的运行
void Schedule::resume(int id) {
    assert(runningid_ == -1);       // 确保没有其他协程在运行，一个时间点只有一个协程运行
    assert(id >= 0 && id < cap_);   // 确保确保传入的id有效
    Coroutine* C = co_[id].get();   // 获取指定id的协程的指针
    if(C == nullptr) return;        // 如果为空直接返回

    uintptr_t ptr = reinterpret_cast<uintptr_t>(this);  // 获取当前调度器对象指针并转换为整数
    switch (C->status) {            // 根据协程状态判断
        case COROUTINE_READY:
            getcontext(&C->ctx);        // 获取当前上下文保存到C->ctx
            C->ctx.uc_stack.ss_sp = stack_;     // 设置协程的栈空间
            C->ctx.uc_stack.ss_size = STACK_SIZE;   //设置栈大小
            C->ctx.uc_link = &main_;        // 设置上下文切换返回点是主上下文
            runningid_ = id;
            C->status = COROUTINE_RUNNING;      
            // 设置协程的入口函数和参数，mainfunc是协程开始执行的函数，这里将调度器对象指针分成32位整数传递给mainfunc
            makecontext(&C->ctx, reinterpret_cast<void(*)()>(mainfunc), 2, static_cast<uint32_t>(ptr), static_cast<uint32_t>(ptr >> 32));
            // 保存当前上下文到main，然后切换到ctx对应的上下文执行，即执行上面设置的mainfunc，执行完再且回到这里
            swapcontext(&main_, &C->ctx);       
            break;

        case COROUTINE_SUSPEND:
            memcpy(stack_ + STACK_SIZE - C->size, C->stack, C->size);   // 恢复协程的栈内容
            runningid_ = id;
            C->status = COROUTINE_RUNNING;
            swapcontext(&main_, &C->ctx);
            break;

        default:
            assert(false);
    }
}

// 当前运行的协程让出执行权，返回主上下文
void Schedule::yield() {
    int id = runningid_;
    assert(id >= 0);
    auto& C = co_[id];
    save_stack(C.get(), stack_ + STACK_SIZE);
    C->status = COROUTINE_SUSPEND;
    runningid_ = -1;
    swapcontext(&C->ctx, &main_);
}

// 查看id协程的状态
int Schedule::status(int id) const {
    assert(id >= 0 && id < cap_);
    if (!co_[id]) {
        return COROUTINE_DEAD;
    }
    return co_[id]->status;
}

// 返回正在运行的协程的id
int Schedule::runningid() const{
    return runningid_;
}

// 保存协程的栈状态，C:指向要保存栈状态的协程对象，top:指向当前栈顶位置的指针
void Schedule::save_stack(Coroutine* C, char* top) {
    // 使用dummy计算当前栈顶地址，即esp
    char dummy = 0;
    assert(top - &dummy <= STACK_SIZE);
    // top - &dummy算出当前栈的上下文有多大，如果比当前容量大，就扩容
    if (C->cap < top - &dummy) {
        delete[] C->stack;
        C->cap = top - &dummy;
        C->stack = new char[C->cap];
    }
    // 记录当前栈大小
    C->size = top - &dummy;
    // 复制公共栈的数据到私有栈
    memcpy(C->stack, &dummy, C->size);
}
