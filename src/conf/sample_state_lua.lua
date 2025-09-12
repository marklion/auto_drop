--[[
环境提供的函数说明：
sm:start_timer(sec,func) 启动一个定时器
    参数:sec为秒数
        func为定时器到时时执行的函数
    返回:一个定时器变量
sm:start_timer_ms(ms_sec,func) 启动一个毫秒定时器
    参数:ms_sec为毫秒数
        func为定时器到时时执行的函数
    返回:一个定时器变量
sm:stop_timer(timer) 停止一个定时器
    参数:timer为定时器变量
sm:dev_voice_broadcast(device, content, times) 播放语音
    参数:device为喇叭设备名称
        content为语音内容字符串
        times为播放次数，-1代表一直播放
sm:dev_voice_stop(device) 停止播放语音
    参数:device为喇叭设备名称
sm:dev_led_display(device, content) 显示LED内容
    参数:device为LED设备名称
        content为显示内容字符串
sm:dev_led_stop(device) 停止显示LED内容
    参数:device为LED设备名称
sm:dev_set_lc_open(device, thredhold) 设定溜槽开启阈值
    参数:device为溜槽设备名称
        thredhold为阈值
sm:dev_gate_control(device, is_close) 控制闸机
    参数:device为闸机设备名称
        is_close为true时关闭闸机，false时打开闸机
sm:dev_get_trigger_vehicle_plate(device) 获取最近的拍到的车牌
    参数:device为车牌识别设备名称
    返回:车牌字符串
sm:dev_vehicle_rd_detect(device) 车辆雷达检测结果检测
    参数:device为检测设备名称
    返回:车辆检测结果对象,其中
        .state是车辆位置 可能是0123分别代表开始、中间、结束、无效
        .is_full代表是否装满
        .full_rate代表装满率，0-100的浮点数
sm:dev_vehicle_passed_gate(device) 获取车辆是否通过闸机
    参数:device为闸机设备名称
    返回:是否通过
sm:dev_save_ply(device, reason) 保存当前的点云
    参数:device为设备名称
        reason为保存原因
sm:trigger_event(event_name) 触发事件
    参数:event_name为事件名称
sm:call_http_api(url, method, body, header) 调用外部的http接口
    参数:url为接口地址
        method为请求方法
        body为请求体
        header为请求头
    返回:http请求的回复
sm:sleep_wait(sec, micro_sec) 睡眠等待
    参数:sec为秒数
        micro_sec为微秒数

sm:refresh_current_state() 刷新当前状态,刷新后，外部系统可获取到当前业务状态

print_log(log) 打印日志
    参数:log为日志内容
举例1：
sm:dev_voice_broadcast("some_device","hello",1)
sm:dev_led_display("some_device","hello")
sm:dev_gate_control("some_device",false)
some_timer = sm:start_timer(1,function()
        detect_result = sm:dev_vehicle_rd_detect("some_device")
        if (sm:dev_vehicle_passed_gate("some_device") and detect_result.is_full) then
                sm:dev_voice_stop("some_device")
                sm:dev_led_stop("some_device")
                sm:stop_timer(some_timer)
                sm:trigger_event("some_event")
        end)
end)
这段代码表示：
让设备some_device播放hello语音一次，显示hello内容，打开闸机
打开定时器，每1秒获取一次车辆雷达检测结果和车辆是否通过，
如果车辆通过闸机并且车辆装满，则停止播放语音，停止显示内容，停止定时器，触发事件some_event
--]]
--init enter
function handler_init_timer()
    sm:stop_timer(init_timer);
    init_timer = nil;
    local rd_ret = sm:dev_vehicle_rd_detect("ridar")
    if (rd_ret.state < 3) then
        sm:trigger_event("ready_occ");
    end
    init_timer = sm:start_timer(1, handler_init_timer)
end
sm:dev_save_ply("ridar", "ready");
init_timer = sm:start_timer(1, handler_init_timer)
--init exit
sm:dev_save_ply("ridar", "exception");
if (init_timer) then
    sm:stop_timer(init_timer);
end
--work enter
function handler_work_timer()
    sm:stop_timer(work_timer);
    work_timer = nil;
    local rd_ret = sm:dev_vehicle_rd_detect("ridar")
    if (rd_ret.state == 3) then
        sm:trigger_event("exception_occ");
    end
    local currentTime = os.date("%Y-%m-%d %H:%M:%S")
    local newRecord = string.format("%s,%.1f", currentTime, rd_ret.full_rate)
    local filePath = "/database/rate_record.csv"
    local file, err = io.open(filePath, "a")
    if file then
        file:write(newRecord .. "\n")
        file:close()
    end

    work_timer = sm:start_timer(1, handler_work_timer)
end
work_timer = sm:start_timer(1, handler_work_timer)
--work exit
if (work_timer) then
    sm:stop_timer(work_timer);
end
--change enter
function handler_change_timer()
    sm:stop_timer(change_timer);
    change_timer = nil;
    local rd_ret = sm:dev_vehicle_rd_detect("ridar")
    if (rd_ret.state < 3) then
        sm:trigger_event("ready_confirm");
    else
        sm:trigger_event("exception_confirm");
    end
    change_timer = sm:start_timer(1, handler_change_timer)
end
change_timer = sm:start_timer(1, handler_change_timer)
--change exit
if (change_timer) then
    sm:stop_timer(change_timer);
end
