#include "coroutine_cpp.h"
#include <bits/stdc++.h>
using namespace std;

void coroutine_func(Schedule* S, void* ud) {
    int* arg = static_cast<int*>(ud);
    for (int i = 0; i < 5; ++i) {
        cout << "Coroutine " << *arg << ": " << i << endl;
        S->yield();  // 打印完就让出执行权
    }
}


int main() {
    Schedule S;

    int arg1 = 1;
    int arg2 = 2;

    int co1 = S.create(coroutine_func, &arg1);
    int co2 = S.create(coroutine_func, &arg2);
    while (S.status(co1) != COROUTINE_DEAD || S.status(co2) != COROUTINE_DEAD) {
        if (S.status(co1) != COROUTINE_DEAD) S.resume(co1);
        if (S.status(co2) != COROUTINE_DEAD) S.resume(co2);
    }

    return 0;
}