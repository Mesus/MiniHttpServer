FROM ubuntu
MAINTAINER ahelloworld <tmj1165818439.tm@gmail.com>
RUN apt-get update
RUN apt-get -y install gcc make zlib1g.dev
COPY / /
EXPOSE 80
RUN make all
CMD ["/httpserver", "80", "./www", "gzip"]
