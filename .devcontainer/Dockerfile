FROM marklion/mt_deploy:v1.0
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y unzip
COPY yaml-cpp-master.zip /software/
WORKDIR /software/
RUN unzip yaml-cpp-master.zip
WORKDIR /software/yaml-cpp-master
RUN mkdir build
WORKDIR /software/yaml-cpp-master/build
RUN cmake .. && make && make install
WORKDIR /root/
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y lua5.3 liblua5.3-dev
COPY LuaBridge-master.zip /software/
WORKDIR /software/
RUN unzip LuaBridge-master.zip
WORKDIR /software/LuaBridge-master
RUN cp -a Source/LuaBridge /usr/include/
WORKDIR /root/
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libpam-dev
COPY .vimrc /root/
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y luarocks
RUN luarocks install lua-cjson
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y ne
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y libpcl-dev
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y net-tools ssh lsof language-pack-zh-hans
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y valgrind vim rsync python3-pip ffmpeg cups
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y gcc-multilib build-essential module-assistant
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y gdb nginx libpcre3 libpcre3-dev libxslt-dev
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libgd-dev cmake wget libpython3-dev npm sqlite3
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libsqlite3-dev uuid uuid-dev libcurl4-openssl-dev
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y automake bison flex g++ git libboost-all-dev
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libevent-dev libssl-dev libtool make pkg-config
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libeigen3-dev
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libboost-dev
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libpcap-dev
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y unzip qt5-default libqt5opengl5-dev libqt5x11extras5-dev libeigen3-dev libboost-all-dev libglew-dev libglvnd-dev
COPY vtk-master.zip /software/vtk-master.zip
WORKDIR /software
RUN unzip vtk-master.zip
WORKDIR /software/vtk-master
RUN mkdir build
WORKDIR /software/vtk-master/build
RUN cmake .. && make -j 4 && make install
WORKDIR /root/
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y libcdk5-dev