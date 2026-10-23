#ifndef PORT_AP_H
#define PORT_AP_H
#ifdef __cplusplus
extern "C" {
#endif
int port_ap_enabled(void);
int port_ap_ticket_mode(void);
int port_ap_starting_nozzle(void);
int port_ap_has_nozzle(void);
void port_ap_observe_fludd_stage(int absent);
int port_ap_fludd_allowed(void);
int port_ap_episode_count(int native_count);
void port_ap_observe_position(int episode);
void port_ap_observe_death(int dying, int alive);
int port_ap_take_linked_death(void);
int port_ap_shuffle_blue_coins(void);
int port_ap_blue_coin_total(int native_total);
int port_ap_block_blue_reward(unsigned int flag);
void port_ap_reset_authority(void);
void port_ap_observe_goal(int physical_goal);
void port_ap_observe_game(int current_nozzle, int secondary_nozzle,
                          int has_fludd);
void port_ap_observe_input(unsigned int buttons, unsigned int trigger,
                           int request, int before, int after, int secondary,
                           int allowed);
void port_ap_new_guid(unsigned char guid[16]);
int port_ap_shines(void);
int port_ap_yoshi(void);
int port_ap_preferred_nozzle(void);
int port_ap_nozzle_allowed(int nozzle);
int port_ap_ticket_flag(unsigned int flag, int *value);
int port_ap_stage_allowed(int stage);
void port_ap_tick(const unsigned char guid[16], int persisted, int ready,
                  int save_slot, int stage, int (*read_flag)(unsigned int),
                  int (*reconcile_one_ups)(int accepted_count, int *applied_count)
#ifdef __cplusplus
                      = nullptr
#endif
);
#ifdef __cplusplus
}
#endif
#endif
