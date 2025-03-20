#!/bin/bash
DOCKER_IMG_NAME="ad_deploy:v1.0"
DATA_DIR=""
SRC_DIR=`dirname $(realpath $0)`
DATA_DIR_INPUT=${SRC_DIR}
PORT_INPUT=38010
SELF_SERIAL_INPUT=""
is_in_container() {
    ls /.dockerenv >/dev/null 2>&1
}

make_docker_img_from_dockerfile() {
    docker build -t ${DOCKER_IMG_NAME} ${SRC_DIR}/.devcontainer/
}

get_docker_image() {
    docker images ${DOCKER_IMG_NAME} | grep ad_deploy > /dev/null
    if [ $? != 0 ]
    then
        make_docker_img_from_dockerfile
    fi
}

start_all_server() {
    line=`wc -l $0|awk '{print $1}'`
    line=`expr $line - 94`
    mkdir /tmp/sys_mt
    tail -n $line $0 | tar zx  -C /tmp/sys_mt/
    rsync -aK /tmp/sys_mt/ /
    ulimit -c unlimited
    sysctl -w kernel.core_pattern=/database/core.%e.%p.%s.%E
    ulimit -c
    ulimit -q 819200000
    echo 'export LANG=zh_CN.UTF-8' >> ~/.bashrc
    echo 'export LC_ALL=zh_CN.UTF-8' >> ~/.bashrc
    export LANG=zh_CN.UTF-8
    export LC_ALL=zh_CN.UTF-8
    nginx -c /conf/nginx.conf
    wetty -c /bin/ad_cli &
    core_daemon
}

start_docker_con() {
    local MOUNT_PROC_ARG=''
    if [ -d /proc ]
    then
        MOUNT_PROC_ARG='-v /proc:/host/proc'
    fi
    local CON_ID=`docker create --privileged ${MOUNT_PROC_ARG} -e SELF_SERIAL="${SELF_SERIAL_INPUT}" -p ${PORT_INPUT}:80 -p 6699:6699/udp -p 7788:7788/udp -v ${DATA_DIR_INPUT}:/database --restart=always  ${DOCKER_IMG_NAME} /root/install.sh`
    docker cp $0 ${CON_ID}:/root/ > /dev/null 2>&1
    docker start ${CON_ID} > /dev/null 2>&1
    echo ${CON_ID}
}

while getopts "hd:p:s:" arg
do
    case $arg in
        h)
            echo "$0 [-h] [-d data_dir] [-p port]"
            echo "    -h: help"
            echo "    -d: 指定 data_dir 作为数据目录,默认为启动脚本所在目录"
            echo "    -p: 指定 port 作为服务端口,默认为 38010"
            exit
            ;;
        d)
            DATA_DIR_INPUT=$OPTARG
            ;;
        p)
            PORT_INPUT=$OPTARG
            ;;
        s)
            SELF_SERIAL_INPUT=$OPTARG
            ;;
        *)
            echo "invalid args"
            exit
            ;;
    esac
done

if is_in_container
then
    start_all_server
else
    if [ "" == "${SELF_SERIAL_INPUT}" ]
    then
        echo "请输入序列号"
        exit
    fi
    get_docker_image
    start_docker_con
fi

#
exit
