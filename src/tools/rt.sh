#!/bin/bash

# 时间记录脚本
# 按回车记录当前时间，按c键退出并显示所有记录的时间

# 初始化时间数组
timestamps=()

echo "时间记录脚本已启动"
echo "按回车键记录当前时间"
echo "按 c 键退出程序"

while true; do
    # 读取单个字符，不需要按回车
    read -rsn1 key
    
    case "$key" in
        # 回车键（空字符或换行符）
        ''|$'\n')
            current_time=$(date '+%Y-%m-%d %H:%M:%S')
            timestamps+=("$current_time")
            echo "已记录时间: $current_time"
            ;;
        # c键或C键
        c|C)
            echo
            echo "程序退出"
            break
            ;;
        # 其他键忽略
        *)
            ;;
    esac
done

# 显示所有记录的时间点
echo
echo "记录的时间点如下："
echo "=================="

if [ ${#timestamps[@]} -eq 0 ]; then
    echo "没有记录任何时间点"
else
    for ((i=0; i<${#timestamps[@]}; i++)); do
        echo "$((i+1)). ${timestamps[$i]}"
    done
    echo "总计记录了 ${#timestamps[@]} 个时间点"
fi
