#include <stdio.h>
#import <QuartzCore/QuartzCore.h>

#include "pows.h"
#include "floats.h"
#include <string>
#include "ryu/ryu.h"
#include "pairs.h"
#include "pow2topow10.h"
#include "numtostrs.h"
#include "absl/strings/numbers.h"
#include "numtostrs_big.h"

static int sum = 0;

#define unlikely(x) __builtin_expect(x, 0)
#define likely(x) __builtin_expect(x, 1)

#define always_inline __attribute__((always_inline))

void infoFromDouble(double d) {
    uint64_t bytes = 0;
    memcpy(&bytes, &d, sizeof(d));
    uint64_t exp = (bytes >> 52) & 0x7FF;
    uint64_t mantissa = ((bytes << 12) >> 12) | (1ULL << 52);
    printf("exp: %llu, mantissa: %llu\n", exp, mantissa);
}

void benchmark(const char *title, void (*func)(double d, char *buffer)) {
    char buffer[30];
    // warmup
    for (int i = 0; i < kNumbersSize; i++) {
        func(kNumbers[i], buffer);
        sum += buffer[0];
    }
    CFTimeInterval start = CACurrentMediaTime();
    for (int z = 0; z < 100; z++) {
        for (int i = 0; i < kNumbersSize; i++) {
            func(kNumbers[i], buffer);
            sum += buffer[0];
        }
    }
    CFTimeInterval end = CACurrentMediaTime();
    printf("%s: %.3e\n", title, end - start);
}

void c_style(double d, char *buffer) {
    snprintf(buffer, 20, "%lf", d);
}

void ryu_style(double d, char *buffer) {
    d2s_buffered(d, buffer);
    //d2exp_buffered(d, buffer);
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
}

void cpp_style(double d, char *buffer) {
    std::to_string(d);
}

void handleUncommonCases(double d, char *buffer) {
    if (std::isinf(d)) {
        if (d < 0) {
            (*buffer++) = '-';
        }
        strcpy(buffer, "Infinity");
    } else if (std::isnan(d)) {
        strcpy(buffer, "NaN");
    } else if (!std::isnormal(d)) {
        // This logic is platform-specific anyways, so let the platform handle it
        sprintf(buffer, "%0.17e", d);
    }
}

__used static void u8(__m128i i) {
    uint8_t buffer[16] = {0};
    _mm_storeu_si128((__m128i *)buffer, i);
    for (int i = 0; i < 16; i++) {
        printf("%u, ", buffer[i]);
    }
    printf("\n");
}

__used static void u16(__m128i i) {
    uint16_t buffer[8] = {0};
    _mm_storeu_si128((__m128i *)buffer, i);
    for (int i = 0; i < 8; i++) {
        printf("%u, ", buffer[i]);
    }
    printf("\n");
}

__used static void u32(__m128i i) {
    uint32_t buffer[4] = {0};
    _mm_storeu_si128((__m128i *)buffer, i);
    for (int i = 0; i < 4; i++) {
        printf("%u, ", buffer[i]);
    }
    printf("\n");
}

void fallback(double d, char *buffer) {
    snprintf(buffer, 25, "%0.16e", d);
}

void tester(uint64_t mantissa, uint64_t exp) {
    // Compute product
    PowerConversion conversion = kPowerConversions[exp];
    int16_t pow10 = conversion.power;
    if (mantissa > conversion.mantissaCutoff) {
        pow10--;
    }
    Pair pair = getPair(pow10);
    int16_t power = pair.power;
    power -= exp;
    printf("%llu\n", power);
}

