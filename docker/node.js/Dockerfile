FROM node:12-alpine as build
RUN apk add gcc g++ autoconf automake make libtool musl-dev git python
RUN mkdir work && cd work && npm install better-sqlite3-aergolite
RUN cd work/node_modules/better-sqlite3-aergolite && rm -rf deps/aergolite deps/binn deps/secp256k1-vrf deps/static_libs build/Release/obj build/Release/obj.target build/Release/sqlite3.a

FROM node:12-alpine
WORKDIR /work
COPY --from=build /work /work
