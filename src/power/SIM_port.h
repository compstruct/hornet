#ifndef _SIM_PORT_H
#define _SIM_PORT_H
/*link length in micro-meter*/
#define PARM_link_length      1000  /* usually set 1000 in 65nm 45nm 32nm */
/*Technology related parameters */
#define PARM_TECH_POINT       32 
#define PARM_TRANSISTOR_TYPE  NVT   /* transistor type, HVT, NVT, or LVT */
#define PARM_Vdd              0.9   /* 65nm 1.2  45nm 1.0 32nm 0.9 */
#define PARM_Freq             2.0e9
//#define PARM_Freq             2000000000

/* router module parameters */
/* general parameters */
//#define PARM_in_port 		5	/* # of router input ports */
#define PARM_cache_in_port	0	/* # of cache input ports */
#define PARM_mc_in_port	        0	/* # of memory controller input ports */
#define PARM_io_in_port	        0	/* # of I/O device input ports */
//#define PARM_out_port		5	
#define PARM_cache_out_port	0	/* # of cache output ports */
#define PARM_mc_out_port	0	/* # of memory controller output ports */
#define PARM_io_out_port	0	/* # of I/O device output ports */
#define PARM_flit_width        64	/* flit width in bits */

/* virtual channel parameters */
#define PARM_v_class_p            1       /* # of total message classes */
//#define PARM_v_channel          4	/* # of virtual channels per virtual message class*/
//#define PARM_vq_size            8
//#define PARM_bw2xbar            1     /* this is set for DARSIM */
#define PARM_cache_class	0	/* # of cache port virtual classes */
#define PARM_mc_class		0	/* # of memory controller port virtual classes */
#define PARM_io_class		0	/* # of I/O device port virtual classes */

#define PARM_in_share_buf_p	0	/* do input virtual channels physically share buffers? */
#define PARM_out_share_buf_p	0	/* do output virtual channels physically share buffers? */
#define PARM_in_share_switch_p	1       /* do input virtual channels share crossbar input ports
                                           and the bandwidth size*/
#define PARM_out_share_switch_p	1	/* do output virtual channels share crossbar output ports? */

/* bridge virtual channel parameters */
#define PARM_v_class_b          1    /* # of total message classes */
//#define PARM_v_channel_bridge          4  /* # of virtual channels per virtual message class*/
//#define PARM_vq_size_bridge            8
//#define PARM_bw2xbar_bridge            1  /* this is set for DARSIM */
#define PARM_in_share_buf_b	0   /* do input virtual channels physically share buffers? */
#define PARM_out_share_buf_b	0   /* do output virtual channels physically share buffers? */
#define PARM_in_share_switch_b	1   /* do input virtual channels share crossbar input ports
                                               and the bandwidth size*/
#define PARM_out_share_switch_b 1   /* do output virtual channels share crossbar output ports? */

/* crossbar parameters */
#define PARM_crossbar_model      MULTREE_CROSSBAR /* crossbar model: MATRIX_CROSSBAR or MULTREE_CROSSBAR*/ 
#define PARM_crsbar_degree         4             /* crossbar mux degree */
#define PARM_connect_type        TRISTATE_GATE    /* crossbar connector type TRISTATE_GATE or TRANS_GATE */
#define PARM_trans_type	         NP_GATE          /* crossbar transmission gate type */
#define PARM_crossbar_in_len       0             /* crossbar input line length, if known */
#define PARM_crossbar_out_len      0             /* crossbar output line length, if known */
#define PARM_xb_in_seg             0
#define PARM_xb_out_seg            0
/* HACK HACK HACK */
#define PARM_exp_xb_model	SIM_NO_MODEL   /* the other parameter is MATRIX_CROSSBAR */
#define PARM_exp_in_seg		   2
#define PARM_exp_out_seg	   2

/* input buffer parameters */
#define PARM_in_buf_p              1         /* have input buffer? */
//#define PARM_in_buf_set          8	
#define PARM_in_buf_rport_p        1         /* # of read ports */
#define PARM_in_buffer_type_p      SRAM      /*buffer model type, SRAM or REGISTER*/

/* input bridge buffer parameters */
#define PARM_in_buf_b              1         /* have input buffer? */
//#define PARM_in_buf_set_bridge   8	
#define PARM_in_buf_rport_b        1         /* # of read ports */
#define PARM_in_buffer_type_b      SRAM      /*buffer model type, SRAM or REGISTER*/

#define PARM_cache_in_buf          0
#define PARM_cache_in_buf_set      0
#define PARM_cache_in_buf_rport    0

#define PARM_mc_in_buf             0
#define PARM_mc_in_buf_set         0
#define PARM_mc_in_buf_rport       0

#define PARM_io_in_buf             0
#define PARM_io_in_buf_set         0
#define PARM_io_in_buf_rport       0

/*port output buffer parameters */
#define PARM_out_buf_p             0
#define PARM_out_buf_set_p         0
#define PARM_out_buf_wport_p       0
#define PARM_out_buffer_type_p     SRAM      /*buffer model type, SRAM or REGISTER*/

/*bridge output buffer parameters */
#define PARM_out_buf_b             0
#define PARM_out_buf_set_b         0
#define PARM_out_buf_wport_b       0
#define PARM_out_buffer_type_b     SRAM      /*buffer model type, SRAM or REGISTER*/
/* central buffer parameters */
#define PARM_central_buf           0         /* have central buffer? */
#define PARM_cbuf_set              2560	     /* # of rows */
#define PARM_cbuf_rport	           2         /* # of read ports */
#define PARM_cbuf_wport            2         /* # of write ports */
#define PARM_cbuf_width            4         /* # of flits in one row */
#define PARM_pipe_depth            4         /* # of banks */

