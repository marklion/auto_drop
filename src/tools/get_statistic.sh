#!/bin/bash
scp $1 ayx:/mnt/data/auto_drop/date_r.csv
ssh ayx "cd /mnt/data/auto_drop && bash -s" < ./date_match.sh

# 将打包文件复制到本地
scp ayx:/mnt/data/auto_drop/output_statistic.csv ./

