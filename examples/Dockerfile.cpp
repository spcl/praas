FROM spcleth/praas:process as process
FROM spcleth/praas:cpp-builder as builder
ARG example

COPY --from=process /praas /praas

WORKDIR /function
ADD ${example} /source/
RUN /opt/bin/cmake \
    -DCMAKE_PREFIX_PATH="/praas/lib/cmake/praas" \
    -DCMAKE_BUILD_TYPE=Release \
    /source
RUN /opt/bin/cmake --build . -j4

FROM spcleth/praas:process

COPY --from=builder /function/functions /function

