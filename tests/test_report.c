#include <assert.h>
#include <stdio.h>
#include "../src/sfc_report.h"
int main(void) {
    uint16_t m;
    uint8_t report[8] = {0};
    const uint8_t keys[] = {0x1d,0x04,0x2b,0x28,0x52,0x51,0x50,0x4f,0x1b,0x16,0x14,0x1a};
    for (unsigned i=0;i<12;i++) { report[2]=keys[i]; assert(sfc_decode(report,8,&m) && m==(1u<<i)); }
    report[2]=0x1d;report[3]=0x04;report[4]=0x4f;
    assert(sfc_decode(report,8,&m) && m==0x083);
    report[2]=0;assert(sfc_decode(report,8,&m) && m==0x082);
    report[3]=report[4]=0;assert(sfc_decode(report,8,&m) && m==0);
    report[2]=1;assert(!sfc_decode(report,8,&m) && m==0);
    assert(!sfc_decode(report,7,&m) && m==0);
    puts("12 mappings, simultaneous input, partial/full release and malformed reports: PASS");
}
