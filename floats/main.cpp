#include <stdio.h>
#import <QuartzCore/QuartzCore.h>

#include "pows.h"
#include "floats.h"
#include <string>
#include "ryu.h"
//#include <intrin.h>

static int sum = 0;

void benchmark(const char *title, void (*func)(double d, char *buffer)) {
    CFTimeInterval start = CACurrentMediaTime();
    char buffer[20];
    for (int i = 0; i < kNumbersSize; i++) {
        func(kNumbers[i], buffer);
        sum += buffer[0];
    }
    CFTimeInterval end = CACurrentMediaTime();
    printf("%s: %lf\n", title, end - start);
}

void c_style(double d, char *buffer) {
    snprintf(buffer, 20, "%lf", d);
}

void ryu_style(double d, char *buffer) {
    d2s_buffered(d, buffer);
}

void new_style(double d, char *buffer) {
    uint64_t bits = 0;
    __uint128_t sum = 0;
    memcpy(&bits, &d, sizeof(d));
    uint64_t mantissa = (bits & ((~0ULL) >> 12)) | (1ULL << 52);
    int idx = 50;
    while (mantissa) {
        sum += (mantissa & 1) * pows[idx];
        idx += 1;
        mantissa >>= 1;
        /*sum += pows[idx];
        int shift = __builtin_ctzll(mantissa) + 1;
        idx += shift;
        mantissa >>= shift;*/
    }
    memcpy(buffer, &sum, sizeof(sum));
    /*for (int i = 0; i < 16; i++) {
        buffer[i] = sum >> (8 * i);
    }*/
}

void cpp_style(double d, char *buffer) {
    std::to_string(d);
}

int main(int argc, const char * argv[]) {
    bool profiling = true;
    if (profiling) {
        while (1) {
            benchmark("new_style", new_style);
        }
    } else {
        benchmark("c_style", c_style);
        benchmark("cpp_style", cpp_style);
        benchmark("ryu_style", ryu_style);
        benchmark("new_style", new_style);
    }
    if (sum == 0) {
        printf("yo\n");
    }
    return 0;
}
