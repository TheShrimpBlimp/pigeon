/* SPDX-License-Identifier: GPL-3.0-only */
/* Focused ownership regressions independent of old synth menu expectations. */
#define MIDI_CONTROL_TEST_MAIN midi_control_full_main
#include "midi_control_test.c"
int main(void)
{
    int bad=packed_ownership_test()+controls_test()+sustain_test()+ownership_test()+usb_burst_test();
    printf("Memory ownership regressions: %s\n",bad?"FAIL":"PASS");return bad!=0;
}
