FROM alpine:latest as build
WORKDIR /work
RUN mkdir /usr/local/include

# Install the build tools

RUN apk add gcc autoconf automake make libtool musl-dev readline-dev git

# Install binn

RUN git clone --depth=1 https://github.com/liteserver/binn && \
cd binn && \
make && \
make install

# Install libuv

RUN git clone --depth=1 https://github.com/libuv/libuv && \
cd libuv && \
./autogen.sh && \
./configure && \
make && \
make install

# Install libsecp256k1-vrf

RUN git clone --depth=1 https://github.com/aergoio/secp256k1-vrf && \
cd secp256k1-vrf && \
./autogen.sh && \
./configure --disable-benchmarks && \
make && \
make install

# Install AergoLite

RUN git clone --depth=1 https://github.com/aergoio/aergolite && \
cd aergolite && \
make && \
make install

# Copy files

RUN cd /usr/local/lib && cp libbinn.so.3.0 libuv.so.1.0.0 libsecp256k1-vrf.so.0.0.0 libaergolite.so.0.0.1 /
RUN cd /usr/local/include && cp sqlite3.h /
RUN cd /usr/local/bin && cp sqlite3 /




FROM python:3-alpine as build2
RUN apk add gcc g++ autoconf automake make libtool musl-dev git
RUN git clone https://github.com/NixOS/patchelf && \
cd patchelf && \
./bootstrap.sh && \
./configure && \
make && \
make install
RUN patchelf --replace-needed libsqlite3.so.0 libaergolite.so /usr/local/lib/python3.9/lib-dynload/_sqlite3.cpython-39-x86_64-linux-gnu.so




FROM python:3-alpine
WORKDIR /work
COPY --from=build libbinn.so.3.0 libuv.so.1.0.0 libsecp256k1-vrf.so.0.0.0 libaergolite.so.0.0.1 /usr/local/lib/
COPY --from=build sqlite3.h /usr/local/include/
COPY --from=build sqlite3 /usr/local/bin/
# create links
RUN cd /usr/local/lib && \
ln -s libbinn.so.3.0 libbinn.so.3 && \
ln -s libuv.so.1.0.0 libuv.so.1 && \
ln -s libuv.so.1.0.0 libuv.so && \
ln -s libsecp256k1-vrf.so.0.0.0 libsecp256k1-vrf.so.0 && \
ln -s libsecp256k1-vrf.so.0.0.0 libsecp256k1-vrf.so && \
ln -s libaergolite.so.0.0.1 libaergolite.so.0 && \
ln -s libaergolite.so.0 libaergolite.so && \
cd - && \
mkdir /usr/local/lib/aergolite && \
cd /usr/local/lib/aergolite && \
ln -s ../libaergolite.so.0.0.1 libsqlite3.so.0 && \
ln -s libsqlite3.so.0 libsqlite3.so && \
cd -
RUN apk --no-cache add readline
COPY --from=build2 /usr/local/lib/python3.9/lib-dynload/_sqlite3.cpython-39-x86_64-linux-gnu.so /usr/local/lib/python3.9/lib-dynload/_sqlite3.cpython-39-x86_64-linux-gnu.so
