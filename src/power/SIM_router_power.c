/*
 * TODO:  (2) add routing table
 *
 * FIXME: (1) ignore internal nodes of tri-state buffer till we find a good solution
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "SIM_parameter.h"
#include "SIM_array.h"
#include "SIM_misc.h"
#include "SIM_router.h"
#include "SIM_static.h"
#include "SIM_clock.h"
#include "SIM_util.h"

/* FIXME: request wire length estimation is ad hoc */
int SIM_router_power_init(SIM_router_info_t *info, SIM_router_power_t *router)
{
	double cbuf_width, req_len = 0;

	router->I_static = 0;
	router->I_in_buf_static_p = 0;
	router->I_out_buf_static_p = 0;
	router->I_crossbar_static = 0;
	router->I_vc_arbiter_static_p = 0;
	router->I_sw_arbiter_static_p = 0;

	router->I_in_buf_static_b = 0;
	router->I_out_buf_static_b = 0;
	router->I_vc_arbiter_static_b = 0;
	router->I_sw_arbiter_static_b = 0;
	/* initialize crossbar */
	if (info->crossbar_model) {
		SIM_crossbar_init(&router->crossbar, info->crossbar_model, 
                                  info->n_switch_in, info->n_switch_out, 
                                  info->xb_in_seg, info->xb_out_seg, 
                                  info->flit_width, info->degree, 
                                  info->connect_type, info->trans_type, 
                                  info->crossbar_in_len, info->crossbar_out_len, &req_len);
		/* static power */
		router->I_crossbar_static = router->crossbar.I_static;
		router->I_static += router->I_crossbar_static;
	}

	/* HACK HACK HACK */
	if (info->exp_xb_model)
		SIM_crossbar_init(&router->exp_xb, info->exp_xb_model, 2 * info->n_switch_in - 1, 2 * info->n_switch_out - 1, info->exp_in_seg, info->exp_out_seg, info->flit_width, info->degree, info->connect_type, info->trans_type, 0, 0, NULL);

	/* initialize various buffers */
	if (info->in_buf_p) {
		SIM_array_power_init(&info->in_buf_info_p, &router->in_buf_p);
		/* static power */
		//router->I_buf_static = router->in_buf.I_static * info->n_in * info->n_v_class * (info->in_share_buf ? 1 : info->n_v_channel);
		router->I_in_buf_static_p = router->in_buf_p.I_static ;
	}
	if (info->in_buf_b) {
		SIM_array_power_init(&info->in_buf_info_b, &router->in_buf_b);
		router->I_in_buf_static_b = router->in_buf_b.I_static ;
	}
	if (info->cache_in_buf)
		SIM_array_power_init(&info->cache_in_buf_info, &router->cache_in_buf);
	if (info->mc_in_buf)
		SIM_array_power_init(&info->mc_in_buf_info, &router->mc_in_buf);
	if (info->io_in_buf)
		SIM_array_power_init(&info->io_in_buf_info, &router->io_in_buf);
	if (info->out_buf_p){
		SIM_array_power_init(&info->out_buf_info_p, &router->out_buf_p);
		/* static power */
		//router->I_buf_static += router->out_buf.I_static * info->n_out * info->n_v_class * (info->out_share_buf ? 1 : info->n_v_channel);
		router->I_out_buf_static_p += router->out_buf_p.I_static;
	}
	if (info->out_buf_p){
		SIM_array_power_init(&info->out_buf_info_p, &router->out_buf_p);
	}

	//router->I_static += router->I_buf_static; 
        double I_in_buf_p = router->I_in_buf_static_p * info->n_in_p * info->n_v_class_p
                                    *(info->in_share_buf_p ? 1 : info->n_v_channel_p)
                          + router->I_out_buf_static_p * info->n_out_p * info->n_v_class_p
                                    *(info->out_share_buf_p ? 1 : info->n_v_channel_p);

        double I_in_buf_b = router->I_in_buf_static_b * info->n_in_b * info->n_v_class_b
                                    *(info->in_share_buf_b ? 1 : info->n_v_channel_b)
                          + router->I_out_buf_static_b * info->n_out_b * info->n_v_class_b
                                    *(info->out_share_buf_b ? 1 : info->n_v_channel_b);
	router->I_static += I_in_buf_p + I_in_buf_b;

	if (info->central_buf) {
		/* MUST initialize array before crossbar because the latter needs array width */
		SIM_array_power_init(&info->central_buf_info, &router->central_buf);

		/* WHS: use MATRIX_CROSSBAR, info->connect_type, info->trans_type rather than specifying new parameters */
		cbuf_width = info->central_buf_info.data_arr_width + info->central_buf_info.tag_arr_width;
		req_len = info->central_buf_info.data_arr_height;

		/* assuming no segmentation for central buffer in/out crossbars */
		SIM_crossbar_init(&router->in_cbuf_crsbar, MATRIX_CROSSBAR, info->n_switch_in, info->pipe_depth * info->central_buf_info.write_ports, 0, 0, info->flit_width, 0, info->connect_type, info->trans_type, cbuf_width, 0, NULL);
		SIM_crossbar_init(&router->out_cbuf_crsbar, MATRIX_CROSSBAR, info->pipe_depth * info->central_buf_info.read_ports, info->n_switch_out, 0, 0, info->flit_width, 0, info->connect_type, info->trans_type, 0, cbuf_width, NULL);

		/* dirty hack */
		SIM_fpfp_init(&router->cbuf_ff, info->cbuf_ff_model, 0);
	}

	/* initialize port switch allocator arbiter */
	if (info->sw_in_arb_model_p) {
		SIM_arbiter_init(&router->sw_in_arb_p, info->sw_in_arb_model_p, 
                                 info->sw_in_arb_ff_model_p, 
                                 info->n_v_channel_p*info->n_v_class_p, 0, 
                                 &info->sw_in_arb_queue_info_p);
		router->I_sw_arbiter_static_p = router->sw_in_arb_p.I_static * 
                                                info->in_n_switch_p * info->n_in_p;
	}

	/* initialize bridge switch allocator arbiter */
	if (info->sw_in_arb_model_b) {
		SIM_arbiter_init(&router->sw_in_arb_b, info->sw_in_arb_model_b,
                                 info->sw_in_arb_ff_model_b,
                                 info->n_v_channel_b*info->n_v_class_b, 0,
                                 &info->sw_in_arb_queue_info_b);
		router->I_sw_arbiter_static_b = router->sw_in_arb_b.I_static * 
                                                info->in_n_switch_b * info->n_in_b;
	}
	/* WHS: must after switch initialization */
	if (info->sw_out_arb_model_p) {
		SIM_arbiter_init(&router->sw_out_arb_p, info->sw_out_arb_model_p, 
                                 info->sw_out_arb_ff_model_p, 
                                 (info->n_total_in - info->n_in_b) * (info->in_n_switch_p), 
                                 req_len, &info->sw_out_arb_queue_info_p);
		router->I_sw_arbiter_static_p += router->sw_out_arb_p.I_static * 
                                                 info->n_switch_out_p;
	}
	if (info->sw_out_arb_model_b) {
		SIM_arbiter_init(&router->sw_out_arb_b, info->sw_out_arb_model_b, 
                                 info->sw_out_arb_ff_model_b, 
                                 (info->n_total_in - info->n_in_p) * (info->in_n_switch_b), 
                                 req_len, &info->sw_out_arb_queue_info_b);
		router->I_sw_arbiter_static_b += router->sw_out_arb_b.I_static * 
                                                 info->n_switch_out_b;
	}
	/*static energy*/ 
	router->I_static += router->I_sw_arbiter_static_p + router->I_sw_arbiter_static_b;

	/* initialize port virtual channel allocator arbiter */
	if(info->vc_allocator_type_p == ONE_STAGE_ARB && 
           info->n_v_channel_p > 1 && (info->n_in_p + info->n_in_b) > 1){
		if (info->vc_out_arb_model_p)
		{
			SIM_arbiter_init(&router->vc_out_arb_p, info->vc_out_arb_model_p, 
                                         info->vc_out_arb_ff_model_p,
					 (info->n_total_in - info->n_in_b) * info->n_v_channel_p,
                                         0, &info->vc_out_arb_queue_info_p);

			router->I_vc_arbiter_static_p = router->vc_out_arb_p.I_static * 
                                info->n_out_p * info->n_v_channel_p * info->n_v_class_p;
		}
		else fprintf (stderr, "error in setting vc allocator parameters\n");
	} else if(info->vc_allocator_type_p == TWO_STAGE_ARB && 
                  info->n_v_channel_p > 1 && info->n_in_p > 1) {
		  if (info->vc_in_arb_model_p && info->vc_out_arb_model_p){
			// first stage
			SIM_arbiter_init(&router->vc_in_arb_p, info->vc_in_arb_model_p, 
                                         info->vc_in_arb_ff_model_p,
					 info->n_v_channel_p, 0, &info->vc_in_arb_queue_info_p);
			router->I_vc_arbiter_static_p = router->vc_in_arb_p.I_static * 
                                info->n_in_p * info->n_v_channel_p * info->n_v_class_p;
			//second stage
			SIM_arbiter_init(&router->vc_out_arb_p, info->vc_out_arb_model_p, 
                                         info->vc_out_arb_ff_model_p,
					 (info->n_in_p - 1) * info->n_v_channel_p + 
                                         info->n_in_b * info->n_v_channel_b,
                                         0, &info->vc_out_arb_queue_info_p);
			router->I_vc_arbiter_static_p += router->vc_out_arb_p.I_static * 
                                info->n_out_p * info->n_v_channel_p * info->n_v_class_p;
		   }	
		else fprintf (stderr, "error in setting vc allocator parameters\n");
	}
	else if(info->vc_allocator_type_p == VC_SELECT && info->n_v_channel_p > 1) {
		SIM_array_power_init(&info->vc_select_buf_info_p, &router->vc_select_buf_p);
		/* static power */
		router->I_vc_arbiter_static_p = router->vc_select_buf_p.I_static * 
                                                info->n_out_p * info->n_v_class_p;
	}
	/* initialize bridge virtual channel allocator arbiter */
	if(info->vc_allocator_type_b == ONE_STAGE_ARB && 
           info->n_v_channel_b > 1 && info->n_in_b > 1){
		if (info->vc_out_arb_model_b)
		{
			SIM_arbiter_init(&router->vc_out_arb_b, info->vc_out_arb_model_b, 
                                         info->vc_out_arb_ff_model_b,
					 info->n_in_b * info->n_v_channel_b,
                                         0, &info->vc_out_arb_queue_info_b);

			router->I_vc_arbiter_static_b = router->vc_out_arb_b.I_static * 
                                info->n_out_b * info->n_v_channel_b * info->n_v_class_b;
		}
		else fprintf (stderr, "error in setting vc allocator parameters\n");
	} else if(info->vc_allocator_type_b == TWO_STAGE_ARB && 
                  info->n_v_channel_b > 1 && (info->n_in_b + info->n_in_p) > 1) {
		  if (info->vc_in_arb_model_b && info->vc_out_arb_model_b){
			// first stage
			SIM_arbiter_init(&router->vc_in_arb_b, info->vc_in_arb_model_b, 
                                         info->vc_in_arb_ff_model_b,
					 info->n_v_channel_b, 0, &info->vc_in_arb_queue_info_b);
			router->I_vc_arbiter_static_b = router->vc_in_arb_b.I_static * 
                                info->n_in_b * info->n_v_channel_b * info->n_v_class_b;
			//second stage
			SIM_arbiter_init(&router->vc_out_arb_b, info->vc_out_arb_model_b, 
                                         info->vc_out_arb_ff_model_b,
					 info->n_in_p * info->n_v_channel_p,
                                         0, &info->vc_out_arb_queue_info_b);
			router->I_vc_arbiter_static_b += router->vc_out_arb_b.I_static * 
                                info->n_out_b * info->n_v_channel_b * info->n_v_class_b;
		   }	
		else fprintf (stderr, "error in setting vc allocator parameters\n");
	}
	else if(info->vc_allocator_type_b == VC_SELECT && info->n_v_channel_b > 1) {
		SIM_array_power_init(&info->vc_select_buf_info_b, &router->vc_select_buf_b);
		/* static power */
		router->I_vc_arbiter_static_b = router->vc_select_buf_b.I_static * 
                                                info->n_out_b * info->n_v_class_b;
	}
	router->I_static += router->I_vc_arbiter_static_p + router->I_vc_arbiter_static_b;
	return 0;
}

/* THIS FUNCTION IS OBSOLETE */
int SIM_router_power_report(SIM_router_info_t *info, SIM_router_power_t *router)
{
	double epart, etotal = 0;

	if (info->crossbar_model) {
		epart = SIM_crossbar_report(&router->crossbar);
		fprintf(stderr, "switch: %g\n", epart);
		etotal += epart;
	}

	fprintf(stderr, "total energy: %g\n", etotal);

	return 0;
}