/* array parameters shared by various buffers */
#define PARM_wordline_model	CACHE_RW_WORDLINE
#define PARM_bitline_model	RW_BITLINE
#define PARM_mem_model		NORMAL_MEM
#define PARM_row_dec_model	GENERIC_DEC
#define PARM_row_dec_pre_model	SINGLE_OTHER
#define PARM_col_dec_model	SIM_NO_MODEL
#define PARM_col_dec_pre_model	SIM_NO_MODEL
#define PARM_mux_model		SIM_NO_MODEL
#define PARM_outdrv_model	REG_OUTDRV

/* these 3 should be changed together */
/* use double-ended bitline because the array is too large */
#define PARM_data_end			2
#define PARM_amp_model			GENERIC_AMP
#define PARM_bitline_pre_model	EQU_BITLINE
//#define PARM_data_end			1
//#define PARM_amp_model		SIM_NO_MODEL
//#define PARM_bitline_pre_model	SINGLE_OTHER

/* port switch allocator arbiter parameters */
#define PARM_sw_in_arb_model_p      RR_ARBITER	/* input model: MATRIX_ARBITER , RR_ARBITER, QUEUE_ARBITER*/
#define PARM_sw_in_arb_ff_model_p   NEG_DFF     /* input side arbiter flip-flop model type */
#define PARM_sw_out_arb_model_p     RR_ARBITER	/* output side arbiter model type, MATRIX_ARBITER */
#define PARM_sw_out_arb_ff_model_p  NEG_DFF	/* output side arbiter flip-flop model type */

/* port virtual channel allocator arbiter parameters */
#define PARM_vc_allocator_type_p   VC_SELECT   /*vc allocator type, ONE_STAGE_ARB, TWO_STAGE_ARB, VC_SELECT*/
#define PARM_vc_in_arb_model_p     RR_ARBITER  /*input model:TWO_STAGE_ARB. MATRIX_ARBITER, RR_ARBITER, QUEUE_ARBITER*/
#define PARM_vc_in_arb_ff_model_p  NEG_DFF     /* input side arbiter flip-flop model type */
#define PARM_vc_out_arb_model_p    RR_ARBITER  /*output model type (for both ONE_STAGE_ARB and TWO_STAGE_ARB). MATRIX_ARBITER, RR_ARBITER, QUEUE_ARBITER */
#define PARM_vc_out_arb_ff_model_p NEG_DFF     /* output side arbiter flip-flop model type */
#define PARM_vc_select_buf_type_p  REGISTER    /* vc_select buffer type, SRAM or REGISTER */

/* bridge switch allocator arbiter parameters */
#define PARM_sw_in_arb_model_b      RR_ARBITER	/* input model: MATRIX_ARBITER , RR_ARBITER, QUEUE_ARBITER*/
#define PARM_sw_in_arb_ff_model_b   NEG_DFF     /* input side arbiter flip-flop model type */
#define PARM_sw_out_arb_model_b     RR_ARBITER	/* output side arbiter model type, MATRIX_ARBITER */
#define PARM_sw_out_arb_ff_model_b  NEG_DFF	/* output side arbiter flip-flop model type */

/* bridge virtual channel allocator arbiter parameters */
#define PARM_vc_allocator_type_b   VC_SELECT   /*vc allocator type, ONE_STAGE_ARB, TWO_STAGE_ARB, VC_SELECT*/
#define PARM_vc_in_arb_model_b     RR_ARBITER  /*input model:TWO_STAGE_ARB. MATRIX_ARBITER, RR_ARBITER, QUEUE_ARBITER*/
#define PARM_vc_in_arb_ff_model_b  NEG_DFF     /* input side arbiter flip-flop model type */
#define PARM_vc_out_arb_model_b    RR_ARBITER  /*output model type (for both ONE_STAGE_ARB and TWO_STAGE_ARB). MATRIX_ARBITER, RR_ARBITER, QUEUE_ARBITER */
#define PARM_vc_out_arb_ff_model_b NEG_DFF     /* output side arbiter flip-flop model type */
#define PARM_vc_select_buf_type_b  REGISTER    /* vc_select buffer type, SRAM or REGISTER */

/*link wire parameters*/
#define WIRE_LAYER_TYPE            INTERMEDIATE    /*wire layer type, INTERMEDIATE or GLOBAL*/
#define PARM_width_spacing         DWIDTH_DSPACE   /*choices are SWIDTH_SSPACE, SWIDTH_DSPACE, DWIDTH_SSPACE, DWIDTH_DSPACE*/
#define PARM_buffering_scheme      MIN_DELAY   	/*choices are MIN_DELAY, STAGGERED */
#define PARM_shielding             FALSE   	/*choices are TRUE, FALSE */

/*clock power parameters*/
#define PARM_pipelined          1   	/*1 means the router is pipelined, 0 means not*/
#define PARM_H_tree_clock       1       /*1 means calculate H_tree_clock power, 0 means not calculate H_tree_clock*/
#define PARM_router_diagonal    626 	/*router diagonal in micro-meter */

/* RF module parameters */
#define PARM_read_port          1
#define PARM_write_port         1
#define PARM_n_regs             64
#define PARM_reg_width          32

#define PARM_ndwl               1
#define PARM_ndbl               1
#define PARM_nspd               1

#define PARM_POWER_STATS        0

#endif	/* _SIM_PORT_H */
