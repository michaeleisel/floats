#include <stdio.h>
#import <QuartzCore/QuartzCore.h>

#include "pows.h"
#include "floats.h"
#include <string>
#include "ryu.h"
#include "pairs.h"
#include "pow2topow10.h"
#include "numtostrs.h"
#include "absl/strings/numbers.h"
#include "numtostrs_big.h"

static int sum = 0;

#define unlikely(x) __builtin_expect(x, 0)
#define likely(x) __builtin_expect(x, 1)

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

void handleUncommons(double d, char *buffer) {
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

// Note that NaN and infinity are not allowed in JSON
void new_mult_style(double d, char *buffer) {
    // todo: subnormals, infinity checks
    uint64_t bits = 0;
    memcpy(&bits, &d, sizeof(d));
    uint64_t mantissa = (bits & ((~0ULL) >> 12)) | (1ULL << 52);
    int exp = ((bits >> 52) & 0x7FF) - 1023;
    if (unlikely(exp == 1024 /* infinite or NaN */ || (exp == -1023 && mantissa != (1ULL << 52)/* subnormal */))) {
        handleUncommons(d, buffer);
        return;
    }
    bool isNegative = bits >> 63;
    if (isNegative) {
        buffer[0] = '-';
        buffer++;
    }
    PowerConversion conversion = kPowerConversions[exp];
    int16_t pow10 = conversion.power;
    if (mantissa > conversion.mantissaCutoff) {
        pow10++;
    }
    pow10 -= 17;
    pow10 = -pow10;
    Pair pair = getPair(pow10);
    int16_t power = pair.power - 64;
    power += (exp - 52);
    __uint128_t product128 = ((__uint128_t)pair.mantissa) * mantissa;
    uint64_t product = (uint64_t)(product128 >> (-power));
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
    memcpy(buffer, kBigStrings[hihi4], 4);
    memcpy(buffer + 4, kBigStrings[hilo4], 4);
    memcpy(buffer + 8, kBigStrings[lohi4], 4);
    // Could gate each of these memcpys with an if statement, then finding the zero cutoff is easier
    memcpy(buffer + 12, kBigStrings[lolo4], 4);
    /*char trailingZeros = 0;
    if (lo8) {
        trailingZeros = lolo4 ? 12 : 8;
    } else {
        trailingZeros = hilo4 ? 4 : 0;
    }
    for (int i = trailingZeros + 3; i >= trailingZeros; i--) {
        if (buffer[i] != '0') {
            buffer[i + 1] = '\0';
            break;
        }
    }*/
    char *mantissaEnd = buffer - 1; // Remove '.' if unnecessary
    for (int i = 15; i >= 0; i--) {
        if (buffer[i] != '0') {
            mantissaEnd = buffer + i + 1;
            break;
        }
    }
    buffer = mantissaEnd; // Just overwrite what we had
    /*int32_t target = -pow10 + 17;
    if (target == 0) {
        return;
    }
    if (target < 0) {
        (*buffer++) = '-';
        target = -target;
    }
    (*buffer++) = 'e';
    char *stringStart = kBigStrings[target];
    stringStart++; // We know it's limited to 3 digits, so we can assume the first digit in target is 0
    char *string = stringStart;
    while (*string == '0') { // This loop would be problematic if target was 0, but we've checked above that it's not
        string++;
    }
    char length = 3 - (string - stringStart);
    memcpy(buffer, string, length);
    buffer[length] = '\0';*/

    /*int i = 0;
    for (; i < 15; i += 3) {
        // lldiv_t result = lldiv(product, 1000);
        // const char *string = kStrings[result.rem];
        // product = result.quot;
        const char *string = kStrings[product % 1000];
        buffer[i] = string[0];
        buffer[i + 1] = string[1];
        buffer[i + 2] = string[2];
        product /= 1000;
    }
    buffer[i++] = '0' + product % 10;
    buffer[i++] = '0' + product / 10;*/
    /*for (int i = 0; i < 16; i++) {
        // lldiv_t result = lldiv(product, 10);
        // buffer[15 - i] = '0' + result.rem;
        // product = result.quot;
        // buffer[15 - i] = '0' + (product % 10);
        // product /= 10;
    }*/
}

void absl_go(double d, char *buffer) {
    absl::numbers_internal::SixDigitsToBuffer(d, buffer);
}

void t(double d) {
    char buffer[40];
    d2s_buffered(d, buffer);
    printf("%s\n", buffer);
}

int main(int argc, const char * argv[]) {
    t(INFINITY);
    t(-INFINITY);
    t(NAN);
    while (0 /* for profiling */) {
        benchmark("new_mult_style", new_mult_style);
    }
    //benchmark("c_style", c_style);
    //benchmark("cpp_style", cpp_style);
    benchmark("ryu_style", ryu_style);
    //benchmark("new_style", new_style);
    benchmark("new_mult_style", new_mult_style);
    //benchmark("absl_go", absl_go);
    if (sum == 0) {
        printf("yo\n");
    }
    return 0;
}
