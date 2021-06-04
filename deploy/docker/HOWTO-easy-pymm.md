# Getting started with PyMM

This document explains how to get started with PyMM using docker containers.  PyMM is a local-only
distribution of MCAS memory management that provides Python3 language integration. The advantage
of using the container version is that you don't need to build the MCAS distribution.  You
can download the image and simply run!  Furthermore, the container is not bloated with the
complete MCAS distribution, which is not needed if you are only using PyMM.

First, you need to ensure you have a pmem mounted partition (e.g., /mnt/pmem0).  Make
sure it has suitable permissions.

## Starting container from existing Dockerhub image

There are Ubuntu 18 and Fedora Core 32 based images available on docker hub.  The '-v' option
is needed to pass through the persistent memory mount to the container.

```bash
$ docker run -it -v /mnt/pmem0:/mnt/pmem0 dwaddington/pymm:ubuntu18

mcasuser@ce09291b91a0:~$ python3 -i sample.py 
Created shelf OK.
Created array 'x' on shelf OK. Data at ('0x10000300018', 800000000)
[-0.89107643 -0.96512335  0.59666344 ...  0.63990706 -0.21330142
 -0.67030775]
Sorted array 'x' on shelf OK. Data at ('0x10000300018', 800000000)
[-0.99999999 -0.99999998 -0.99999998 ...  0.99999997  0.99999998
  0.99999999]
Use s and s.x to access shelf...
>>> 
>>> s.used
74
```

## Setup Docker Environment

- Follow [Ubuntu install instructions](https://docs.docker.com/install/linux/docker-ce/ubuntu/) 
or [Fedora install instructions](https://docs.docker.com/engine/install/fedora/) to install docker.

- Add your user id into the docker group for non-root execution.


## Building your own container

Use the provided Dockerfile.pymm-ubuntu-18 to build your own container.


On Docker Hub (https://hub.docker.com/) create an account and a registry.

- Build image (e.g.):
```bash
docker build -t <your-docker-username>/pymm:ubuntu18 -f Dockerfile.pymm-ubuntu-18 .
```

```bash
docker build -f $MCAS_HOME/deploy/docker/Dockerfile.mcas-fc-27 -t <your-docker-username>/ibm-mcas-runtime:fc27 .
```

- (Optional) Push image to Docker Hub, e.g.:

```bash
docker login
docker push <your-docker-username>/pymm:ubuntu18
```
