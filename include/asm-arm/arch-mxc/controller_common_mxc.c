/*
 *  linux/drivers/video/eink/controller_common/controller_common_mxc.c --
 *  eInk common controller mxc related operations
 *
 *      Copyright (C) 2005-2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <asm/io.h>
#include <asm/arch/controller_common.h>

#include <asm/arch/ipu_regs.h>

// Static Globals
//
static void                   controller_eof_irq_completion(void);

static display_port_t         controller_disp;
static uint32_t               controller_pixel_fmt;
static bool                   ignore_hw_ready          = false;
static bool                   force_hw_not_ready       = false;
static bool                   dma_available            = false;
static bool                   dma_xfer_done            = true;
static uint32_t               dma_started;
static uint32_t               dma_stopped;
static int                    dma_failed               = 0;
static controller_common_get_dma_addr_t
                              controller_get_dma_addr  = NULL;
static controller_common_done_dma_addr_t
                              controller_done_dma_addr = NULL;

#define CONTROLLER_READY()    (ignore_hw_ready || (EINKFB_SUCCESS == controller_wait_for_ready()))
#define USE_WR_BUF(s)         (CONTROLLER_COMMON_CMD_ARGS_MAX < (s))
#define USE_RD_BUF(s)         (CONTROLLER_COMMON_RD_DAT_ONE != (s))

// Static Functions
//
static bool controller_hardware_ready(void *unused)
{
    return ( !force_hw_not_ready && controller_common_ready() );
}

// Interrupt function for completion of ADC buffer transfer.
// All we need to do is disable the channel and set the
// "done" flag.
//
static void controller_eof_irq_completion(void)
{
    ipu_disable_irq(IPU_IRQ_ADC_SYS1_EOF);
    ipu_disable_channel(ADC_SYS1, false);
    dma_xfer_done = true;
}

static irqreturn_t controller_eof_irq_handler(int irq, void *dev_id)
{
    controller_eof_irq_completion();    
    dma_stopped = jiffies;

    einkfb_debug("DMA transfer completed, time = %dms\n",
           jiffies_to_msecs(dma_stopped - dma_started));
    
    return ( IRQ_HANDLED );
}

static int controller_wr_which(bool which, uint32_t data)
{
    display_port_t disp = controller_disp;
    cmddata_t type = (CONTROLLER_COMMON_CMD == which) ? CMD : DAT;
    int result = EINKFB_FAILURE;
    
    if ( CONTROLLER_READY() )
        result = ipu_adc_write_cmd(disp, type, data, 0, 0);
    else
        result = EINKFB_FAILURE;
    
    return ( result );
}

static bool controller_dma_ready(void *unused)
{
    return ( dma_xfer_done );
}

static int controller_io_buf_dma(u32 data_size, dma_addr_t phys_addr)
{
    int result = EINKFB_FAILURE;

    if ( CONTROLLER_READY() )
    {
        // Set up for the DMA transfer.
        //
        if ( EINKFB_SUCCESS == ipu_init_channel_buffer(ADC_SYS1, IPU_INPUT_BUFFER, controller_pixel_fmt,
             CONTROLLER_COMMON_DMA_SIZE(data_size), IPU_ROTATE_NONE, phys_addr, 0, 0, 0) )
        {
            ipu_enable_irq(IPU_IRQ_ADC_SYS1_EOF);
            ipu_enable_channel(ADC_SYS1);
            dma_xfer_done = false;
            dma_started = jiffies;
            
            // Start it.
            //
            ipu_select_buffer(ADC_SYS1, IPU_INPUT_BUFFER, 0);
            einkfb_debug("DMA transfer started\n");
            
            // Wait for it to complete.
            //
            result = EINKFB_SCHEDULE_TIMEOUT(CONTROLLER_COMMON_DMA_TIMEOUT, controller_dma_ready);
            
            // If it fails to complete, complete it anyway.
            //
            if ( EINKFB_FAILURE == result )
            {
                controller_eof_irq_completion();
                dma_failed++;
                
                if ( CONTROLLER_COMMON_DMA_FAILED() )
                    einkfb_print_warn("DMA isn't working; falling back to PIO\n");
            }
            else
                dma_failed = 0;
        }
    }
    
    return ( result );
}

// Public Functions
//
bool controller_ignore_hw_ready(void) 
{
    return ( ignore_hw_ready );
}

void controller_set_ignore_hw_ready(bool value) 
{
    ignore_hw_ready = value;
}

bool controller_force_hw_not_ready(void) 
{
    return ( force_hw_not_ready );
}

void controller_set_force_hw_not_ready(bool value) 
{
    force_hw_not_ready = value;
}

int controller_wait_for_ready(void)
{
    return ( EINKFB_SCHEDULE_TIMEOUT(CONTROLLER_COMMON_RDY_TIMEOUT, controller_hardware_ready) );
}

int controller_wr_cmd(uint32_t cmd, bool poll)
{
    int result;
    
    einkfb_debug_full("command = 0x%04X, poll = %d\n", cmd, poll);
    result = controller_wr_which(CONTROLLER_COMMON_CMD, (uint32_t)cmd);
    
    if ( (EINKFB_SUCCESS == result) && poll )
    {
        if ( !CONTROLLER_READY() )
           result = EINKFB_FAILURE; 
    }

    return ( result );
}

// Scheduled loop for doing IO on framebuffer-sized buffers.
//
int controller_io_buf(u32 data_size, u16 *data, bool which)
{
    display_port_t disp = controller_disp;
    int result = EINKFB_FAILURE;

    einkfb_debug_full("size    = %d\n", data_size);

    if ( CONTROLLER_READY() )
    {
        int     i = 0, j, length = (EINKFB_MEMCPY_MIN >> 1), num_loops = data_size/length,
                remainder = data_size % length;
        bool    done = false;
        
        if ( 0 != num_loops )
            einkfb_debug("num_loops @ %d bytes = %d, remainder = %d\n",
                   (length << 1), num_loops, (remainder << 1));
        
        result = EINKFB_SUCCESS;
        
        // Read/write EINKFB_MEMCPY_MIN bytes (hence, divide by 2) at a time.  While
        // there are still bytes to read/write, yield the CPU.
        //
        do
        {
            if ( 0 >= num_loops )
                length = remainder;

            for ( j = 0; j < length; j++)
            {
                if ( CONTROLLER_COMMON_WR == which )
                    ipu_adc_write_cmd(disp, DAT, (uint32_t)data[i + j], 0, 0);
                else
                    data[i + j] = (u16)(ipu_adc_read_data(disp) & 0x0000FFFF);
            }
                
            i += length;
            
            if ( i < data_size )
            {
                EINKFB_SCHEDULE();
                num_loops--;
            }
            else
                done = true;
        }
        while ( !done );
    }

    return ( result );
}

bool controller_wr_one_ready(void)
{
    bool ready = false;
    
    if ( CONTROLLER_READY() )
        ready = true;
    
    return ( ready );
}

void controller_wr_one(u16 data)
{
    ipu_adc_write_cmd((display_port_t)controller_disp, DAT, (uint32_t)data, 0, 0);
}

int controller_wr_dat(bool which, u32 data_size, u16 *data)
{
    int result = EINKFB_FAILURE;
    
    if ( USE_WR_BUF(data_size) )
    {
        u16 *pio_data = data;
        
        if ( dma_available && !CONTROLLER_COMMON_DMA_FAILED() )
        {
            dma_addr_t phys_addr = controller_get_dma_addr ? controller_get_dma_addr() : 0;
            
            if ( phys_addr )
            {
                u32 alignment = 0, align_size = 0, remainder = 0;
                
                // Ensure that we're doing enough DMA to warrant it.
                //
                if ( CONTROLLER_COMMON_DMA_MIN_SIZE <= data_size )
                {
                    alignment  = data_size / CONTROLLER_COMMON_DMA_MIN_SIZE;
                    
                    // Ensure that the DMA we're doing is properly aligned.
                    //
                    align_size = alignment * CONTROLLER_COMMON_DMA_MIN_SIZE;
                    remainder  = data_size % CONTROLLER_COMMON_DMA_MIN_SIZE;
                }
                
                einkfb_debug("DMA alignment = %d: align_size = %d, remainder = %d\n",
                       alignment, align_size, remainder);

                // Send the properly sized and aligned data along to be DMA'd.
                //
                if ( align_size )
                    result = controller_io_buf_dma(align_size, phys_addr);
                
                // If the DMA went well, only PIO the remainder if there is any.
                //
                if ( EINKFB_SUCCESS == result )
                {
                    data_size = remainder;
                    
                    if ( remainder )
                    {
                        pio_data = &data[alignment * CONTROLLER_COMMON_DMA_MIN_SIZE];
                        result = EINKFB_FAILURE;
                    }
                }

                if ( controller_done_dma_addr )
                    controller_done_dma_addr();
            }
        }

        if ( EINKFB_FAILURE == result )
            result = controller_io_buf(data_size, pio_data, CONTROLLER_COMMON_WR);
    }
    else
    {
        int i; result = EINKFB_SUCCESS;
        
        for ( i = 0; (i < data_size) && (EINKFB_SUCCESS == result); i++ )
        {
            if ( CONTROLLER_COMMON_WR_DAT_DATA == which )
            {
                if ( EINKFB_DEBUG() && (9 > i) )
                    einkfb_debug_full("data[%d] = 0x%04X\n", i, data[i]);
            }
            else
                einkfb_debug_full("args[%d] = 0x%04X\n", i, data[i]);
            
            result = controller_wr_which(CONTROLLER_COMMON_DAT, (uint32_t)data[i]);
        }
    }
    
    return ( result );
}

int controller_rd_one(u16 *data)
{
    display_port_t disp = controller_disp;
    int result = EINKFB_SUCCESS;
    
    if ( CONTROLLER_READY() )
        *data = (u16)(ipu_adc_read_data(disp) & 0x0000FFFF);
    else
        result = EINKFB_FAILURE;
    
    if ( EINKFB_SUCCESS == result )
        einkfb_debug_full("data    = 0x%04X\n", *data);
    
    return ( result );
}

int controller_rd_dat(u32 data_size, u16 *data)
{
    int result = EINKFB_FAILURE;

    // For single-word reads, don't use the buffer call.
    //
    if ( !USE_RD_BUF(data_size) )
         result = controller_rd_one(data);
    else
    {
        einkfb_debug_full("size    = %d\n", data_size);

        if ( CONTROLLER_READY() )
            result = controller_io_buf(data_size, data, CONTROLLER_COMMON_RD);
    }
    
    return ( result );
}

// Controller hardware interface and reset
//
bool controller_hw_init(bool use_dma, controller_properties_t *prop)
{
    ipu_channel_params_t params;
    bool result = false;

    ipu_adc_sig_cfg_t sig = { 0, 0, 0, 0, 0, 0, 0, 0,
            IPU_ADC_BURST_WCS,
            IPU_ADC_IFC_MODE_SYS80_TYPE2,
            16, 0, 0, IPU_ADC_SER_NO_RW
    };

    // Set up ADC IRQ.
    //
    if (use_dma)
    {
        if (ipu_request_irq(IPU_IRQ_ADC_SYS1_EOF, controller_eof_irq_handler, 0, CONTROLLER_COMMON_NAME"_controller_ipu_irq", NULL) )
        {
            einkfb_print_crit("Could not register SYS1 irq handler\n");
            dma_available = false;
        }
        else
        {
            controller_eof_irq_completion();
            dma_available = true;
        }
    }

    // first set the DI_HSP_CLK_PER to the right frequency
    __raw_writel(prop->hsp_clk_per, DI_HSP_CLK_PER);
    
    memset(&params, 0, sizeof(params));
    params.adc_sys1.disp = prop->controller_disp;
    params.adc_sys1.ch_mode = WriteDataWoRS;
    ipu_init_channel(ADC_SYS1, &params);

    ipu_adc_init_panel(prop->controller_disp,
                       prop->screen_width,
                       prop->screen_height,
                       prop->pixel_fmt,
                       prop->screen_stride,
                       sig, XY, 0, VsyncInternal);

    // Set IPU timing for read cycles.
    //
    ipu_adc_init_ifc_timing(prop->controller_disp, true,
                            prop->read_cycle_time,
                            prop->read_up_time,
                            prop->read_down_time,
                            prop->read_latch_time,
                            prop->pixel_clk);
                            
    // Set IPU timing for write cycles.
    //
    ipu_adc_init_ifc_timing(prop->controller_disp, false,
                            prop->write_cycle_time,
                            prop->write_up_time,
                            prop->write_down_time,
                            0, 0);

    ipu_adc_set_update_mode(ADC_SYS1, IPU_ADC_REFRESH_NONE, 0, 0, 0);

    switch ( controller_common_gpio_config(NULL, NULL) )
    {
        case CONTROLLER_COMMON_GPIO_INIT_SUCCESS:
            einkfb_debug("Sending RST signal to controller...\n");
            controller_common_reset();
            einkfb_debug("Controller reset done.\n");
            
            einkfb_debug("GPIOs and IRQ set; controller has been reset\n");
            result = true;
        break;

        case CONTROLLER_COMMON_HRST_INIT_FAILURE:
            einkfb_print_crit("Could not obtain GPIO pin for HRST\n");
        break;
        
        case CONTROLLER_COMMON_HRDY_INIT_FAILURE:
            einkfb_print_crit("Could not obtain GPIO pin for HRDY\n");
        break;
    }

    controller_disp = prop->controller_disp;
    controller_pixel_fmt = prop->pixel_fmt;
    controller_get_dma_addr = prop->get_dma_phys_addr;
    controller_done_dma_addr = prop->done_dma_phys_addr;

    return ( result );
}

void controller_hw_done(void)
{
    if ( dma_available )
    {
        controller_eof_irq_completion();
        ipu_free_irq(IPU_IRQ_ADC_SYS1_EOF, NULL);
    }

    ipu_uninit_channel(ADC_SYS1);
    controller_common_gpio_disable(0);

    einkfb_debug("Released controller GPIO pins and IRQs\n");
}


