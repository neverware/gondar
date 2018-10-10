FROM fedora:25

RUN dnf install -y \
	cmake \
	gcc \
	gcc-c++ \
	make \
	qt5-qtbase-devel \
	which \
	zlib-devel

ENV TREAT_WARNINGS_AS_ERRORS=true

ADD CMakeLists.txt Makefile /opt/gondar/
ADD infra /opt/gondar/infra
ADD minizip /opt/gondar/minizip
ADD plog /opt/gondar/plog
ADD resources /opt/gondar/resources
ADD src /opt/gondar/src
ADD test /opt/gondar/test
