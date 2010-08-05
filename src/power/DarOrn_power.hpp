#include <stdint.h>

extern "C" {
  #include "SIM_parameter.h"
  #include "SIM_router.h"
  #include "SIM_link.h"
}

typedef struct
{
    double port_vq_r;
    double port_vq_w;
    double bridge_vq_r;
    double bridge_vq_w;
    double xbar_trans;
    double va_act_stage1_port;
    double va_req_stage1_port;
    double va_act_stage2_port;
    double va_req_stage2_port;
    double va_act_stage1_bridge;
    double va_req_stage1_bridge;
    double va_act_stage2_bridge;
    double va_req_stage2_bridge;
    double sw_act_stage1_port;
    double sw_req_stage1_port;
    double sw_act_stage2_port;
    double sw_req_stage2_port;
    double sw_act_stage1_bridge;
    double sw_req_stage1_bridge;
    double sw_act_stage2_bridge;
    double sw_req_stage2_bridge;
} power_statistic_d;

typedef struct
{
    uint32_t input_ports;
    uint64_t output_ports;
    uint64_t vq_per_port;
    uint64_t flits_per_port_vq;
    uint64_t port_bw2xbar;
    uint64_t port_bandwidth;

    uint64_t bridge_in;
    uint64_t bridge_out;
    uint64_t vq_per_bridge;
    uint64_t flits_per_bridge_vq;
    uint64_t bridge_bw2xbar;
    uint64_t bridge_bandwidth;
} system_parameter_d;

typedef struct
{
   double dynamic_p;
   double static_p;
   double total;
} link_power_d;

typedef struct
{
   double dynamic_p;
   double static_p;
   double total;
} router_power_d;

router_power_d  DarOrn_power(const uint32_t node_id,
                     system_parameter_d parameter,
                     power_statistic_d power);

void    Dar_router_init(SIM_router_info_t *info, 
                        SIM_router_power_t *router_power,
                        system_parameter_d parameter);

router_power_d  Dar_router_stat_energy(SIM_router_info_t *info, 
                               SIM_router_power_t *router_power,
                               power_statistic_d *stats, 
                               const uint32_t node_id,
                               const double freq);

link_power_d  Link_power(double link_load,
                         const uint64_t bandwidth);
