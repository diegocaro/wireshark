/* plugins.c
 * plugin routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define WS_LOG_DOMAIN LOG_DOMAIN_PLUGINS
#include "plugins.h"

#include <time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gmodule.h>

#include <wsutil/filesystem.h>
#include <wsutil/privileges.h>
#include <wsutil/file_util.h>
#include <wsutil/report_message.h>
#include <wsutil/wslog.h>

typedef struct _plugin {
    GModule        *handle;       /* handle returned by g_module_open */
    char           *name;         /* plugin name */
    const char     *version;      /* plugin version */
    const char     *type_name;    /* user-facing name (what it does). Should these be capitalized? */
} plugin;

#define TYPE_NAME_DISSECTOR "dissector"
#define TYPE_NAME_FILE_TYPE "file type"
#define TYPE_NAME_CODEC     "codec"


static GSList *plugins_module_list = NULL;


static inline const char *
type_to_signature(plugin_type_e type)
{
    switch (type) {
    case WS_PLUGIN_EPAN:
        return WIRESHARK_EPAN_PLUGIN;
    case WS_PLUGIN_WIRETAP:
        return WIRESHARK_WIRETAP_PLUGIN;
    case WS_PLUGIN_CODEC:
        return WIRESHARK_CODEC_PLUGIN;
    default:
        ws_error("Unknown plugin type: %u. Aborting.", (unsigned) type);
        break;
    }
    ws_assert_not_reached();
}

static inline const char *
type_to_name(plugin_type_e type)
{
    switch (type) {
    case WS_PLUGIN_EPAN:
        return TYPE_NAME_DISSECTOR;
    case WS_PLUGIN_WIRETAP:
        return TYPE_NAME_FILE_TYPE;
    case WS_PLUGIN_CODEC:
        return TYPE_NAME_CODEC;
    default:
        ws_error("Unknown plugin type: %u. Aborting.", (unsigned) type);
        break;
    }
    ws_assert_not_reached();
}

static void
free_plugin(void * data)
{
    plugin *p = (plugin *)data;
    g_module_close(p->handle);
    g_free(p->name);
    g_free(p);
}

static int
compare_plugins(gconstpointer a, gconstpointer b)
{
    return g_strcmp0((*(plugin *const *)a)->name, (*(plugin *const *)b)->name);
}

static bool
pass_plugin_version_compatibility(GModule *handle, const char *name)
{
    void * symb;
    int major, minor;

    if(!g_module_symbol(handle, "plugin_want_major", &symb)) {
        report_failure("The plugin '%s' has no \"plugin_want_major\" symbol", name);
        return false;
    }
    major = *(int *)symb;

    if(!g_module_symbol(handle, "plugin_want_minor", &symb)) {
        report_failure("The plugin '%s' has no \"plugin_want_minor\" symbol", name);
        return false;
    }
    minor = *(int *)symb;

    if (major != VERSION_MAJOR || minor != VERSION_MINOR) {
        report_failure("The plugin '%s' was compiled for Wireshark version %d.%d",
                            name, major, minor);
        return false;
    }

    return true;
}

// GLib and Qt allow ".dylib" and ".so" on macOS. Should we do the same?
#ifdef _WIN32
#define MODULE_SUFFIX ".dll"
#else
#define MODULE_SUFFIX ".so"
#endif

