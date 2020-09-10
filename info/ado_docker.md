# Deploy ADO Container

ADO processes can be launched as a separate linux process or a docker container.
By default, ADOs are launched as processes and no actions are required. To
launch ADO as a docker container, you need to first build the ADO docker image.
This document includes instructions how to deploy and launch ADO as a
docker container.

## Setup Docker Environment
- Follow [instructions](https://docs.docker.com/install/linux/docker-ce/ubuntu/) to install
docker.

- Add yourself into docker group for no-root execution.

## Build Docker Image for ADO:
  ```bash
  docker build -f $MCAS_HOME/deploy/docker/Dockerfile.ado -t res-mcas-docker-local.artifactory.swg-devops.com/ado:latest $MCAS_HOME
  ```

**Note: you can also do a `docker push` so that next time you can directly pull the image from the registry.**

## Deploy ADO container:
Launch mcas server with environment variable `USE_DOCKER`:
```bash
USE_DOCKER=1 mcas --config <file>
```
