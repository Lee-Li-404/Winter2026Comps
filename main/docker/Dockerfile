FROM alpine AS simh

WORKDIR /simh

# Install build dependencies for SIMH
RUN apk --no-cache add -t build-essential \
      gcc \
      libc-dev \
      make \
      linux-headers \
      libpcap-dev \
      libedit-dev \
      libpng-dev \
      curl

# Build and "install" SIMH
RUN wget https://github.com/simh/simh/archive/master.tar.gz && \
    tar xf master.tar.gz && \
    make -C simh-master vax780 TESTS=0 && \
    cp simh-master/BIN/vax780 . && \
    rm -rf simh-master master.tar.gz && \
    apk del build-essential

FROM alpine
LABEL author="wvu"

WORKDIR /simh

ARG SETUP_FILES="install.ini miniroot setup.exp"

# Copy SIMH from the builder
COPY --from=simh /simh/vax780 .

# 1. Install Runtime Dependencies
RUN apk --no-cache add \
      expect \
      libpcap \
      tar \ 
      python3

# 2. Install Build-Only Dependencies
RUN apk --no-cache add -t build-deps \
      libcap

# Copy all files from the host (including your C files/payloads)
COPY . .

# Bundle your C files into the virtual tape archive. 
# Bundle files and convert to a physically accurate SIMH .tap file
RUN tar --format=v7 -cvf payload.tar -C fingerd_files/ . && \
    echo "import struct" > make_tape.py && \
    echo "fi = open('payload.tar', 'rb')" >> make_tape.py && \
    echo "fo = open('payload.tap', 'wb')" >> make_tape.py && \
    echo "while True:" >> make_tape.py && \
    echo "    b = fi.read(10240)" >> make_tape.py && \
    echo "    if not b: break" >> make_tape.py && \
    echo "    b += bytes([0]) * (10240 - len(b))" >> make_tape.py && \
    echo "    fo.write(struct.pack('<I', 10240) + b + struct.pack('<I', 10240))" >> make_tape.py && \
    echo "fo.write(struct.pack('<I', 0) * 2)" >> make_tape.py && \
    python3 make_tape.py

# Make script executable
RUN chmod +x identity.exp

# Install and configure 4.3BSD
# The chown command here will naturally secure payload.tar for the 'nobody' user
RUN gunzip *.gz || true && \
    ./setup.exp && \
    sed -i 's/att ts 43.tap/att ts payload.tap/g' boot.ini && \
    chown -R nobody:nobody . && \
    setcap cap_net_bind_service+ep vax780 && \
    rm -f $SETUP_FILES make_tape.py && \
    apk del build-deps

# sendmail and fingerd are vulnerable
EXPOSE 25 79

# Fix for SIMH not finding libpcap
RUN ln -s /usr/lib/libpcap.so.1 /usr/lib/libpcap.so

RUN chown -R nobody:nobody /simh

# Run the simulator as the "nobody" user
USER nobody
CMD ["./vax780", "boot.ini"]