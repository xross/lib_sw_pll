// Copyright 2022-2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <xscope.h>
#include <xs1.h>
#include <platform.h>

#include <xcore/select.h>

#include "sw_pll.h"
#include "resource_setup.h"

#define MCLK_FREQUENCY              24576000
#define REF_FREQUENCY               48000
#define PLL_RATIO                   (MCLK_FREQUENCY / REF_FREQUENCY)
#define CONTROL_LOOP_COUNT          512
#define PPM_RANGE                   150 //TODO eliminate

#include "register_setup.h"

void sdm_task(chanend_t c_sdm_control){
    printf("sdm_task\n");

    const uint32_t sdm_interval = 100;

    sw_pll_sdm_state_t sdm_state;
    init_sigma_delta(&sdm_state);

    tileref_t this_tile = get_local_tile_id();

    hwtimer_t tmr = hwtimer_alloc();
    int32_t trigger_time = hwtimer_get_time(tmr) + sdm_interval;
    bool running = true;
    int32_t ds_in = 666666;

    while(running){
        // Poll for new SDM control value
        SELECT_RES(
            CASE_THEN(c_sdm_control, ctrl_update),
            DEFAULT_THEN(default_handler)
        )
        {
            ctrl_update:
            {
                ds_in = chan_in_word(c_sdm_control);
            }
            break;

            default_handler:
            {
                // Do nothing & fall-through
            }
            break;
        }

        // calc new ds_out and then wait to write
        int32_t ds_out = do_sigma_delta(&sdm_state, ds_in);
        uint32_t frac_val = ds_out_to_frac_reg(ds_out);

        hwtimer_wait_until(tmr, trigger_time);
        trigger_time += sdm_interval;
        write_frac_reg(this_tile, frac_val);

        static int cnt = 0;
        if (cnt % 1000000 == 0) printintln(cnt);
        cnt++;
    }
}


void sw_pll_sdm_test(chanend_t c_sdm_control){

    // Declare mclk and refclk resources and connect up
    port_t p_mclk = PORT_MCLK_IN;
    xclock_t clk_mclk = XS1_CLKBLK_1;
    port_t p_ref_clk = PORT_I2S_LRCLK;
    xclock_t clk_word_clk = XS1_CLKBLK_2;
    port_t p_ref_clk_count = XS1_PORT_32A;
    setup_ref_and_mclk_ports_and_clocks(p_mclk, clk_mclk, p_ref_clk, clk_word_clk, p_ref_clk_count);

    // Make a test output to observe the recovered mclk divided down to the refclk frequency
    xclock_t clk_recovered_ref_clk = XS1_CLKBLK_3;
    port_t p_recovered_ref_clk = PORT_I2S_DAC_DATA;
    setup_recovered_ref_clock_output(p_recovered_ref_clk, clk_recovered_ref_clk, p_mclk, PLL_RATIO / 2); // TODO fix me /2
    
    sw_pll_state_t sw_pll;
    sw_pll_sdm_init(&sw_pll,
                SW_PLL_15Q16(0.0),
                SW_PLL_15Q16(32.0),
                CONTROL_LOOP_COUNT,
                PLL_RATIO,
                0,
                APP_PLL_CTL_REG,
                APP_PLL_DIV_REG,
                APP_PLL_FRAC_REG,
                PPM_RANGE);

    sw_pll_lock_status_t lock_status = SW_PLL_LOCKED;

    uint32_t max_time = 0;
    while(1)
    {
        port_in(p_ref_clk_count);   // This blocks each time round the loop until it can sample input (rising edges of word clock). So we know the count will be +1 each time.
        uint16_t mclk_pt =  port_get_trigger_time(p_ref_clk);// Get the port timer val from p_ref_clk (which is running from MCLK). So this is basically a 16 bit free running counter running from MCLK.
        
        uint32_t t0 = get_reference_time();
        sw_pll_sdm_do_control(&sw_pll, c_sdm_control, mclk_pt, 0);
        uint32_t t1 = get_reference_time();
        if(t1 - t0 > max_time){
            max_time = t1 - t0;
            printf("Max ticks taken: %lu\n", max_time);
        }

        if(sw_pll.lock_status != lock_status){
            lock_status = sw_pll.lock_status;
            const char msg[3][16] = {"UNLOCKED LOW\0", "LOCKED\0", "UNLOCKED HIGH\0"};
            printf("%s\n", msg[lock_status+1]);
        }
    }
}
