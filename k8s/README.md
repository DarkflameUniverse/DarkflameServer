## Kubernetes Setup

Debian
```
minikube start --driver kvm2
minikube mount /path/to/client:/luclient
```
or
```
minikube start --mount --mount-options="ro" --mount-string="/path/to/client:/luclient"
```

Fish Shell
```
eval $(minikube -p minikube docker-env)
```

To Build
```
docker build -t ghcr.io/darkflameuniverse/darkflameserver -f ./k8s/DLU.Dockerfile .
```

Install
```
kubectl apply -k k8s
```
