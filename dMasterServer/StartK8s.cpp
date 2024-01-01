#include "Start.h"
#include "Logger.h"

extern "C" {
#include <kubernetes/config/kube_config.h>
#include <kubernetes/config/incluster_config.h>
#include <kubernetes/include/apiClient.h>
#include <kubernetes/model/v1_daemon_set_spec.h>
#include <kubernetes/api/CoreV1API.h>
#include <kubernetes/api/AppsV1API.h>
}

#include <cstdlib>
#include <cstdio>
#include <cerrno>

namespace k8s {
	using DaemonSet = std::unique_ptr<v1_daemon_set_t, decltype(v1_daemon_set_free)&>;
	using Deployment = std::unique_ptr<v1_deployment_t, decltype(v1_deployment_free)&>;
	using PodSpec = std::unique_ptr<v1_pod_spec_t, decltype(v1_pod_spec_free)&>;
	using Pod = std::unique_ptr<v1_pod_t, decltype(v1_pod_free)&>;
}

class ApiClient {
	char* basePath = NULL;
	sslConfig_t* sslConfig = NULL;
	list_t* apiKeys = NULL;
	apiClient_t* k8sApiClient = NULL;

public:
	ApiClient();
	~ApiClient();

	k8s::DaemonSet CreateOrReplaceDeamonSet(const std::string& namespace_, k8s::DaemonSet& daemonset);
	k8s::Deployment CreateOrReplaceDeployment(const std::string& namespace_, k8s::Deployment& deployment);
	k8s::Pod SpawnPod(const std::string& namespace_, k8s::Pod& pod);
	k8s::PodSpec CreatePodSpec(const std::string& entrypoint, int container_port, int host_port, const std::vector<std::string>& args);
};

k8s::PodSpec ApiClient::CreatePodSpec(const std::string& entrypoint, int container_port, int host_port, const std::vector<std::string>& args) {
	v1_pod_spec_t* pod_spec;
	
	char* podTemplateName = strdup("pod-template");
	char* podTemplateNamespace = strdup("default");
	v1_pod_template_t* pod_template = CoreV1API_readNamespacedPodTemplate(k8sApiClient, podTemplateName, podTemplateNamespace, NULL);
	LOG("code = %d", k8sApiClient->response_code);
	Game::logger->Flush();
	if (k8sApiClient->response_code >= 400) {
		throw new std::runtime_error("pod-template not found");
	}
	
	pod_spec = pod_template->_template->spec;
	pod_template->_template->spec = NULL;
	v1_pod_template_free(pod_template);
	free(podTemplateName);
	free(podTemplateNamespace);

	v1_container_t* con = (v1_container_t*) list_getElementAt(pod_spec->containers, 0)->data;

	/* set command for container */
	list_t* commandlist = list_createList();
	char* cmd = strdup(entrypoint.c_str());
	list_addElement(commandlist, cmd);
	// FIXME: old value?
	con->command = commandlist;

	if (!args.empty()) {
		list_t *arglist = list_createList();
		for (auto it = args.begin(); it != args.end(); it++) {
			char *arg1 = strdup(it->c_str());
			list_addElement(arglist, arg1);
		}
		// FIXME: old value?
		con->args = arglist;
	}

	v1_container_port_t* port = (v1_container_port_t*) list_getElementAt(con->ports, 0)->data;
	port->container_port = container_port;
	port->host_port = host_port;

	return std::move(k8s::PodSpec(pod_spec, v1_pod_spec_free));
}

k8s::DaemonSet CreateDeamonSet(const std::string& name, const std::string& app_label, k8s::PodSpec podSpec) {
	v1_daemon_set_t* daemonsetinfo = (v1_daemon_set_t*)calloc(1, sizeof(v1_daemon_set_t));
	daemonsetinfo->api_version = strdup("apps/v1");
	daemonsetinfo->kind = strdup("DaemonSet");

	daemonsetinfo->metadata = (v1_object_meta_t*)calloc(1, sizeof(v1_object_meta_t));
	daemonsetinfo->metadata->name = strdup(name.c_str());

	daemonsetinfo->spec = (v1_daemon_set_spec_t*)calloc(1, sizeof(v1_daemon_set_spec_t));
	list_t* match_labels = list_createList();
	list_addElement(match_labels, keyValuePair_create(strdup("app.kubernetes.io/name"), strdup(app_label.c_str())));
	daemonsetinfo->spec->selector = (v1_label_selector_t*)calloc(1, sizeof(v1_label_selector_t));
	daemonsetinfo->spec->selector->match_labels = match_labels;

	v1_pod_template_spec_t* pod_template_spec = (v1_pod_template_spec_t*)calloc(1, sizeof(v1_pod_template_spec_t));
	pod_template_spec->metadata = (v1_object_meta_t*)calloc(1, sizeof(v1_object_meta_t));
	list_t* labels = list_createList();
	list_addElement(labels, keyValuePair_create(strdup("app.kubernetes.io/name"), strdup(app_label.c_str())));
	pod_template_spec->metadata->labels = labels;
	pod_template_spec->spec = podSpec.release();

	daemonsetinfo->spec->_template = pod_template_spec;
	return std::move(k8s::DaemonSet(daemonsetinfo, v1_daemon_set_free));
}

