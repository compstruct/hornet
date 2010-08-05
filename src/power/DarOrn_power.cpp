#include <iostream>
#include "DarOrn_power.hpp"

extern "C" {
#include "SIM_util.h"
#include "SIM_clock.h"
}

using namespace std;

void Dar_router_init(SIM_router_info_t *info, 
                     SIM_router_power_t *router_power,
                     system_parameter_d system_parameter
                    ) {

        u_int line_width;
        int outdrv;
   
        /* PHASE1 : set parameters */
        /* general parameters */
        info->n_in_p = system_parameter.input_ports;
        info->n_in_b = system_parameter.bridge_in;
        info->n_total_in = info->n_in_p + info->n_in_b;
        info->n_out_p = system_parameter.output_ports;
        info->n_out_b = system_parameter.bridge_out;
        info->n_total_out = info->n_out_p + info->n_out_b;
        info->flit_width = PARM(flit_width);
   
        /* virtual channel parameters */
        info->n_v_class_p = PARM(v_class_p);
        info->n_v_channel_p = system_parameter.vq_per_port;
        int  n_channel_sum_p = info->n_v_class_p * info->n_v_channel_p;
        if (info->n_v_channel_p > 1) info->vc_allocator_type_p = PARM(vc_allocator_type_p);
        else  info->vc_allocator_type_p = SIM_NO_MODEL;
        if (n_channel_sum_p > 1) {
           info->in_share_buf_p = PARM(in_share_buf_p);
           info->out_share_buf_p = PARM(out_share_buf_p);
           info->in_share_switch_p = PARM(in_share_switch_p);
           info->out_share_switch_p = PARM(out_share_switch_p);
        } else {
           info->in_share_buf_p = 0;
           info->out_share_buf_p = 0;
           info->in_share_switch_p = 0;
           info->out_share_switch_p = 0;
        }
        /* bridge virtual channel parameters */
        info->n_v_class_b = PARM(v_class_b);
        info->n_v_channel_b = system_parameter.vq_per_bridge;
        int  n_channel_sum_b = info->n_v_class_b * info->n_v_channel_b;
        if (info->n_v_channel_b > 1) info->vc_allocator_type_b 
                                              = PARM(vc_allocator_type_b);
        else  info->vc_allocator_type_b = SIM_NO_MODEL;
        if(n_channel_sum_b > 1) {
           info->in_share_buf_b = PARM(in_share_buf_b);
           info->out_share_buf_b = PARM(out_share_buf_b);
           info->in_share_switch_b = PARM(in_share_switch_b);
           info->out_share_switch_b = PARM(out_share_switch_b);
        } else {
           info->in_share_buf_b = 0;
           info->out_share_buf_b = 0;
           info->in_share_switch_b = 0;
           info->out_share_switch_b = 0;
        }

        /* crossbar */
        info->crossbar_model = PARM(crossbar_model); 
        info->degree         = PARM(crsbar_degree);
        info->connect_type   = PARM(connect_type);
        info->trans_type     = PARM(trans_type);
        info->xb_in_seg      = PARM(xb_in_seg);
        info->xb_out_seg     = PARM(xb_out_seg);
        info->crossbar_in_len  = PARM(crossbar_in_len);
        info->crossbar_out_len = PARM(crossbar_out_len);
   
        info->exp_xb_model = PARM(exp_xb_model);
        info->exp_in_seg   = PARM(exp_in_seg);
        info->exp_out_seg  = PARM(exp_out_seg);

        /* input port buffer */
        info->in_buf_p = PARM(in_buf_p);
        info->in_buf_set_p = system_parameter.flits_per_port_vq;
        info->in_buf_rport_p = PARM(in_buf_rport_p);
        info->in_buffer_model_p = PARM(in_buffer_type_p);
        if(info->in_buf_p) {
           outdrv = !info->in_share_buf_p && info->in_share_switch_p && 
                           (system_parameter.port_bw2xbar != 4);
           SIM_array_init(&info->in_buf_info_p, 1, info->in_buf_rport_p, 1, info->in_buf_set_p, 
                          info->flit_width, outdrv, info->in_buffer_model_p);
        }

        /* input bridge buffer */ 
        info->in_buf_b = PARM(in_buf_b);
        info->in_buf_set_b = system_parameter.flits_per_bridge_vq;
        info->in_buf_rport_b = PARM(in_buf_rport_b);
        info->in_buffer_model_b = PARM(in_buffer_type_b);
        if(info->in_buf_b) {
           outdrv = !info->in_share_buf_b && info->in_share_switch_b && 
                           (system_parameter.bridge_bw2xbar != 4);
           SIM_array_init(&info->in_buf_info_b, 1, info->in_buf_rport_b, 
                          1, info->in_buf_set_b, info->flit_width, 
                          outdrv, info->in_buffer_model_b);
        }

        /* output port buffer */
        info->out_buf_p = PARM(out_buf_p);
        info->out_buf_set_p = PARM(out_buf_set_p);
        info->out_buf_wport_p = PARM(out_buf_wport_p);
        info->out_buffer_model_p = PARM(out_buffer_type_p);
        if(info->out_buf_p) {
           SIM_array_init(&info->out_buf_info_p, 1, 1, info->out_buf_wport_p, 
                          info->out_buf_set_p, info->flit_width, 
                          0, info->out_buffer_model_p);
        } 
        /* output bridge buffer */
        info->out_buf_b = PARM(out_buf_b);
        info->out_buf_set_b = PARM(out_buf_set_b);
        info->out_buf_wport_b = PARM(out_buf_wport_b);
        info->out_buffer_model_b = PARM(out_buffer_type_b);
        if(info->out_buf_b) {
           SIM_array_init(&info->out_buf_info_b, 1, 1, info->out_buf_wport_b,
                          info->out_buf_set_b, info->flit_width, 
                          0, info->out_buffer_model_b);
        } 
	/* central buffer */
	info->central_buf = PARM(central_buf);
	if (info->central_buf){
		info->pipe_depth = PARM(pipe_depth);
		/* central buffer is no FIFO */
		SIM_array_init(&info->central_buf_info, 0, PARM(cbuf_rport), 
                               PARM(cbuf_wport), PARM(cbuf_set), 
                               PARM(cbuf_width) * PARM(flit_width), 0, SRAM);
		/* dirty hack */
		info->cbuf_ff_model = NEG_DFF;
	}

        if (info->in_buf_p) {
           if (info->in_share_buf_p) info->in_n_switch_p = info->in_buf_info_p.read_ports;
           else info->in_n_switch_p = system_parameter.port_bw2xbar; 
        } else  info->in_n_switch_p = 1;

        if (info->in_buf_b) {
           if (info->in_share_buf_b) info->in_n_switch_b = info->in_buf_info_b.read_ports;
           else info->in_n_switch_b = system_parameter.bridge_bw2xbar; 
        } else  info->in_n_switch_b = 1;

        //decide crossbar input sizes
        info->n_switch_in_p = info->n_in_p * info->n_v_class_p * info->in_n_switch_p;
        info->n_switch_in_b = info->n_in_b * info->n_v_class_b * info->in_n_switch_b;
        info->n_switch_in = info->n_switch_in_p + info->n_switch_in_b;
        //decide crossbar output sizes
        info->out_n_switch_p = system_parameter.port_bandwidth; 
        info->out_n_switch_b = system_parameter.bridge_bandwidth; 
        info->n_switch_out_p = info->n_out_p * info->n_v_class_p * info->out_n_switch_p; 
        info->n_switch_out_b = info->n_out_b * info->n_v_class_b * info->out_n_switch_b;
        info->n_switch_out = info->n_switch_out_p + info->n_switch_out_b;
        /* switch allocator input port arbiter */
        if (n_channel_sum_p > 1) {
        info->sw_in_arb_model_p = PARM(sw_in_arb_model_p);
           if (info->sw_in_arb_model_p) {
              if (info->sw_in_arb_model_p == QUEUE_ARBITER) {
                  SIM_array_init( &info->sw_in_arb_queue_info_p, 1, 1, 1, n_channel_sum_p, 
                                  SIM_logtwo(n_channel_sum_p), 0, REGISTER);
                  info->sw_in_arb_ff_model_p = SIM_NO_MODEL;
              } else  info->sw_in_arb_ff_model_p = PARM(sw_in_arb_ff_model_p);
           } else info->sw_in_arb_ff_model_p = SIM_NO_MODEL;
        } else {
                info->sw_in_arb_model_p = SIM_NO_MODEL;
                info->sw_in_arb_ff_model_p = SIM_NO_MODEL;
               }
        /* switch allocator output port arbiter */
        if (info->n_total_in > 2) {
          info->sw_out_arb_model_p = PARM(sw_out_arb_model_p);
            if (info->sw_out_arb_model_p) {
               if (info->sw_out_arb_model_p == QUEUE_ARBITER) {
                   int sw_arbiter_size = (info->n_in_p - 1) * info->out_n_switch_p +
                                         info->n_in_b * info->out_n_switch_b; 
                   line_width = SIM_logtwo(sw_arbiter_size);
                   SIM_array_init(&info->sw_out_arb_queue_info_p, 1, 1, 1, 
                                  sw_arbiter_size, line_width, 0, REGISTER);
                   info->sw_out_arb_ff_model_p = SIM_NO_MODEL;
               } else  info->sw_out_arb_ff_model_p = PARM(sw_out_arb_ff_model_p);
            } else  info->sw_out_arb_ff_model_p = SIM_NO_MODEL;
        } else {
              info->sw_out_arb_model_p = SIM_NO_MODEL;
              info->sw_out_arb_ff_model_p = SIM_NO_MODEL;
               }

        /* switch allocator input bridge arbiter */
        if (n_channel_sum_b > 1) {
        info->sw_in_arb_model_b = PARM(sw_in_arb_model_b);
           if (info->sw_in_arb_model_b) {
              if (info->sw_in_arb_model_b == QUEUE_ARBITER) {
                  SIM_array_init( &info->sw_in_arb_queue_info_b, 1, 1, 1, 
                                  n_channel_sum_b, SIM_logtwo(n_channel_sum_b), 0, REGISTER);
                  info->sw_in_arb_ff_model_b = SIM_NO_MODEL;
   	      } else  info->sw_in_arb_ff_model_b = PARM(sw_in_arb_ff_model_b);
           } else  info->sw_in_arb_ff_model_b = SIM_NO_MODEL;
        } else {
                info->sw_in_arb_model_b = SIM_NO_MODEL;
                info->sw_in_arb_ff_model_b = SIM_NO_MODEL;
               }
        /* switch allocator output bridge arbiter */
        if (info->n_total_in > 2) {
          info->sw_out_arb_model_b = PARM(sw_out_arb_model_b);
            if (info->sw_out_arb_model_b) {
               if (info->sw_out_arb_model_b == QUEUE_ARBITER) {
                   int sw_arbiter_size = info->n_in_p * info->out_n_switch_p; 
                   line_width = SIM_logtwo(sw_arbiter_size);
                   SIM_array_init(&info->sw_out_arb_queue_info_b, 1, 1, 1, 
                                  sw_arbiter_size, line_width, 0, REGISTER);
                   info->sw_out_arb_ff_model_b = SIM_NO_MODEL;
               } else  info->sw_out_arb_ff_model_b = PARM(sw_out_arb_ff_model_b);
            } else  info->sw_out_arb_ff_model_b = SIM_NO_MODEL;
        } else {
              info->sw_out_arb_model_b = SIM_NO_MODEL;
              info->sw_out_arb_ff_model_b = SIM_NO_MODEL;
               }

        /* virtual channel allocator input port arbiter */
        if ( info->n_v_channel_p > 1 && info->n_in_p > 1) {
           info->vc_in_arb_model_p = PARM(vc_in_arb_model_p);
           if (info->vc_in_arb_model_p) {
              if (info->vc_in_arb_model_p == QUEUE_ARBITER) { 
                  SIM_array_init(&info->vc_in_arb_queue_info_p, 1, 1, 1, 
                           info->n_v_channel_p, SIM_logtwo(info->n_v_channel_p), 0, REGISTER);
                  info->vc_in_arb_ff_model_p = SIM_NO_MODEL;
              }  else info->vc_in_arb_ff_model_p = PARM(vc_in_arb_ff_model_p);
            }  else  info->vc_in_arb_ff_model_p = SIM_NO_MODEL;
        } else {
               info->vc_in_arb_model_p = SIM_NO_MODEL;
               info->vc_in_arb_ff_model_p = SIM_NO_MODEL;
                }
       /* virtual channel allocator output port arbiter */
       if (info->n_in_p > 1 && info->n_v_channel_p > 1) {
         info->vc_out_arb_model_p = PARM(vc_out_arb_model_p);
          if (info->vc_out_arb_model_p) {
              if (info->vc_out_arb_model_p == QUEUE_ARBITER) {
                  int vc_arbiter_size = (info->n_in_p - 1)*info->n_v_channel_p +
                                    info->n_in_b * info->n_v_channel_b;
                  line_width = SIM_logtwo(vc_arbiter_size);
                  SIM_array_init(&info->vc_out_arb_queue_info_p, 1, 1, 1, 
                                 vc_arbiter_size, line_width, 0, REGISTER);
                  info->vc_out_arb_ff_model_p = SIM_NO_MODEL;
              } else  info->vc_out_arb_ff_model_p = PARM(vc_out_arb_ff_model_p);
          } else  info->vc_out_arb_ff_model_p = SIM_NO_MODEL;
       }  else {  info->vc_out_arb_model_p = SIM_NO_MODEL;
                  info->vc_out_arb_ff_model_p = SIM_NO_MODEL;
                }

         /* virtual channel allocator input bridge arbiter */
        if ( info->n_v_channel_b > 1 ) {
           info->vc_in_arb_model_b = PARM(vc_in_arb_model_b);
           if (info->vc_in_arb_model_b) {
              if (info->vc_in_arb_model_b == QUEUE_ARBITER) { 
                  SIM_array_init(&info->vc_in_arb_queue_info_b, 1, 1, 1, 
                           info->n_v_channel_b, SIM_logtwo(info->n_v_channel_b), 0, REGISTER);
                  info->vc_in_arb_ff_model_b = SIM_NO_MODEL;
              }  else info->vc_in_arb_ff_model_b = PARM(vc_in_arb_ff_model_b);
            }  else  info->vc_in_arb_ff_model_b = SIM_NO_MODEL;
        } else {
               info->vc_in_arb_model_b = SIM_NO_MODEL;
               info->vc_in_arb_ff_model_b = SIM_NO_MODEL;
                }
       /* virtual channel allocator output bridge arbiter */
       if ( info->n_v_channel_b > 1) {
         info->vc_out_arb_model_b = PARM(vc_out_arb_model_b);
          if (info->vc_out_arb_model_b) {
              if (info->vc_out_arb_model_b == QUEUE_ARBITER) {
                  int vc_arbiter_size = info->n_in_b * info->n_v_channel_b;
                  line_width = SIM_logtwo(vc_arbiter_size);
                  SIM_array_init(&info->vc_out_arb_queue_info_b, 1, 1, 1, 
                                 vc_arbiter_size, line_width, 0, REGISTER);
                  info->vc_out_arb_ff_model_b = SIM_NO_MODEL;
              } else  info->vc_out_arb_ff_model_b = PARM(vc_out_arb_ff_model_b);
          } else  info->vc_out_arb_ff_model_b = SIM_NO_MODEL;
       }  else {  info->vc_out_arb_model_b = SIM_NO_MODEL;
                  info->vc_out_arb_ff_model_b = SIM_NO_MODEL;
                }
  
       /*virtual channel allocation vc selection model */
       if (info->vc_allocator_type_p == VC_SELECT && info->n_v_channel_p > 1 && (info->n_in_p + info->n_in_b)> 1) {
           info->vc_select_buf_type_p = PARM(vc_select_buf_type_p);
           SIM_array_init(&info->vc_select_buf_info_p, 1, 1, 1, info->n_v_channel_p,
                          SIM_logtwo(info->n_v_channel_p), 0, info->vc_select_buf_type_p);
   	} else info->vc_select_buf_type_p = SIM_NO_MODEL;

       /*virtual channel allocation vc selection model */
       if (info->vc_allocator_type_b == VC_SELECT && info->n_v_channel_b > 1 && (info->n_in_b + info->n_in_p) > 1) {
           info->vc_select_buf_type_b = PARM(vc_select_buf_type_b);
           SIM_array_init(&info->vc_select_buf_info_b, 1, 1, 1, info->n_v_channel_b,
                          SIM_logtwo(info->n_v_channel_b), 0, info->vc_select_buf_type_b);
   	} else info->vc_select_buf_type_b = SIM_NO_MODEL;

       /* clock related parameters */	
       info->pipelined = PARM(pipelined);
       info->H_tree_clock = PARM(H_tree_clock);
       info->router_diagonal = PARM(router_diagonal);
   
       /* PHASE 2: initialization */
       if(router_power) SIM_router_power_init(info, router_power);
       return;
}

