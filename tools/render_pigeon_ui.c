/* SPDX-License-Identifier: GPL-3.0-only */
/* Render application UI using the real firmware renderer and synthetic audio. */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN render_fixture_main
#include "../tests/ko_test.c"
static const char *out_dir;
static uint32_t capture_count;
static void save_raw(const char *category,const char *slug,const char *label)
{
    char file[640];snprintf(file,sizeof file,"%s/%03u-%s.ppm",out_dir,++capture_count,slug);
    FILE *f=fopen(file,"wb");if(!f){perror(file);exit(1);}fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240u*240u;i++){uint16_t c=swap16(host_screen[i]);uint8_t p[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);
    printf("%03u-%s\t%s\t%s\n",capture_count,slug,category,label);
}
static void reset_page(uint32_t track)
{
    song.sel=(uint8_t)track;song.playing=0;song.rec=0;transport_req=0;cin_left=cin_total=0;
    memset(&nm,0,sizeof nm);ui.menu=ui.confirm=ui.layer=ui.lock=ui.ly=ui.uboot=0;ui.home=1;ui.page=0;
    ui.msg_t=ui.hot_t=ui.bpm_t=0;ui.msg2[0]=0;ui.force=1;ui.head_sig=~0u;
    ko_ui_invalidate();lcd_fill(0,0,240,240,T_BG);
}
static void capture(const char *category,const char *slug,const char *label)
{ko_ui_invalidate();ui.force=1;ui.head_sig=~0u;ui_draw();save_raw(category,slug,label);}
static const char *const views[29]={"Waveform / home","FX amounts","Drive / shared delay","Compressor dynamics","Compressor makeup","Chop settings","Mute / solo","Sequence grid","Patterns A-D","Arrangement","Mixer levels","Mixer delay sends","Mixer chorus sends","Mixer reverb sends","Step details","Swing / timing","Drum pad settings",0,"Arrangement tools","Chop envelopes","Chop copy / split / merge","Recording preferences","Pattern variations","Tap / loop tempo","Step pitch / nudge / condition","Chop groups / layouts","Drum rolls","Speed sync","Chop swap / move"};
static int allowed(uint32_t track,uint32_t view)
{
    if(!views[view])return 0;
    if(track==1 && (view==5||view==19||view==20||view==25||view==27||view==28))return 0;
    if(track!=1 && (view==16||view==26))return 0;
    return 1;
}
int main(int argc,char **argv)
{
    if(argc!=2)return 2;out_dir=argv[1];ui_power_on();memset(ko_states,0,sizeof ko_states);
    memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);
    for(uint32_t k=0;k<3;k++){smp_user_hdr_t *h=(smp_user_hdr_t *)((uint8_t *)ko_flash+k*SMP_USER_SIZE);memcpy(h->name,k==0?"SONG ONE":k==1?"DRUM KIT":"SONG TWO",8);}
    ko_product_boot();render_blocks(32);go_home();ko_palette_on=1;palette_set(0);
    for(uint32_t k=0;k<3;k++){
        trk[k].p[P_SLEN]=16;ko_seq[k].enabled=1;ko_seq[k].cursor=4;
        trk[k].p[P_DLY]=32;trk[k].p[P_CHOR]=18;trk[k].p[P_REV]=46;trk[k].p[P_DIST]=24;trk[k].p[P_LEVEL]=100;
        for(uint32_t j=0;j<16;j++)trk[k].step[j]=(step_t){.note={60+j%4},.n=(j%4==0||j==6),.time=ST_NOTE,.vel=100};
    }
    ko.comp[0]=(ko_comp_t){18,4,12,18,3};ko_states[1].comp[0]=ko.comp[0];ko.comp[1]=ko.comp[0];
    song.g[G_DTIME]=11;song.g[G_DFDBK]=56;
    project_save_as(0,"FIRST PHRASE");project_save_as(1,"SECOND PART");
    chain_config.count=3;chain_config.row[0]=chain_row_of(0,2);chain_config.row[1]=chain_row_of(1,1);chain_config.row[2]=chain_row_of(0,1);
    ko_pattern_refresh();ko_section=1;
    reset_page(0);draw_splash();save_raw("Boot","splash","Three-second splash");
    for(uint32_t k=0;k<3;k++)for(uint32_t v=0;v<29;v++)if(allowed(k,v)){
        char slug[96],label[128];reset_page(k);ko.view=(uint8_t)v;
        snprintf(slug,sizeof slug,"track-%u-view-%02u",k+1,v);snprintf(label,sizeof label,"%s / %s",k==1?"Drums":k==2?"Song 2":"Song 1",views[v]);
        capture(k==1?"Drums":k==2?"Song 2":"Song 1",slug,label);
    }
    for(uint32_t k=0;k<3;k++){
        char slug[80],label[100];reset_page(k);ko.view=7;ko_seq[k].end_mode=1;snprintf(slug,sizeof slug,"track-%u-sample-end",k+1);snprintf(label,sizeof label,"Track %u / sample-end timing",k+1);capture("States",slug,label);ko_seq[k].end_mode=0;
        reset_page(k);ko.view=7;song.rec=1u<<k;snprintf(slug,sizeof slug,"track-%u-armed",k+1);snprintf(label,sizeof label,"Track %u / armed step entry",k+1);capture("States",slug,label);
        reset_page(k);ko.view=7;song.playing=1;trk[k].seq_idx=6;snprintf(slug,sizeof slug,"track-%u-playing",k+1);snprintf(label,sizeof label,"Track %u / sequence playing",k+1);capture("States",slug,label);
    }
    const uint8_t layers[]={LAYER_GLO,LAYER_SEQ};
    const char *const layer_names[]={"Global controls","Sequencer tools"};
    for(uint32_t k=0;k<3;k++)for(uint32_t j=0;j<2;j++){
        char slug[80],label[100];reset_page(k);ko.view=0;ui.layer=layers[j];ui.lock=layers[j]==LAYER_FX?0:layers[j];
        snprintf(slug,sizeof slug,"track-%u-layer-%u",k+1,layers[j]);snprintf(label,sizeof label,"Track %u / %s",k+1,layer_names[j]);capture("Overlays",slug,label);
    }
    for(uint32_t tab=0;tab<MTAB_COUNT;tab++)for(uint32_t row=0;row<mtab_rows(tab);row++){
        char slug[80],label[120];reset_page(0);ui.menu=1;ui.menu_sel=(uint8_t)mtab_at(tab,row);mt.pos=(int16_t)(tab*MT_ONE);
        snprintf(slug,sizeof slug,"menu-%u-row-%02u",tab,row);snprintf(label,sizeof label,"%s / %s",MTAB_NAME[tab],MI_NAME[ui.menu_sel]);capture("Settings",slug,label);
    }
    reset_page(0);ui.menu=2;int maximum=menu_scroll_max();
    for(int scroll=0;;scroll+=MENU_DOC_H-24){char slug[80],label[100];ui.menu_scroll=scroll>maximum?maximum:scroll;ui.force=1;snprintf(slug,sizeof slug,"about-%04d",ui.menu_scroll);snprintf(label,sizeof label,"About / credits / scroll %u",ui.menu_scroll);capture("Settings",slug,label);if(scroll>=maximum)break;}
    reset_page(0);ui.menu=3;capture("Settings","system-info","System information");
    for(uint32_t kind=NK_PROJ_SAVE;kind<=NK_PROJ_RENAME;kind++)for(uint32_t numeric=0;numeric<2;numeric++){
        char slug[80],label[100];reset_page(0);name_open(kind,0);nm.num=(uint8_t)numeric;str_cpy(nm.s,"FIRST PHRASE",sizeof nm.s);nm.len=nm.cur=12;
        snprintf(slug,sizeof slug,"name-%u-%u",kind,numeric);snprintf(label,sizeof label,"Project %s / %s keyboard",kind==NK_PROJ_SAVE?"save":"rename",numeric?"numbers":"letters");capture("Dialogs",slug,label);
    }
    const uint8_t dialogs[]={CF_CLEAR_SEQ,CF_CLEAR_TRK,CF_OVR_PROJ,CF_LOAD_PAT,CF_DEL_ROW,CF_CLEAR_SONG,CF_CLEAR_MOTION};
    const char *const questions[]={"Clear sequence","Clear track","Overwrite project","Replace sequence","Delete arrangement section","Clear arrangement","Clear automation"};
    for(uint32_t j=0;j<7;j++){char slug[80];reset_page(0);confirm_open(dialogs[j],0);snprintf(slug,sizeof slug,"confirm-%u",dialogs[j]);capture("Dialogs",slug,questions[j]);}
    reset_page(0);cin_total=cin_left=4;ui.force=1;draw_countin();draw_head();save_raw("Boot","count-in-one-bar","Count-in / one bar");
    reset_page(0);cin_total=8;cin_left=4;ui.force=1;draw_countin();draw_head();save_raw("Boot","count-in-two-bars","Count-in / second bar of two");
    reset_page(0);ui.uboot=3;capture("Boot","update-countdown","Update-mode countdown");
    for(uint32_t k=0;k<NB;k++){char slug[80],label[100];reset_page(0);setup_title();setup_show("PRESS",B_NAME[k]);snprintf(slug,sizeof slug,"calibration-button-%02u",k);snprintf(label,sizeof label,"Calibration / press %s",B_NAME[k]);save_raw("Calibration",slug,label);}
    for(uint32_t k=0;k<NE;k++){char slug[80],label[100];reset_page(0);setup_title();setup_show("TURN RIGHT",E_NAME[k]);snprintf(slug,sizeof slug,"calibration-knob-%02u",k);snprintf(label,sizeof label,"Calibration / turn %s",E_NAME[k]);save_raw("Calibration",slug,label);}
    for(uint32_t k=0;k<3;k++){char slug[80],label[100];reset_page(k);ko.view=0;if(k==1)usr_nz[1]=0;else {slc_usr[k==2?2:0].len=0;usr_nz[k==2?2:0]=0;}
        snprintf(slug,sizeof slug,"empty-track-%u",k+1);snprintf(label,sizeof label,"%s / empty",k==1?"Drum kit":k==2?"Song 2":"Song 1");capture("Empty slots",slug,label);}
    return 0;
}