k8s::Deployment CreateDeployment(const std::string& name, const std::string& app_label, k8s::PodSpec podSpec) {
	v1_deployment_t* deployment = (v1_deployment_t*)calloc(1, sizeof(v1_deployment_t));
	deployment->api_version = strdup("apps/v1");
	deployment->kind = strdup("Deployment");

	deployment->metadata = (v1_object_meta_t*)calloc(1, sizeof(v1_object_meta_t));
	deployment->metadata->name = strdup(name.c_str());

	deployment->spec = (v1_deployment_spec_t*)calloc(1, sizeof(v1_deployment_spec_t));
	deployment->spec->replicas = 1;

	list_t* match_labels = list_createList();
	list_addElement(match_labels, keyValuePair_create(strdup("app.kubernetes.io/name"), strdup(app_label.c_str())));
	deployment->spec->selector = (v1_label_selector_t*)calloc(1, sizeof(v1_label_selector_t));
	deployment->spec->selector->match_labels = match_labels;

	v1_pod_template_spec_t* pod_template_spec = (v1_pod_template_spec_t*)calloc(1, sizeof(v1_pod_template_spec_t));
	pod_template_spec->metadata = (v1_object_meta_t*)calloc(1, sizeof(v1_object_meta_t));
	list_t* labels = list_createList();
	list_addElement(labels, keyValuePair_create(strdup("app.kubernetes.io/name"), strdup(app_label.c_str())));
	pod_template_spec->metadata->labels = labels;
	pod_template_spec->spec = podSpec.release();

	deployment->spec->_template = pod_template_spec;
	return std::move(k8s::Deployment(deployment, v1_deployment_free));
}

char shiftNumbers(char c) {
	return c < 'a' ? c - ('a' - 10 - '0') : c;
}

k8s::Pod CreatePod(const std::string& name, const std::string& app_label, k8s::PodSpec podSpec) {
	v1_pod_t* pod = (v1_pod_t*)calloc(1, sizeof(v1_pod_t));
	pod->api_version = strdup("v1");
	pod->kind = strdup("Pod");

	std::uniform_int_distribution<char> distribution('a' - 10, 'z');
	std::string pod_name = name + '-';
	for (int i = 0; i < 5; i++) {
		pod_name += shiftNumbers(distribution(Game::randomEngine));
	}

	pod->metadata = (v1_object_meta_t*)calloc(1, sizeof(v1_object_meta_t));
	pod->metadata->name = strdup(pod_name.c_str());

	list_t* labels = list_createList();
	list_addElement(labels, keyValuePair_create(strdup("app.kubernetes.io/name"), strdup(app_label.c_str())));
	pod->metadata->labels = labels;

	pod->spec = podSpec.release();
	return std::move(k8s::Pod(pod, v1_pod_free));
}

ApiClient::ApiClient() {
	int rc = 0;
	rc = load_incluster_config(&basePath, &sslConfig, &apiKeys);
	if (0 == rc) {
		k8sApiClient = apiClient_create_with_base_path(basePath, sslConfig, apiKeys);
	} else {
		throw std::runtime_error("Cannot load kubernetes configuration in cluster.");
	}
}

k8s::DaemonSet ApiClient::CreateOrReplaceDeamonSet(const std::string& namespace_, k8s::DaemonSet& daemonset) {
	char* podNamespace = strdup(namespace_.c_str());

	v1_daemon_set_t* created;
	v1_daemon_set_t* existing = AppsV1API_readNamespacedDaemonSet(k8sApiClient, daemonset->metadata->name, podNamespace, NULL);
	LOG("read code=%ld", k8sApiClient->response_code);
	if (k8sApiClient->response_code == 200) {
		daemonset->metadata->resource_version = existing->metadata->resource_version;
		created = AppsV1API_replaceNamespacedDaemonSet(k8sApiClient, daemonset->metadata->name, podNamespace, daemonset.get(), NULL, NULL, NULL, NULL);
	} else {
		created = AppsV1API_createNamespacedDaemonSet(k8sApiClient, podNamespace, daemonset.get(), NULL, NULL, NULL, NULL);
	}
	LOG("code=%ld", k8sApiClient->response_code);

	free(podNamespace);
	v1_daemon_set_free(existing);
	return k8s::DaemonSet(created, v1_daemon_set_free);
}

