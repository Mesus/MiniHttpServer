FROM ubuntu
MAINTAINER ahelloworld <tmj1165818439.tm@gmail.com>
RUN apt-get update
RUN apt-get -y install gcc make
COPY / /
EXPOSE 80
RUN make all
ENTRYPOINT ["/httpserver", "80"]