static void
scan_plugins_dir(GHashTable *plugins_module, const char *plugin_folder, plugin_type_e type, int level)
{
    GDir          *dir;
    const char    *name;            /* current file name */
    char          *plugin_file;     /* current file full path */
    GModule       *handle;          /* handle returned by g_module_open */
    void *         symbol;
    const char    *plug_version;
    plugin        *new_plug;
    char          *tmp_path;
    const char    *type_signature;

    /* Only recurse one level */
    if (level > 1) {
        return;
    }

    dir = g_dir_open(plugin_folder, 0, NULL);
    if (dir == NULL) {
        return;
    }

    ws_debug("Scanning plugins folder \"%s\" with type \"%s\"",
                plugin_folder, type_to_signature(type));

    while ((name = g_dir_read_name(dir)) != NULL) {
        /* Skip anything but files with .dll or .so. */
        if (!g_str_has_suffix(name, MODULE_SUFFIX)) {
            /* Recurse into subdirectories one level */
            tmp_path = g_build_filename(plugin_folder, name, (char *)NULL);
            if (g_file_test(tmp_path, G_FILE_TEST_IS_DIR)) {
                scan_plugins_dir(plugins_module, tmp_path, type, level + 1);
                g_free(tmp_path);
                continue;
            }
            g_free(tmp_path);
            continue;
        }

        plugin_file = g_build_filename(plugin_folder, name, (char *)NULL);
        handle = g_module_open(plugin_file, G_MODULE_BIND_LOCAL);
        if (handle == NULL) {
            /* g_module_error() provides file path. */
            report_failure("Couldn't load plugin '%s': %s", name,
                            g_module_error());
            g_free(plugin_file);
            continue;
        }

        if (!g_module_symbol(handle, "plugin_type", &symbol)) {
            report_failure("The plugin '%s' has no \"plugin_type\" symbol", name);
            g_module_close(handle);
            g_free(plugin_file);
            continue;
        }
        type_signature = (const char *)symbol;
        if (strcmp(type_signature, type_to_signature(type)) != 0) {
            /* Skip wrong type */
            ws_noisy("%s: skip type %s", name, type_signature);
            g_module_close(handle);
            g_free(plugin_file);
            continue;
        }

        if (!g_module_symbol(handle, "plugin_version", &symbol)) {
            report_failure("The plugin '%s' has no \"plugin_version\" symbol", name);
            g_module_close(handle);
            g_free(plugin_file);
            continue;
        }
        plug_version = (const char *)symbol;

        if (!pass_plugin_version_compatibility(handle, name)) {
            /* pass_plugin_version_compatibility() reports failures */
            g_module_close(handle);
            g_free(plugin_file);
            continue;
        }

        /* Search for the entry point for the plugin registration function */
        if (!g_module_symbol(handle, "plugin_register", &symbol)) {
            report_failure("The plugin '%s' has no \"plugin_register\" symbol", name);
            g_module_close(handle);
            g_free(plugin_file);
            continue;
        }

DIAG_OFF_PEDANTIC
        /* Found it, call the plugin registration function. */
        ((plugin_register_func)symbol)();
DIAG_ON_PEDANTIC

        new_plug = g_new(plugin, 1);
        new_plug->handle = handle;
        new_plug->name = g_strdup(name);
        new_plug->version = plug_version;
        new_plug->type_name = type_to_name(type);

        /* Add it to the list of plugins. */
        g_hash_table_replace(plugins_module, new_plug->name, new_plug);
        ws_info("Registered plugin: %s (%s)", new_plug->name, plugin_file);
        g_free(plugin_file);
    }
    ws_dir_close(dir);
}

/*
 * Scan for plugins.
 */
plugins_t *
plugins_init(plugin_type_e type)
{
    if (!g_module_supported())
        return NULL; /* nothing to do */

    GHashTable *plugins_module = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free_plugin);

    /*
     * Scan the global plugin directory.
     */
    scan_plugins_dir(plugins_module, get_plugins_dir_with_version(), type, 0);

    /*
     * If the program wasn't started with special privileges,
     * scan the users plugin directory.  (Even if we relinquish
     * them, plugins aren't safe unless we've *permanently*
     * relinquished them, and we can't do that in Wireshark as,
     * if we need privileges to start capturing, we'd need to
     * reclaim them before each time we start capturing.)
     */
    if (!started_with_special_privs()) {
        scan_plugins_dir(plugins_module, get_plugins_pers_dir_with_version(), type, 0);
    }

    plugins_module_list = g_slist_prepend(plugins_module_list, plugins_module);

    return plugins_module;
}

WS_DLL_PUBLIC void
plugins_get_descriptions(plugin_description_callback callback, void *callback_data)
{
    GPtrArray *plugins_array = g_ptr_array_new();
    GHashTableIter iter;
    void * value;

    for (GSList *l = plugins_module_list; l != NULL; l = l->next) {
        g_hash_table_iter_init (&iter, (GHashTable *)l->data);
        while (g_hash_table_iter_next (&iter, NULL, &value)) {
            g_ptr_array_add(plugins_array, value);
        }
    }

    g_ptr_array_sort(plugins_array, compare_plugins);

    for (unsigned i = 0; i < plugins_array->len; i++) {
        plugin *plug = (plugin *)plugins_array->pdata[i];
        callback(plug->name, plug->version, plug->type_name, g_module_name(plug->handle), callback_data);
    }

    g_ptr_array_free(plugins_array, true);
}

static void
print_plugin_description(const char *name, const char *version,
                         const char *description, const char *filename,
                         void *user_data _U_)
{
    printf("%-16s\t%s\t%s\t%s\n", name, version, description, filename);
}

void
plugins_dump_all(void)
{
    plugins_get_descriptions(print_plugin_description, NULL);
}

int
plugins_get_count(void)
{
    unsigned count = 0;

    for (GSList *l = plugins_module_list; l != NULL; l = l->next) {
        count += g_hash_table_size((GHashTable *)l->data);
    }
    return count;
}

void
plugins_cleanup(plugins_t *plugins)
{
    if (!plugins)
        return;

    plugins_module_list = g_slist_remove(plugins_module_list, plugins);
    g_hash_table_destroy((GHashTable *)plugins);
}

bool
plugins_supported(void)
{
    return g_module_supported();
}

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
