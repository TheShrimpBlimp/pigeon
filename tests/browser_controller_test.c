/* SPDX-License-Identifier: GPL-3.0-only */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
int main(void) {
    uint8_t enable[]={125,70,76,110,1}, disable[]={125,70,76,110,0};
    panel_init();host_wire_n=0;
    ed_handle(enable,sizeof enable);host_drain();
    if(!panel_controller_poll || !kb_boot_hold || host_wire_n!=8 || host_wire[4]!=110 || host_wire[5]!=0) return 1;
    fm1_ms+=20;fm1_in.buttons=1u<<panel.btn[B_PLAY];fm1_in.notes=5;
    host_wire_n=0;panel_controller_poll();host_drain();
    if(host_wire_n!=22 || host_wire[4]!=111 || host_wire[5]!=1 || (host_wire[6]|host_wire[7]<<7)!=(1u<<B_PLAY) || host_wire[8]!=5) return 2;
    for(uint32_t role=0;role<NE;role++) {
        fm1_ms+=20;host_wire_n=0;
        host_encoder_deltas[panel.enc[role]]=(int32_t)(role+1)*panel.dir[role];
        panel_controller_poll();host_drain();
        if(host_wire_n!=22 || host_wire[12+role]!=65+role)return 5;
        for(uint32_t other=0;other<NE;other++)if(other!=role && host_wire[12+other]!=64)return 6;
    }
    fm1_ms+=1501;panel_controller_poll();
    if(panel_controller_poll || kb_boot_hold) return 3;
    ed_handle(enable,sizeof enable);ed_handle(disable,sizeof disable);
    if(panel_controller_poll || kb_boot_hold) return 4;
    puts("Browser controller: lease, panel snapshot, all seven encoder roles, timeout and disconnect passed.");return 0;
}
