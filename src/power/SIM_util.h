#ifndef _SIM_UTIL_H
#define _SIM_UTIL_H

extern u_int SIM_Hamming(LIB_Type_max_uint old_val, LIB_Type_max_uint new_val, LIB_Type_max_uint mask);
extern u_int SIM_Hamming_group(LIB_Type_max_uint d1_new, LIB_Type_max_uint d1_old, LIB_Type_max_uint d2_new, LIB_Type_max_uint d2_old, u_int width, u_int n_grp);

/* statistical functions */
extern int SIM_print_stat_energy(char *path, double Energy, int print_flag);
extern u_int SIM_strlen(char *s);
extern char *SIM_strcat(char *dest, const char *src);
extern int SIM_res_path(char *path, u_int id);
extern int SIM_dump_tech_para(void);

extern u_int SIM_logtwo(LIB_Type_max_uint x);

extern int SIM_squarify(int rows, int cols);
extern double SIM_driver_size(double driving_cap, double desiredrisetime);

#endif /* _SIM_UTIL_H */

