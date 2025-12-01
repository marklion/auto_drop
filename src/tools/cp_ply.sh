#!/bin/bash

# 获取当前日期用于默认时间范围
current_date=$(date "+%Y-%m-%d")

# 设置默认时间范围（今日00:00:00 到 23:59:59）
start_time="${1:-$current_date 00:00:00}"
end_time="${2:-$current_date 23:59:59}"
echo $start_time $end_time

# 转换时间格式为时间戳（兼容Mac的date命令）
start_timestamp=$(date -j -f "%Y-%m-%d %H:%M:%S" "$start_time" "+%s")
end_timestamp=$(date -j -f "%Y-%m-%d %H:%M:%S" "$end_time" "+%s")

# 在远程服务器执行命令
ssh ayx << EOF
  # 在远程服务器创建临时目录
  temp_dir=\$(mktemp -d)
  cd /mnt/data/auto_drop || exit 1

  # 查找匹配的.ply文件
  find . -type f -name "*.ply" -print0 | while IFS= read -r -d '' file; do
    # 从文件名提取时间部分（格式为 yyyy-mm-dd HH:MM:SS）
    filename=\$(basename "\$file")
    
    # 匹配格式为 xxxx-xx-xx xx:xx:xx 的时间字符串
    if [[ \$filename =~ ([0-9]{4}-[0-9]{2}-[0-9]{2}\ [0-9]{2}:[0-9]{2}:[0-9]{2}) ]]; then
      file_time="\${BASH_REMATCH[1]}"
      # 将文件名中的时间字符串转换为时间戳（远程服务器是Linux）
      file_timestamp=\$(date -d "\$file_time" "+%s" 2>/dev/null)

      # 检查时间戳是否在范围内
      if [[ -n \$file_timestamp ]] && 
         (( file_timestamp >= $start_timestamp )) && 
         (( file_timestamp <= $end_timestamp )); then
        cp -- "\$file" "\$temp_dir/"
      fi
    fi
  done

  # 打包匹配的文件
  tar -czf "ply_files.tar.gz" -C "\$temp_dir" .
  rm -rf "\$temp_dir"
EOF

# 将打包文件复制到本地
scp ayx:/mnt/data/auto_drop/ply_files.tar.gz ./
scp ayx:/mnt/data/auto_drop/rate_record.csv ./
scp ayx:/mnt/data/auto_drop/offset_log.csv ./

# 清理远程服务器上的临时文件
ssh ayx "rm -f /mnt/data/auto_drop/ply_files.tar.gz"

echo "操作完成！文件已保存为: $(pwd)/ply_files.tar.gz"
