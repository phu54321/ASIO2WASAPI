//
// Created by whyask37 on 2023-07-02.
//

#include <Windows.h>
#include "accutateTime.h"

static LARGE_INTEGER qpcFreq;

void initAccurateTime() {
    QueryPerformanceFrequency(&qpcFreq);
}

double accurateTime() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double) counter.QuadPart / (double) qpcFreq.QuadPart;
}
