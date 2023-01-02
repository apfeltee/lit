
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include "lit.h"

#if 0
    #define pack754_32(f) (pack754((f), 32, 8))
    #define unpack754_32(i) (unpack754((i), 32, 8))
#endif

#define pack754_64(f) (pack754((f), 64, 11))
#define unpack754_64(i) (unpack754((i), 64, 11))

uint64_t pack754(long double f, unsigned bits, unsigned expbits)
{
    long double fnorm;
    int shift;
    long long sign;
    long long exp;
    long long significand;
    unsigned significandbits;
    /* -1 for sign bit */
    significandbits = bits - expbits - 1;
    /* get this special case out of the way */
    if(f == 0.0)
    {
        return 0;
    }
    /* check sign and begin normalization */
    if(f < 0)
    {
        sign = 1;
        fnorm = -f;
    }
    else
    {
        sign = 0;
        fnorm = f;
    }
    /* get the normalized form of f and track the exponent */
    shift = 0;
    while(fnorm >= 2.0)
    {
        fnorm /= 2.0;
        shift++;
    }
    while(fnorm < 1.0)
    {
        fnorm *= 2.0;
        shift--;
    }
    fnorm = fnorm - 1.0;
    /* calculate the binary form (non-float) of the significand data */
    significand = fnorm * ((1LL << significandbits) + 0.5f);
    /* get the biased exponent */
    /* shift + bias */
    exp = shift + ((1<<(expbits-1)) - 1);
    /* return the final answer */
    return (
        (sign << (bits - 1)) | (exp << (bits - expbits - 1)) | significand
    );
}

long double unpack754(uint64_t i, unsigned bits, unsigned expbits)
{
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits;
    /* -1 for sign bit */
    significandbits = bits - expbits - 1;
    if(i == 0)
    {
        return 0.0;
    }
    /* pull the significand */
    /* mask */
    result = (i&((1LL<<significandbits)-1));
    /* convert back to float */
    result /= (1LL<<significandbits);
    /* add the one back on */
    result += 1.0f;
    /* deal with the exponent */
    bias = ((1 << (expbits - 1)) - 1);
    shift = (((i >> significandbits) & ((1LL << expbits) - 1)) - bias);
    while(shift > 0)
    {
        result *= 2.0;
        shift--;
    }
    while(shift < 0)
    {
        result /= 2.0;
        shift++;
    }
    /* sign it */
    if(((i>>(bits-1)) & 1) == 1)
    {
        result = result * -1.0;
    }
    else
    {
        result = result * 1.0;
    }
    return result;
}

/* this used to be done via type punning, which may not be portable */
double lit_util_uinttofloat(unsigned int val)
{
    return unpack754_64(val);
}

unsigned int lit_util_floattouint(double val)
{
    return pack754_64(val);
}

int lit_util_doubletoint(double n)
{
    if(n == 0)
    {
        return 0;
    }
    if(isnan(n))
    {
        return 0;
    }
    if(n < 0)
    {
        n = -floor(-n);
    }
    else
    {
        n = floor(n);
    }
    if(n < INT_MIN)
    {
        return INT_MIN;
    }
    if(n > INT_MAX)
    {
        return INT_MAX;
    }
    return (int)n;
}

int lit_util_numbertoint32(double n)
{
    /* magic. no idea. */
    double two32 = 4294967296.0;
    double two31 = 2147483648.0;
    if(!isfinite(n) || n == 0)
    {
        return 0;
    }
    n = fmod(n, two32);
    if(n >= 0)
    {
        n = floor(n);
    }
    else
    {
        n = ceil(n) + two32;
    }
    if(n >= two31)
    {
        return n - two32;
    }
    return n;
}

unsigned int lit_util_numbertouint32(double n)
{
    return (unsigned int)lit_util_numbertoint32(n);
}


// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
int lit_util_closestpowof2(int n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}


char* lit_util_patchfilename(char* file_name)
{
    int i;
    int name_length;
    char c;
    name_length = strlen(file_name);
    // Check, if our file_name ends with .lit or lbc, and remove it
    if(name_length > 4 && (memcmp(file_name + name_length - 4, ".lit", 4) == 0 || memcmp(file_name + name_length - 4, ".lbc", 4) == 0))
    {
        file_name[name_length - 4] = '\0';
        name_length -= 4;
    }
    // Check, if our file_name starts with ./ and remove it (useless, and makes the module name be ..main)
    if(name_length > 2 && memcmp(file_name, "./", 2) == 0)
    {
        file_name += 2;
        name_length -= 2;
    }
    for(i = 0; i < name_length; i++)
    {
        c = file_name[i];
        if(c == '/' || c == '\\')
        {
            file_name[i] = '.';
        }
    }
    return file_name;
}

char* lit_util_copystring(const char* string)
{
    size_t length;
    char* new_string;
    length = strlen(string) + 1;
    new_string = (char*)malloc(length);
    memcpy(new_string, string, length);
    return new_string;
}
