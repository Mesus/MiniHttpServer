FROM ubuntu
MAINTAINER ahelloworld <tmj1165818439.tm@gmail.com>
RUN apt-get update
RUN apt-get -y install gcc make
EXPOSE 80
RUN make
ENTRYPOINT ./httpserver 80
