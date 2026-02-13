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

# 1. Install Runtime Dependencies (WE KEEP THESE NOW)
# 'expect' is needed for your identity script
# 'libpcap' is needed for networking
RUN apk --no-cache add \
      expect \
      libpcap

# 2. Install Build-Only Dependencies (WE DELETE THESE LATER)
RUN apk --no-cache add -t build-deps \
      libcap

# Copy files
COPY . .

# Make script executable
RUN chmod +x identity.exp

# Install and configure 4.3BSD
RUN gunzip *.gz && \
    ./setup.exp && \
    chown -R nobody:nobody . && \
    setcap cap_net_bind_service+ep vax780 && \
    rm -f $SETUP_FILES && \
    apk del build-deps  # <--- Only deletes libcap. Expect stays!

# sendmail and fingerd are vulnerable
EXPOSE 25 79

# Fix for SIMH not finding libpcap
RUN ln -s /usr/lib/libpcap.so.1 /usr/lib/libpcap.so

RUN chown -R nobody:nobody /simh

# Run the simulator as the "nobody" user
USER nobody
CMD ["./vax780", "boot.ini"]