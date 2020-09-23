# Deploy and Run MCAS inside a Docker Container

Similar to containerizing the ADO process, the MCAS server process can
also run within a container. This document shows how to build and run
the MCAS server inside container. Note this is different from [ADO Docker Containerization](./ado-docker.md) which only launches ADO processes inside container.

## Setup Docker Environment
- Follow [Ubuntu install instructions](https://docs.docker.com/install/linux/docker-ce/ubuntu/) 
or [Fedora install instructions](https://docs.docker.com/engine/install/fedora/)to install docker.

- Add yours user id into the docker group for non-root execution.

## Setup host kernel module
Follow [Quick Start](./quick_start.md) for system configuration and instructions on how to install kernel modules ```xpmem.ko``` and ```mcasmod```.

## Build Docker Image for MCAS (Optional if you do not have access to registry):

On Docker Hub (https://hub.docker.com/) create an account and a registry.

- Build image:
```bash
docker build -f $MCAS_HOME/deploy/docker/Dockerfile.mcas-ubuntu-18 -t <your-docker-username>/ibm-mcas-runtime .
  ```

- (Optional) Push image to Docker Hub, e.g.:
```bash
docker push <your_username>/ibm-mcas-runtime
```

## Run MCAS Docker Image:
- If running with RDMA:
```bash
docker run --rm -it --privileged --cap-add=ALL -v /dev:/dev -v /lib/modules:/lib/modules --net=host --device=/dev/infiniband/uverbs0 --device=/dev/infiniband/rdma_cm --ulimit memlock=-1 <your-docker-username</ibm-mcas-runtime bash
```

- If running with standard sockets:
```bash
docker run --rm -it --privileged --cap-add=ALL -v /dev:/dev -v /lib/modules:/lib/modules --ulimit memlock=-1 res-mcas-docker-local.artifactory.swg-devops.com/mcas:latest bash
```

After getting into the bash console, your mcas binary is located in
```/mcas/build/dist```. Then you can continue following [Quick
Start](./quick_start.md) to launch MCAS server. Another container can
be launched as a client with the same command as server. See
[Kubernetes](./kubernetes.md) for deploying containers on a kubernetes
cluster (different nodes).


