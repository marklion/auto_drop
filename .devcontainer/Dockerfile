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