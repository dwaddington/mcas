# Deploy on Kubernetes

MCAS server can run on a dedicated cluster as well as cloud native service. This document introduces how to setup MCAS in kubernetes environment. The document only deploy MCAS server as a pod. Users may change the pod into deployment/service based on their own needs. MCAS client can be deployed as a kubernetes pod using the same way.

## Setup Kubernetes Environment
- Follow [instructions](https://kubernetes.io/docs/setup/production-environment/container-runtimes/) to install a container runtime.
  
  **Note: only docker has been tested.**

- Follow [instructions](https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/) to install kubernetes runtime.
  
  **Note: To use RDMA and multus CNI, install earlier version of kubernetes (e.g., 1.13.0)**

- Follow [instructions](https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/create-cluster-kubeadm/) to setup kubernetes cluster.
  
  **Note: Only Flannel CNI has been tested.**
  
- (Optional) Follow [instructions](https://github.com/Mellanox/k8s-rdma-sriov-dev-plugin) to setup RDMA in HCA or SRIOV mode.

## Build Docker Image for MCAS:
- Build image:
```bash
docker build -f $MCAS_HOME/deploy/kubernetes/Dockerfile.mcas -t res-mcas-docker-local.artifactory.swg-devops.com/mcas:latest $MCAS_HOME
  ```
- Push image:
```bash
docker push res-mcas-docker-local.artifactory.swg-devops.com/mcas:latest
```

- (Optional) Run docker image local:
```bash
docker run --rm -it --privileged --cap-add=ALL -v /dev:/dev -v /lib/modules:/lib/modules --net=host --device=/dev/infiniband/uverbs0 --device=/dev/infiniband/rdma_cm --ulimit memlock=-1 res-mcas-docker-local.artifactory.swg-devops.com/mcas:latest bash
```

## Deploy MCAS as Pod:
- Make sure kubernetes cluster is up and run, in your control pane:
```bash
kubectl get nodes
```
- Create configmap with your own config file you want to pass
```bash
kubectl configmap mcas-config --from-file <mcas config file>
```
- Edit $MCAS_HOME/deploy/kubernetes/mcas-server.yaml to configure the MCAS pod (e.g., setup volume, parameters).
- Deploy MCAS on kubernetes cluster, in your control pane:
```bash
kubectl apply -f $MCAS_HOME/deploy/kubernetes/mcas-server.yaml
```
