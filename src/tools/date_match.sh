#!/bin/bash


file1="${1-rate_record.csv}"
file2="${2-date_r.csv}"

# 检查文件是否存在
if [ ! -f "$file1" ]; then
	echo "错误: 文件 $file1 不存在"
	exit 1
fi

if [ ! -f "$file2" ]; then
	echo "错误: 文件 $file2 不存在"
	exit 1
fi

# 创建临时文件
temp_file1=$(mktemp)
temp_file2=$(mktemp)

# 函数：标准化时间格式（转换为统一格式进行比较）
normalize_time() {
	local time_str="$1"
	echo ${time_str}
}

# 处理第二个文件，创建标准化时间列表
echo "处理第二个文件的时间列表..."
declare -A time_dict
while IFS=, read -r time_col || [ -n "$time_col" ]; do
	# 去除可能的引号和空格
	time_col=$(echo "$time_col" | sed 's/^["'\'']//; s/["'\'']$//; s/^[[:space:]]*//; s/[[:space:]]*$//')
	if [ -n "$time_col" ]; then
		normalized=$(normalize_time "$time_col")
		time_dict["$normalized"]=1
	fi
done < "$file2"

# 处理第一个文件
echo "处理第一个文件并匹配时间..."
output_file="output_statistic.csv"
echo '' > $output_file
match_count=0
# 处理CSV文件，考虑可能的引号和空格
while IFS=, read -r time_col value_col || [ -n "$time_col" ]; do
	# 去除可能的引号和空格
	time_col_clean=$(echo "$time_col" | sed 's/^["'\'']//; s/["'\'']$//; s/^[[:space:]]*//; s/[[:space:]]*$//')
	value_col_clean=$(echo "$value_col" | sed 's/^["'\'']//; s/["'\'']$//; s/^[[:space:]]*//; s/[[:space:]]*$//')

	if [ -n "$time_col_clean" ] && [ "0.00" != "$value_col_clean" ]; then
		normalized_time=$(normalize_time "$time_col_clean")
		# 检查是否在第二个文件的时间列表中
		if [ "${time_dict[$normalized_time]}" = "1" ]; then
			# 输出原时间格式、数值、和重复的数值
			echo "\"$value_col_clean\",\"$value_col_clean\"" >> "$output_file"
			match_count=$((match_count+1))
		else
			echo "\"$value_col_clean\"" >> "$output_file"
		fi
	fi
done < "$file1"

# 清理临时文件
rm -f "$temp_file1" "$temp_file2"

echo "处理完成！结果已保存到: $output_file"
echo "匹配到的行数: $match_count"

# 显示前几行结果（如果文件不为空）
if [ -s "$output_file" ]; then
	echo ""
	echo "前5行结果:"
	head -5 "$output_file"
fi