k8s::Pod ApiClient::SpawnPod(const std::string& namespace_, k8s::Pod& pod) {
	char* podNamespace = strdup(namespace_.c_str());

	// Kill the pod (if it exists)
	//int grace_period_seconds = 0;
	//v1_pod_t* existing = CoreV1API_deleteNamespacedPod(k8sApiClient, pod->metadata->name, podNamespace, NULL, NULL, &grace_period_seconds, NULL, NULL, NULL);
	//LOG("read code=%ld", k8sApiClient->response_code);
	// Spawn a new pod
	v1_pod_t* created = CoreV1API_createNamespacedPod(k8sApiClient, podNamespace, pod.get(), NULL, NULL, NULL, NULL);
	LOG("code=%ld", k8sApiClient->response_code);

	free(podNamespace);
	//v1_pod_free(existing);
	return k8s::Pod(created, v1_pod_free);
}

k8s::Deployment ApiClient::CreateOrReplaceDeployment(const std::string& namespace_, k8s::Deployment& deployment) {
	char* podNamespace = strdup(namespace_.c_str());

	v1_deployment_t* created;
	v1_deployment_t* existing = AppsV1API_readNamespacedDeployment(k8sApiClient, deployment->metadata->name, podNamespace, NULL);
	LOG("read code=%ld", k8sApiClient->response_code);
	if (k8sApiClient->response_code == 200) {
		deployment->metadata->resource_version = existing->metadata->resource_version;
		created = AppsV1API_replaceNamespacedDeployment(k8sApiClient, deployment->metadata->name, podNamespace, deployment.get(), NULL, NULL, NULL, NULL);
	} else {
		created = AppsV1API_createNamespacedDeployment(k8sApiClient, podNamespace, deployment.get(), NULL, NULL, NULL, NULL);
	}
	LOG("code=%ld", k8sApiClient->response_code);

	free(podNamespace);
	v1_deployment_free(existing);
	return k8s::Deployment(created, v1_deployment_free);
}

ApiClient::~ApiClient() {
	free_client_config(basePath, sslConfig, apiKeys);
	basePath = NULL;
	sslConfig = NULL;
	apiKeys = NULL;

	apiClient_free(k8sApiClient);
	k8sApiClient = NULL;
	apiClient_unsetupGlobalEnv();
}

void StartAuthServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Auth will not be restarted.");
		return;
	}
	LOG("Starting AuthServer");
	ApiClient apiClient;
	k8s::PodSpec podSpec = apiClient.CreatePodSpec("/app/AuthServer", 1001, 1001, {});
	//k8s::DaemonSet daemonset = CreateDeamonSet("auth-servers", "auth-server", podSpec);
	k8s::Pod pod = CreatePod("auth-server-pod", "auth-server", std::move(podSpec));
	apiClient.SpawnPod("default", pod);
	LOG("Done starting AuthServer");
}

void StartChatServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Chat will not be restarted.");
		return;
	}
	LOG("Starting ChatServer");
	ApiClient apiClient;
	k8s::PodSpec podSpec = apiClient.CreatePodSpec("/app/ChatServer", 2005, 0, {});
	//k8s::Deployment deployment = CreateDeployment("chat-server", "chat-server", podSpec);
	k8s::Pod pod = CreatePod("chat-server-pod", "chat-server", std::move(podSpec));
	apiClient.SpawnPod("default", pod);
	LOG("Done starting ChatServer");
}

void StartWorldServer(LWOMAPID mapID, uint16_t port, LWOINSTANCEID instanceID, int maxPlayers, LWOCLONEID cloneID) {
	LOG("Starting WorldServer %d %d %d (port %d)", mapID, instanceID, cloneID, port);

	std::vector<std::string> args = {
		"-zone",
		std::to_string(mapID),
		"-port",
		std::to_string(port),
		"-instance",
		std::to_string(instanceID),
		"-maxclients",
		std::to_string(maxPlayers),
		"-clone",
		std::to_string(cloneID),
	};

	ApiClient apiClient;
	int host_port = (int) port; // for now
	k8s::PodSpec podSpec = apiClient.CreatePodSpec("/app/WorldServer", (int) port, host_port, args);
	std::string name = "world-" + std::to_string(mapID) + "-" + std::to_string(instanceID) + "-" + std::to_string(cloneID);
	k8s::Pod pod = CreatePod(name, "world-server", std::move(podSpec));
	apiClient.SpawnPod("default", pod);
	LOG("Done starting WorldServer");
}
