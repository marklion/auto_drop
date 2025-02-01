exception ad_gen_exp{
    1: string msg,
}

service runner_sm{
    oneway void push_sm_event(1:string event_name),
}

service device_driver{
    oneway void voice_broadcast(1:string voice_content),
}