router_power_d Dar_router_stat_energy(SIM_router_info_t *info,  
                              SIM_router_power_t *router, 
                              power_statistic_d *stats,  
                              const uint32_t node_id,
                              const double freq) {

 	double Eavg = 0, Estatic = 0;
        double Pclock_dyn = 0,    Pclock_static = 0, Pclock = 0;
	double Pbuf_static_p = 0, Pbuf_static_b = 0, Pbuf_static = 0;
        double Pbuf_dyn_p = 0,    Pbuf_dyn_b = 0, Pbuf_dyn = 0;
        double Pbuf_p = 0, Pbuf_b = 0, Pbuf = 0;
        double Pxbar_static = 0,  Pxbar_dyn = 0, Pxbar = 0;
        double Pva_arbiter_static_p = 0, Pva_arbiter_static_b = 0, Pva_arbiter_static = 0;
        double Pva_arbiter_dyn_p = 0,    Pva_arbiter_dyn_b = 0,    Pva_arbiter_dyn = 0;
        double Pva_arbiter_p = 0,        Pva_arbiter_b = 0,        Pva_arbiter = 0;
        double Psw_arbiter_static_p = 0, Psw_arbiter_static_b = 0, Psw_arbiter_static = 0;
        double Psw_arbiter_dyn_p = 0,    Psw_arbiter_dyn_b = 0,    Psw_arbiter_dyn = 0;
        double Psw_arbiter_p = 0,        Psw_arbiter_b = 0,        Psw_arbiter = 0;
        double Ptotal = 0;
        router_power_d router_p; 
	double e_in_buf_r_p, e_in_buf_w_p, 
               e_in_buf_r_b, e_in_buf_w_b,
               e_xbar, 
               e_va_act_stage1_p, e_va_req_stage1_p, e_sw_act_stage1_p, e_sw_req_stage1_p,
               e_va_act_stage2_p, e_va_req_stage2_p, e_sw_act_stage2_p, e_sw_req_stage2_p,
               e_va_act_stage1_b, e_va_req_stage1_b, e_sw_act_stage1_b, e_sw_req_stage1_b,
               e_va_act_stage2_b, e_va_req_stage2_b, e_sw_act_stage2_b, e_sw_req_stage2_b;
	int vc_allocator_enabled_p = 1;
	int vc_allocator_enabled_b = 1;

	/* expected value computation */
	e_in_buf_r_p     = stats->port_vq_r;
	e_in_buf_w_p     = stats->port_vq_w;
        e_in_buf_r_b     = stats->bridge_vq_r; 
        e_in_buf_w_b     = stats->bridge_vq_w;
	e_xbar           = stats->xbar_trans;
        e_va_act_stage1_p  = stats->va_act_stage1_port;
        e_va_req_stage1_p  = stats->va_req_stage1_port;
        e_va_act_stage2_p  = stats->va_act_stage2_port;
        e_va_req_stage2_p  = stats->va_req_stage2_port;
        e_va_act_stage1_b  = stats->va_act_stage1_bridge;
        e_va_req_stage1_b  = stats->va_req_stage1_bridge;
        e_va_act_stage2_b  = stats->va_act_stage2_bridge;
        e_va_req_stage2_b  = stats->va_req_stage2_bridge;
        e_sw_act_stage1_p  = stats->sw_act_stage1_port;
        e_sw_req_stage1_p  = stats->sw_req_stage1_port;
        e_sw_act_stage2_p  = stats->sw_act_stage2_port;
        e_sw_req_stage2_p  = stats->sw_req_stage2_port;
        e_sw_act_stage1_b  = stats->sw_act_stage1_bridge;
        e_sw_req_stage1_b  = stats->sw_req_stage1_bridge;
        e_sw_act_stage2_b  = stats->sw_act_stage2_bridge;
        e_sw_req_stage2_b  = stats->sw_req_stage2_bridge;


	/* input port buffers */
        int vq_num_p = info->n_in_p * info->n_v_class_p * (info->in_share_buf_p ?
                                                      1 : info->n_v_channel_p);

	if (info->in_buf_p) Eavg += SIM_array_stat_energy( &info->in_buf_info_p, 
                                                           &router->in_buf_p, 
                                                           e_in_buf_r_p, 
                                                           e_in_buf_w_p,
                                                           0, NULL, 0); 

        Pbuf_dyn_p = Eavg * freq;
        Pbuf_static_p = router->I_in_buf_static_p * vq_num_p * Vdd * SCALE_S;
        Pbuf_p = Pbuf_dyn_p + Pbuf_static_p;

        cout << "port vq in node " << hex << node_id << " power : " 
             << dec << Pbuf_p << " dyn : " << Pbuf_dyn_p 
             << " static : " << Pbuf_static_p << endl;

	/* input bridge buffers */
        int vq_num_b = info->n_in_b * info->n_v_class_b * (info->in_share_buf_b ?
                                                      1 : info->n_v_channel_b);

	if (info->in_buf_b) Eavg += SIM_array_stat_energy( &info->in_buf_info_b, 
                                                           &router->in_buf_b, 
                                                           e_in_buf_r_b, 
                                                           e_in_buf_w_b,
                                                           0, NULL, 0); 

        Pbuf_dyn_b = Eavg * freq - Pbuf_dyn_p;
        Pbuf_static_b = router->I_in_buf_static_b * vq_num_b * Vdd * SCALE_S;
        Pbuf_b = Pbuf_dyn_b + Pbuf_static_b;

        cout << "bridge vq in node " << hex << node_id << " power : " 
             << dec << Pbuf_b << " dyn : " << Pbuf_dyn_b 
             << " static : " << Pbuf_static_b << endl;
        
        Pbuf = Pbuf_p + Pbuf_b;
        Pbuf_dyn = Pbuf_dyn_p + Pbuf_dyn_b;
        Pbuf_static = Pbuf_static_p + Pbuf_static_b;

        cout << "vq in node " << hex << node_id << " power : " 
             << dec << Pbuf << " dyn : " << Pbuf_dyn 
             << " static : " << Pbuf_static << endl;
        /* main crossbar */
	if (info->crossbar_model) Eavg += SIM_crossbar_stat_energy( &router->crossbar, 
                                                                    0, NULL, 0, 
                                                                    e_xbar);

	Pxbar_dyn = (Eavg * freq - Pbuf_dyn);
	Pxbar_static = router->I_crossbar_static * Vdd * SCALE_S;
	Pxbar = Pxbar_dyn + Pxbar_static;
        cout << "xbar in node " << hex << node_id << " power : " 
             << dec << Pxbar << " dyn : " << Pxbar_dyn
             << " static : " << Pxbar_static << endl;
	/* port switch allocation (arbiter energy only) */
	/* port input (local) arbiter for switch allocation*/
	if (info->sw_in_arb_model_p) {
        Eavg += SIM_arbiter_stat_energy( &router->sw_in_arb_p, &info->sw_in_arb_queue_info_p,
                                         e_sw_req_stage1_p, 0, NULL, 0) * e_sw_act_stage1_p;  
	}
	/* port output (global) arbiter for switch allocation*/
	if (info->sw_out_arb_model_p) {
        Eavg += SIM_arbiter_stat_energy( &router->sw_out_arb_p, &info->sw_out_arb_queue_info_p, 
                                         e_sw_req_stage2_p, 0, NULL, 0) * e_sw_act_stage2_p; 
	}

	if(info->sw_in_arb_model_p || info->sw_out_arb_model_p){
		Psw_arbiter_dyn_p = Eavg * freq - Pbuf_dyn - Pxbar_dyn;
		Psw_arbiter_static_p = router->I_sw_arbiter_static_p * Vdd * SCALE_S;
		Psw_arbiter_p = Psw_arbiter_dyn_p + Psw_arbiter_static_p;
	}
//        cout << "port sw in node " << hex << node_id << " power : " 
//             << dec << Psw_arbiter_p 
//             << " dyn : " << Psw_arbiter_dyn_p  
//             << " static : " << Psw_arbiter_static_p << endl;

	/* bridge switch allocation (arbiter energy only) */
	/* bridge input (local) arbiter for switch allocation*/
	if (info->sw_in_arb_model_b) {
        Eavg += SIM_arbiter_stat_energy( &router->sw_in_arb_b, &info->sw_in_arb_queue_info_b,
                                         e_sw_req_stage1_b, 0, NULL, 0) * e_sw_act_stage1_b;  
	}
	/* port output (global) arbiter for switch allocation*/
	if (info->sw_out_arb_model_b) {
        Eavg += SIM_arbiter_stat_energy( &router->sw_out_arb_b, &info->sw_out_arb_queue_info_b, 
                                         e_sw_req_stage2_b, 0, NULL, 0) * e_sw_act_stage2_b; 
	}

	if(info->sw_in_arb_model_b || info->sw_out_arb_model_b){
		Psw_arbiter_dyn_b = Eavg * freq - Pbuf_dyn - Pxbar_dyn - Psw_arbiter_dyn_p;
		Psw_arbiter_static_b = router->I_sw_arbiter_static_b * Vdd * SCALE_S;
		Psw_arbiter_b = Psw_arbiter_dyn_b + Psw_arbiter_static_b;
	}
//        cout << "bridge sw in node " << hex << node_id << " power : " 
//             << dec << Psw_arbiter_b 
//             << " dyn : " << Psw_arbiter_dyn_b  
//             << " static : " << Psw_arbiter_static_b << endl;

        Psw_arbiter = Psw_arbiter_p + Psw_arbiter_b;
        Psw_arbiter_dyn = Psw_arbiter_dyn_p + Psw_arbiter_dyn_b;
        Psw_arbiter_static = Psw_arbiter_static_p + Psw_arbiter_static_b;

        cout << "sw in node " << hex << node_id << " power : " 
             << dec << Psw_arbiter 
             << " dyn : " << Psw_arbiter_dyn  
             << " static : " << Psw_arbiter_static << endl;
	/* port virtual channel allocation (arbiter energy only) */

	if(info->vc_allocator_type_p == ONE_STAGE_ARB && info->vc_out_arb_model_p){
        /* one stage arbitration (vc allocation)*/
        /* This works only if the routing function returns a single virtual channel,  
           otherwise, shoule use the two stages vc allocator , we just assume cost
           same power as the stage2 vc alloactor */
        Eavg += SIM_arbiter_stat_energy(&router->vc_out_arb_p, 
                                        &info->vc_out_arb_queue_info_p, 
                                        e_va_req_stage2_p,
                                        0, NULL, 0) * e_va_act_stage2_p;
	}
	else if(info->vc_allocator_type_p == TWO_STAGE_ARB && info->vc_in_arb_model_p 
                && info->vc_out_arb_model_p){
                /* first stage arbitration (vc allocation)*/
                if (info->vc_in_arb_model_p) {
                  Eavg += SIM_arbiter_stat_energy(&router->vc_in_arb_p, 
                                                  &info->vc_in_arb_queue_info_p, 
                                                  e_va_req_stage1_p, 0, NULL, 0) 
                                                  * e_va_act_stage1_p;}

		/* second stage arbitration (vc allocation)*/
		if (info->vc_out_arb_model_p) {
                  Eavg += SIM_arbiter_stat_energy(&router->vc_out_arb_p, 
                                                  &info->vc_out_arb_queue_info_p, 
                                                  e_va_req_stage2_p, 0, NULL, 0) 
                                                  * e_va_act_stage2_p;}
	}
	else if(info->vc_allocator_type_p == VC_SELECT && info->n_v_channel_p > 1 && 
                (info->n_in_p + info->n_in_b) > 1){
		double n_read_p = e_va_act_stage1_p;
		double n_write_p = e_va_act_stage2_p;
		Eavg += SIM_array_stat_energy(&info->vc_select_buf_info_p, 
                                              &router->vc_select_buf_p, n_read_p , 
                                              n_write_p, 0, NULL, 0); }
	else    vc_allocator_enabled_p = 0; //set to 0 means no vc allocator is used

	if(info->n_v_channel_p > 1 && vc_allocator_enabled_p){
		Pva_arbiter_dyn_p = Eavg * freq - Pbuf_dyn - Pxbar_dyn - Psw_arbiter_dyn; 
		Pva_arbiter_static_p = router->I_vc_arbiter_static_p * Vdd * SCALE_S;
		Pva_arbiter_p = Pva_arbiter_dyn_p + Pva_arbiter_static_p;
	}
//        cout << "port va in node " << hex << node_id << " power : " 
//             << dec << Pva_arbiter_p 
//             << " dyn : " << Pva_arbiter_dyn_p
//             << " static : " << Pva_arbiter_static_p << endl;

	/* bridge virtual channel allocation (arbiter energy only) */

	if(info->vc_allocator_type_b == ONE_STAGE_ARB && info->vc_out_arb_model_b){
        /* one stage arbitration (vc allocation)*/
        /* This works only if the routing function returns a single virtual channel,  
           otherwise, shoule use the two stages vc allocator , we just assume cost
           same power as the stage2 vc alloactor */
        Eavg += SIM_arbiter_stat_energy(&router->vc_out_arb_b, 
                                        &info->vc_out_arb_queue_info_b, 
                                        e_va_req_stage2_b,
                                        0, NULL, 0) * e_va_act_stage2_b;
	}
	else if(info->vc_allocator_type_b == TWO_STAGE_ARB && info->vc_in_arb_model_b 
                && info->vc_out_arb_model_b) {
                /* first stage arbitration (vc allocation)*/
                if (info->vc_in_arb_model_b) {
                  Eavg += SIM_arbiter_stat_energy(&router->vc_in_arb_b, 
                                                  &info->vc_in_arb_queue_info_b, 
                                                  e_va_req_stage1_b, 0, NULL, 0) 
                                                  * e_va_act_stage1_b;}

		/* second stage arbitration (vc allocation)*/
		if (info->vc_out_arb_model_b) {
                  Eavg += SIM_arbiter_stat_energy(&router->vc_out_arb_b, 
                                                  &info->vc_out_arb_queue_info_b, 
                                                  e_va_req_stage2_b, 0, NULL, 0) 
                                                  * e_va_act_stage2_b;}
	}
	else if(info->vc_allocator_type_b == VC_SELECT && info->n_v_channel_b > 1 && 
                (info->n_in_b + info->n_in_p) > 1){
		double n_read_b = e_va_act_stage1_b;
		double n_write_b = e_va_act_stage2_b;
		Eavg += SIM_array_stat_energy(&info->vc_select_buf_info_b, 
                                              &router->vc_select_buf_b, n_read_b , 
                                              n_write_b, 0, NULL, 0); }
	else    vc_allocator_enabled_b = 0; //set to 0 means no vc allocator is used

	if(info->n_v_channel_b > 1 && vc_allocator_enabled_b){
		Pva_arbiter_dyn_b = Eavg * freq - Pbuf_dyn - Pxbar_dyn - Psw_arbiter_dyn -
                                    Pva_arbiter_dyn_p; 
		Pva_arbiter_static_b = router->I_vc_arbiter_static_b * Vdd * SCALE_S;
		Pva_arbiter_b = Pva_arbiter_dyn_b + Pva_arbiter_static_b;
	}

//        cout << "bridge va in node " << hex << node_id << " power : " 
//             << dec << Pva_arbiter_b 
//             << " dyn : " << Pva_arbiter_dyn_b
//             << " static : " << Pva_arbiter_static_b << endl;
        
        Pva_arbiter = Pva_arbiter_p + Pva_arbiter_b;
        Pva_arbiter_dyn = Pva_arbiter_dyn_p + Pva_arbiter_dyn_b;
        Pva_arbiter_static = Pva_arbiter_static_p + Pva_arbiter_static_b;

        cout << "va in node " << hex << node_id << " power : " 
             << dec << Pva_arbiter 
             << " dyn : " << Pva_arbiter_dyn
             << " static : " << Pva_arbiter_static << endl;
	/*router clock power (supported for 90nm and below) */
	if(PARM(TECH_POINT) <=90) {
		Eavg += SIM_total_clockEnergy(info, router);
		Pclock_dyn = Eavg * freq - Pbuf_dyn - Pxbar_dyn - Pva_arbiter_dyn - 
                             Psw_arbiter_dyn;
		Pclock_static = router->I_clock_static * Vdd * SCALE_S;
		Pclock = Pclock_dyn + Pclock_static;
	}

        cout << "clk in node " << hex << node_id << " power : " 
             << dec << Pclock 
             << " dyn : " << Pclock_dyn
             << " static : " << Pclock_static << endl;

	/* static power */
	Estatic = router->I_static * Vdd * Period * SCALE_S;
	Eavg += Estatic;
	Ptotal = Eavg * freq;
        
        router_p.dynamic_p = Pbuf_dyn + Pxbar_dyn + Pva_arbiter_dyn + Psw_arbiter_dyn + Pclock_dyn;
        router_p.static_p = Pbuf_static + Pxbar_static + Pva_arbiter_static +
                            Psw_arbiter_static + Pclock_static;
        router_p.total = Ptotal;
	return router_p;
}