// Note that NaN and infinity are not allowed in JSON
// todo: add subnormal/infinity tests, zero test for all forms, e.g. negative
// todo: is 17 digits still sufficient when we're rounding down and not to nearest?
void new_mult_style(double d, char *buffer) {
    // Decompose double
    uint64_t bits = 0;
    memcpy(&bits, &d, sizeof(d));
    uint64_t mantissa = (bits & ((~0ULL) >> 12)) | (1ULL << 52);
    int exp = ((bits >> 52) & 0x7FF) - 1023;

    // Handle uncommon cases
    if (unlikely(exp == 1024 /* infinite or NaN */ || (exp == -1023 && mantissa != (1ULL << 52)/* subnormal */))) {
        handleUncommonCases(d, buffer);
        return;
    }

    // Handle if negative
    bool isNegative = bits >> 63;
    if (isNegative) {
        buffer[0] = '-';
        buffer++;
    }

    // Compute product
    PowerConversion conversion = kPowerConversions[exp];
    int16_t pow10 = conversion.power;
    if (mantissa > conversion.mantissaCutoff) {
        pow10--;
    }
    Pair pair = getPair(pow10);
    int16_t power = pair.power;
    power -= exp;
    __uint128_t product128 = ((__uint128_t)pair.mantissa) * mantissa;
    uint64_t product = (uint64_t)(product128 >> power);
    if (unlikely(!(0 <= pow10 && pow10 < 26))) {
        if (unlikely((product128 + mantissa) >> power != product)) {
            fallback(d, buffer);
            return;
        }
    }

    // Write out digits
    uint64_t first = product / 10000000000000000ULL;
    product -= first * 10000000000000000ULL;
    (*buffer++) = '0' + first;
    (*buffer++) = '.';
    uint32_t lo8 = (uint32_t)(product % 100000000);
    uint32_t hi8 = (uint32_t)(product / 100000000);
    uint16_t lolo4 = lo8 % 10000;
    uint16_t lohi4 = lo8 / 10000;
    uint16_t hilo4 = hi8 % 10000;
    uint16_t hihi4 = hi8 / 10000;
    // Could gate each of these memcpys with an if statement, then finding the zero cutoff is easier
    memcpy(buffer, kBigStrings[hihi4], 4);
    memcpy(buffer + 4, kBigStrings[hilo4], 4);
    memcpy(buffer + 8, kBigStrings[lohi4], 4);
    memcpy(buffer + 12, kBigStrings[lolo4], 4);

    // Remove trailing zeros
    char *mantissaEnd = buffer - 1; // Remove '.' if unnecessary
    for (int i = 15; i >= 0; i--) {
        if (buffer[i] != '0') {
            mantissaEnd = buffer + i + 1;
            break;
        }
    }
    buffer = mantissaEnd; // Just overwrite what we had

    // Add exponent
    int32_t target = -pow10 + 17 - 1 /* account for digit to left of decimal */;
    if (target == 0) {
        *buffer = '\0';
        return;
    }
    if (target < 0) {
        (*buffer++) = '-';
        target = -target;
    }
    (*buffer++) = 'E';
    const char *string = kBigStrings[target];
    if (unlikely(target >= 100)) {
        (*buffer++) = string[1];
    }
    if (unlikely(target >= 10)) {
        (*buffer++) = string[2];
    }
    (*buffer++) = string[3];
}

void absl_go(double d, char *buffer) {
    absl::numbers_internal::SixDigitsToBuffer(d, buffer);
}

void t(double d) {
    char buffer[40];
    d2s_buffered(d, buffer);
    printf("%s\n", buffer);
}

void run_shift_test() {
    uint64_t minMantissa = 1ULL << 53;
    uint64_t maxMantissa = (1ULL << 54) - 1;
    for (int exp = -200; exp < 200; exp++) {
        tester(minMantissa, exp);
        tester(maxMantissa, exp);
    }
}

void run_tests() {
    for (int i = 0; i < kNumbersSize; i++) {
        char actual[30] = {0};
        char expected[30] = {0};
        new_mult_style(kNumbers[i], actual);
        ryu_style(kNumbers[i], expected);
        if (strcmp(expected, actual)) {
            abort();
        }
    }
}

int main(int argc, const char * argv[]) {
    run_tests();
    t(INFINITY);
    t(-INFINITY);
    t(NAN);
    while (1 /* for profiling */) {
        //benchmark("ryu_style", ryu_style);
        benchmark("new_mult_style", new_mult_style);
    }
    //benchmark("c_style", c_style);
    //benchmark("cpp_style", cpp_style);
    for (int i = 0; i < 3; i++) {
        benchmark("ryu_style     ", ryu_style);
        benchmark("new_mult_style", new_mult_style);
    }
    if (sum == 0) {
        printf("yo\n");
    }
    return 0;
}
