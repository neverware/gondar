FROM fedora:25

RUN dnf install -y \
	cmake \
	gcc \
	gcc-c++ \
	qt5-qtbase-devel \
	zlib-devel

# Build minizip
ADD minizip /opt/gondar/minizip
WORKDIR /opt/gondar/build/minizip
RUN cmake ../../minizip
RUN make -j

# Build gondar
ADD Makefile.linux *.pro *.qrc /opt/gondar/
ADD images /opt/gondar/images
ADD plog /opt/gondar/plog
ADD src /opt/gondar/src

WORKDIR /opt/gondar/build
RUN qmake-qt5 ..
RUN make -j