router_power_d  DarOrn_power(const uint32_t node_id,
                     system_parameter_d system_parameter,
                     power_statistic_d power_stats) {

     SIM_router_info_t     router_info;
     SIM_router_power_t    router;
     router_power_d        router_p;
 
     Dar_router_init( &router_info, &router, system_parameter); 
     router_p = Dar_router_stat_energy(&router_info, &router, 
                                       &power_stats, node_id, PARM(Freq));

     return router_p;
}

link_power_d Link_power(double link_load, const uint64_t bandwidth) {

#if ( PARM(TECH_POINT) <= 90 )
     double link_len = PARM(link_length) * 1e-6;
     double freq = PARM(Freq);
     u_int  data_width = PARM(flit_width);
     double Pdynamic, Pleakage;
     link_power_d link_power;

     Pdynamic = 0.5 * link_load * bandwidth
                * LinkDynamicEnergyPerBitPerMeter(link_len, Vdd) * freq 
                * link_len * (double)data_width;
     Pleakage = LinkLeakagePowerPerMeter(link_len, Vdd) * link_len * data_width;
     link_power.dynamic_p = Pdynamic;
     link_power.static_p = Pleakage;
     link_power.total = Pdynamic + Pleakage;
 
     return link_power;
     
#else 
    cout << "Link power only supported for 90nm, 65nm, 45nm and 32nm " << endl;
    return 0;
#endif
}
