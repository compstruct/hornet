
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "SIM_parameter.h"
#include "SIM_clock.h"
#include "SIM_array.h"
#include "SIM_static.h"
#include "SIM_time.h"
#include "SIM_util.h"
#include "SIM_link.h"

#if ( PARM(TECH_POINT) <= 90 )
double SIM_total_clockEnergy(SIM_router_info_t *info, SIM_router_power_t *router)
{
	double sram_buffer_clockcap = 0;
	double reg_buffer_clockcap = 0;
	double pipereg_clockcap = 0;
	double H_tree_clockcap = 0;
	double H_tree_resistance = 0;
	double ClockBufferCap = 0; 
	double ClockEnergy = 0;
	double Ctotal = 0;
	double bitline_cmetal = 0;
	double Cpre_charge = 0;
	double Cline = 0;
	double pmosLeakage = 0;
	double nmosLeakage = 0;

	double pre_size, bitline_len, n_bitline_pre;
	int ports;
	int rows;
	int pipeline_regs = 0;
	u_int n_gate;

 /*================Pipeline registers capacitive load on clock network ================*/
		
	if(info->pipelined){
	    /*pipeline registers after the link traversal stage */
    	    //pipeline_regs += info->n_total_in * info->flit_width;
    	    pipeline_regs += info->n_switch_out * info->flit_width;
            /*pipeline registers for input buffer*/
            if(info->in_buf_p)
               pipeline_regs += info->n_in_p * info->in_n_switch_p * 
                                info->flit_width * info->n_v_class_p;
            if(info->in_buf_b)
               pipeline_regs += info->n_in_b * info->in_n_switch_b * 
                                info->flit_width *info->n_v_class_b;
            /*pipeline registers for crossbar*/
	    if (info->crossbar_model){
	        if(info->out_share_switch_p)
    	    	   pipeline_regs += (info->n_switch_in_p + info->n_switch_in_p) * info->flit_width ;
	        if(info->out_share_switch_b)
    	    	   pipeline_regs += (info->n_switch_in_b + info->n_switch_in_b) * info->flit_width ;
        	else 
	           pipeline_regs += (info->n_out_p * info->n_v_channel_p * info->n_v_class_p +
                                     info->n_out_b * info->n_v_channel_b * info->n_v_class_b) * 
                                     info->flit_width;
             }
	        /*pipeline registers for output buffer*/
    	    if(info->out_buf_p)
               pipeline_regs += info->n_out_p * info->flit_width;
    	    if(info->out_buf_b)
               pipeline_regs += info->n_out_b * info->flit_width;
        	

			/*for vc allocator and sw allocator, the clock power has been included in the dynamic power part,
 			 * so we will not calculate them here to avoid duplication */   

	       pipereg_clockcap = pipeline_regs * fpfp_clock_cap();
           }

		/*========================H_tree wiring load ========================*/
		/* The 1e-6 factor is to convert the "router_diagonal" back to meters.
		   To be consistent we use micro-meters unit for our inputs, but 
		   the functions, internally, use meters. */

		if(info->H_tree_clock){
			if ((PARM(TRANSISTOR_TYPE) == HVT) || (PARM(TRANSISTOR_TYPE) == NVT)) {
				H_tree_clockcap = (4+4+2+2) * (info->router_diagonal * 1e-6) * (Clockwire);
				H_tree_resistance = (4+4+2+2) * (info->router_diagonal * 1e-6) * (Reswire);

				int k;
				double h;
				int *ptr_k = &k;
				double *ptr_h = &h;
				getOptBuffering(ptr_k, ptr_h, ((4+4+2+2) * (info->router_diagonal * 1e-6)));
				ClockBufferCap = ((double)k) * (ClockCap) * h;

				pmosLeakage = BufferPMOSOffCurrent * h * k * 15;
				nmosLeakage = BufferNMOSOffCurrent * h * k * 15;
			}
			else if(PARM(TRANSISTOR_TYPE) == LVT) {
				H_tree_clockcap = (8+4+4+4+4) * (info->router_diagonal * 1e-6)  * (Clockwire);
				H_tree_resistance = (8+4+4+4+4) * (info->router_diagonal * 1e-6) * (Reswire);

				int k;
				double h;
				int *ptr_k = &k;
				double *ptr_h = &h;
				getOptBuffering(ptr_k, ptr_h, ((8+4+4+4+4) * (info->router_diagonal * 1e-6)));
				ClockBufferCap = ((double)k) * (BufferInputCapacitance) * h;

                pmosLeakage = BufferPMOSOffCurrent * h * k * 29;
                nmosLeakage = BufferNMOSOffCurrent * h * k * 29;
			}
		}

		/* total dynamic energy for clock */
		Ctotal = pipereg_clockcap + H_tree_clockcap + ClockBufferCap;
		ClockEnergy =  Ctotal * EnergyFactor; 

		/* total leakage current for clock */
		/* Here we divide ((pmosLeakage + nmosLeakage) / 2) by SCALE_S is because pmosLeakage and nmosLeakage value is
		 * already for the specified technology, doesn't need to use scaling factor. So we divide SCALE_S here first since 
		 * the value will be multiplied by SCALE_S later */
		router->I_clock_static = (((pmosLeakage + nmosLeakage) / 2)/SCALE_S + (pipeline_regs * DFF_TAB[0]*Wdff));
		router->I_static += router->I_clock_static;

		return ClockEnergy;
}

double fpfp_clock_cap() 
{
	return ClockCap;
}

#else
/*=================clock power is not supported for 110nm and above=================*/
double SIM_total_clockEnergy(SIM_router_info_t *info, SIM_router_power_t *router)
{
    return 0;
}

double fpfp_clock_cap()
{
    return 0;
}

#endif

