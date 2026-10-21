/* SPDX-License-Identifier: GPL-3.0-only */
/* Inlined into the RAM flash-program wait, with IRQs masked and XIP disabled.
 * Do not call the normal SIE helpers: their timeout path lives in flash.
 * A stuck controller invalidates the take instead of blocking flash recovery. */
FM1_INLINE int usb_sample_flash_sie(uint32_t reg, uint32_t value, int read)
{
    if(read) fm1_usb_sie_rd_start(reg);
    else fm1_usb_sie_wr_start(reg,value);
    for(uint32_t n=0;n<20000u;n++)
        if(fm1_usb_sie_done()) return (int)fm1_usb_sie_data();
    usb_sample.fault=4;usb_sample.armed=0;
    return -1;
}
FM1_INLINE void usb_sample_flash_poll(void)
{
    if(!usb_sample.armed || !fm1_usb_sie_on())return;
    if(usb_sample_flash_sie(S_INDEX,4,0)<0)return;
    int rx=usb_sample_flash_sie(S_RXCSR1,0,1);
    if(rx<0 || !(rx&1))return;
    int bytes=usb_sample_flash_sie(S_RXCOUNT1,0,1);
    if(bytes<0)return;
    fm1_usb_rx_sync();
    if(rx&4)usb_sample.fault=4;
    usb_sample_take(usb_sample_packet,(uint32_t)bytes);
    usb_sample_flash_sie(S_RXCSR1,usb_sample_ack((uint32_t)rx),0);
}
