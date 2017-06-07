FROM neverware/gondar-build-mxe:v2

WORKDIR /opt/gondar
RUN git clone https://github.com/nmoinvaz/minizip
WORKDIR /opt/gondar/minizip
RUN git checkout dc3ad01e3d5928e9105f770b7e896a8e9fe0d3b4
RUN mkdir build
WORKDIR /opt/gondar/minizip/build
ENV PATH=$PATH:/opt/gondar/mxe/usr/bin
RUN /opt/mxe/usr/bin/i686-w64-mingw32.static-cmake ..
RUN make

WORKDIR /opt/gondar
ADD *.c *.cc *.h *.pro *.qrc /opt/gondar/
ADD images /opt/gondar/images

ENV PATH=$PATH:/opt/mxe/usr/bin
RUN /opt/mxe/usr/bin/i686-w64-mingw32.static-qmake-qt5 gondar.pro
RUN make -j4 release
