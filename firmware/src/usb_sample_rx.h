/* SPDX-License-Identifier: GPL-3.0-only */
/* USB OUT is host-clocked mono PCM16 at 22050 Hz. IRQ produces, main loop consumes.
 * Never wrap over unread samples: one overrun invalidates the take. */
#ifndef USB_SAMPLE_POOL
#define USB_SAMPLE_POOL __attribute__((section(".pool")))
#endif
#define USB_SAMPLE_N 2048u
static int16_t usb_sample_pcm[USB_SAMPLE_N] USB_SAMPLE_POOL;
static uint8_t usb_sample_packet[64+4] __attribute__((aligned(4))) USB_SAMPLE_POOL;
static struct {
    volatile uint32_t write, read, fault, frames, peak;
    volatile uint8_t armed, alt;
} usb_sample;
static void usb_sample_reset(uint32_t alt)
{
    if (usb_sample.armed) usb_sample.fault=1;
    usb_sample.armed=0; usb_sample.alt=(uint8_t)alt;
}
static inline __attribute__((always_inline)) void usb_sample_take(const uint8_t *data,uint32_t bytes)
{
    if (bytes>46u || (bytes&1u)) { if(usb_sample.armed)usb_sample.fault=2; return; }
    uint32_t n=bytes/2u,w=usb_sample.write,r=usb_sample.read;
    if(usb_sample.armed && n>USB_SAMPLE_N-(w-r)){usb_sample.fault=3;usb_sample.armed=0;return;}
    for(uint32_t i=0;i<n;i++) {
        int16_t value=(int16_t)((uint32_t)data[i*2]|(uint32_t)data[i*2+1]<<8);
        uint32_t level=value<0?-(int32_t)value:value;
        if(level>usb_sample.peak)usb_sample.peak=level;
        if(usb_sample.armed)usb_sample_pcm[(w+i)&(USB_SAMPLE_N-1u)]=value;
    }
    if(usb_sample.armed){RING_PUBLISH();usb_sample.write=w+n;}
    usb_sample.frames+=n;
}
static int usb_sample_begin(void)
{
    if(!usb_sample.alt)return 0;
    usb_sample.armed=0;usb_sample.write=usb_sample.read=0;usb_sample.fault=0;usb_sample.peak=0;
    RING_PUBLISH();usb_sample.armed=1;return 1;
}

/* Match the controller's existing OUT DMA rearm sequence, preserving ISO mode.
 * Merely clearing RxPktRdy does not flush/rearm its receive FIFO. */
static inline __attribute__((always_inline)) uint32_t usb_sample_ack(uint32_t csr){return (csr&~0x164u)|0x10u;}
