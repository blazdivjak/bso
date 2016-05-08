#include "libmath.h"

uint32_t sqrt(uint32_t v)
{
    uint32_t t, b, r;
    uint32_t q;
    r = v;           // r = v - x²
    b = 0x40000000;  // a²
    q = 0;           // 2ax
    while( b > 0 )
    {
        t = q + b;   // t = 2ax + a²
        q >>= 1;     // if a' = a/2, then q' = q/2
        if( r >= t ) // if (v - x²) >= 2ax + a²
        {
            r -= t;  // r' = (v - x²) - (2ax + a²)
            q += b;  // if x' = (x + a) then ax' = ax + a², thus q' = q' + b
        }
        b >>= 2;     // if a' = a/2, then b' = b / 4
    }
    return q;
}

uint16_t int_sqrt32(uint32_t x)
{
    uint16_t res=0;
    uint16_t add= 0x8000;   
    uint8_t i;
    for(i=0;i<16;i++)
    {
        uint16_t temp=res | add;
        uint32_t g2=temp*temp;      
        if (x>=g2)
        {
            res=temp;           
        }
        add>>=1;
    }
    return res;
}