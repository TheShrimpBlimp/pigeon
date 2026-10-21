/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN ko_controls_test_main
#include "ko_test.c"
int main(void)
{
    int bad=0;
    memset(ENG_MEM(0),0xA5,ENG_MEM_PART);
    memset(ENG_MEM(2),0xC3,ENG_MEM_PART);
    memset(ENG_MEM(1),0x5A,ENG_KIT_MEM);
    trk[1].engine=13;eng_mem_owner[1]=0;eng_mem_claim(&trk[1]);
    int clean=1,songs=1;
    for(uint32_t i=0;i<ENG_KIT_MEM;i++)clean&=((uint8_t *)ENG_MEM(1))[i]==0;
    for(uint32_t i=0;i<NVOICE;i++)for(uint32_t j=0;j<SLC_RB;j++)slc_rb(&trk[1],&trk[1].v[i])[j]=(int16_t)(i*100+j);
    for(uint32_t i=0;i<ENG_MEM_PART;i++)songs&=((uint8_t *)ENG_MEM(0))[i]==0xA5 && ((uint8_t *)ENG_MEM(2))[i]==0xC3;
    bad+=check("kit reverse windows fit and cannot overwrite either full song cache",clean&&songs);
    bad+=check("kit no longer reserves a song-sized cache",sizeof eng_mem==2u*ENG_MEM_PART && sizeof eng_kit_mem==1024u);
    bad+=check("reserved fourth track has no workspace",ENG_MEM(3)==0 && slc_rb(&trk[3],&trk[3].v[0])==0);
    printf("PIGEON workspaces: %s\n",bad?"FAIL":"PASS");return bad!=0;
}
