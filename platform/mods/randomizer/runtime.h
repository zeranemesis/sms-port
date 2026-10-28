#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int port_randomizer_prepare(void);
int port_randomizer_active(void);
int port_randomizer_ready(void);
void port_randomizer_add_scene(unsigned scene);
int port_randomizer_finish_scenes(void);
int port_randomizer_route(unsigned scene, unsigned current_scene);
void port_randomizer_request_nozzle(int nozzle);
int port_randomizer_take_nozzle(void);
int port_randomizer_classic(void);
unsigned port_randomizer_unlocks(void);
unsigned port_randomizer_reward(unsigned shine);
void port_randomizer_set_collected(unsigned shine, int collected);
int port_randomizer_filter_action(unsigned* status, int is_player, int* clear_holder);
int port_randomizer_can_use_nozzle(unsigned nozzle);
int port_randomizer_can_switch_backup(void);
int port_randomizer_fludd_available(void);
int port_randomizer_backup_nozzle(void);
void port_randomizer_player_loaded(int has_fludd);
const char* port_randomizer_unlock_name(unsigned reward);
const char* port_randomizer_reward_name(unsigned shine);
void* port_randomizer_shine_model(unsigned* size);
void port_randomizer_shine_color(unsigned char* rgb);
void port_randomizer_set_language(unsigned language);
unsigned port_randomizer_language(void);
const char* port_randomizer_text(unsigned text);
int port_randomizer_reward_message(unsigned shine, char* output, unsigned capacity);
#ifdef __cplusplus
}
#endif
