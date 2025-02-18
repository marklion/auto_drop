exception ad_gen_exp{
    1: string msg,
}

typedef i32 u16 (cpp.type = "unsigned short");
service runner_sm{
    bool match_device(1:string use_for, 2:u16 port) throws (1:ad_gen_exp exp),
    bool clear_device(1:string use_for) throws (1:ad_gen_exp exp),
    oneway void push_sm_event(1:string event_name),
    string get_sm_state_string() throws (1:ad_gen_exp exp),
    oneway void stop_sm(),
    bool check_lua_code(1:string code, 2:bool real_run) throws (1:ad_gen_exp exp),
    void reset_sm() throws (1:ad_gen_exp exp),
}

enum vehicle_position_detect_state{
    vehicle_postion_begin = 0,
    vehicle_postion_middle = 1,
    vehicle_postion_end = 2,
    vehicle_postion_out = 3,
}

struct vehicle_rd_detect_result {
    1: vehicle_position_detect_state state,
    2: bool is_full,
}

struct device_config {
    1:string driver_name,
    2:u16 port,
    3:list<string> argv,
    4:string device_name,
}
struct sm_config {
    1:string sm_name,
    2:u16 port,
    3:list<string> argv,
}

service config_management{
    device_config start_device(1:string driver_name, 2:list<string> argv, 3:string device_name) throws (1:ad_gen_exp exp),
    void stop_device(1:u16 port) throws (1:ad_gen_exp exp),
    list<device_config> get_device_list() throws (1:ad_gen_exp exp),
    sm_config start_sm(1:string sm_name, 2:list<string> argv) throws (1:ad_gen_exp exp),
    void stop_sm(1:u16 port) throws (1:ad_gen_exp exp),
    list<sm_config> get_sm_list() throws (1:ad_gen_exp exp),
}

service driver_service{
    bool set_sm(1:u16 sm_port) throws (1:ad_gen_exp exp),
    oneway void stop_device(),
    oneway void voice_broadcast(1:string voice_content, 2:i32 times),
    oneway void voice_stop(),
    oneway void led_display(1:string led_content),
    oneway void led_stop(),
    oneway void gate_control(1:bool is_close),
    string get_trigger_vehicle_plate() throws (1:ad_gen_exp exp),
    vehicle_rd_detect_result vehicle_rd_detect() throws (1:ad_gen_exp exp),
    bool vehicle_passed_gate() throws (1:ad_gen_exp exp),
    oneway void sim_vehicle_came(1:string plate),
    oneway void sim_gate_status(1:bool is_close),
    oneway void sim_scale_weight(1:double weight),
    oneway void sim_vehicle_position(1:vehicle_position_detect_state state),
    oneway void sim_vehicle_stuff(1:bool is_full),
}