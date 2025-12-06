#ifndef LIBVIRT_HELPER_H
#define LIBVIRT_HELPER_H

/* Helpers C exposés à Python via ctypes */

int connect_hypervisor(const char *uri);

int list_active_domains(const char *uri);
int list_inactive_domains(const char *uri);

int start_domain(const char *uri, const char *domName);
int stop_domain(const char *uri, const char *domName);

int save_domain(const char *uri, const char *domName, const char *path);
int restore_domain(const char *uri, const char *path);

int ensure_persistent(const char *uri, const char *domName);
int reboot_domain(const char *uri, const char *domName);

/* Migration à chaud (live) d’une VM vers un autre hyperviseur */
int migrate_domain(const char *src_uri,
                   const char *domName,
                   const char *dest_host,
                   const char *dest_name);

#endif /* LIBVIRT_HELPER_H */
