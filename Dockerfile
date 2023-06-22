FROM docker.io/library/ubuntu:22.04
RUN apt update && apt full-upgrade -y && apt install -y git cmake g++ linux-headers-generic
RUN adduser --disabled-password umap && mkdir ~umap/umap
COPY . /home/umap/umap/
RUN cd ~umap/umap \
    && cmake . && make && make install \
    && ldconfig \
    && chown -R umap:umap ~umap
USER umap
WORKDIR /home/umap
