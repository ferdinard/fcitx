/***************************************************************************
 *   Copyright (C) 2010~2010 by CSSlayer                                   *
 *   wengxt@gmail.com                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

/**
 * @file addon.c
 * Addon Support for fcitx
 * @author CSSlayer wengxt@gmail.com
 */

#include <sys/stat.h>
#include <libintl.h>
#include <dlfcn.h>

#include "fcitx/fcitx.h"
#include "addon.h"
#include "fcitx-config/xdg.h"
#include "fcitx-utils/log.h"
#include "fcitx-utils/utils.h"
#include "instance.h"
#include "instance-internal.h"
#include "addon-internal.h"

CONFIG_BINDING_BEGIN(FcitxAddon)
CONFIG_BINDING_REGISTER("Addon", "Name", name)
CONFIG_BINDING_REGISTER("Addon", "GeneralName", generalname)
CONFIG_BINDING_REGISTER("Addon", "Comment", comment)
CONFIG_BINDING_REGISTER("Addon", "Category", category)
CONFIG_BINDING_REGISTER("Addon", "Enabled", bEnabled)
CONFIG_BINDING_REGISTER("Addon", "Library", library)
CONFIG_BINDING_REGISTER("Addon", "Type", type)
CONFIG_BINDING_REGISTER("Addon", "Dependency", depend)
CONFIG_BINDING_REGISTER("Addon", "Priority", priority)
CONFIG_BINDING_REGISTER("Addon", "SubConfig", subconfig)
CONFIG_BINDING_REGISTER("Addon", "IMRegisterMethod", registerMethod)
CONFIG_BINDING_REGISTER("Addon", "IMRegisterArgument", registerArgument)
CONFIG_BINDING_REGISTER("Addon", "UIFallback", uifallback)
CONFIG_BINDING_REGISTER("Addon", "Advance", advance)
CONFIG_BINDING_END()

static const UT_icd function_icd = {sizeof(void*), 0, 0 , 0};
static const UT_icd addon_icd = {sizeof(FcitxAddon), NULL , NULL, FcitxAddonFree};
static int AddonPriorityCmp(const void* a, const void* b)
{
    FcitxAddon *aa = (FcitxAddon*)a, *ab = (FcitxAddon*)b;
    return aa->priority - ab->priority;
}

FCITX_EXPORT_API
void FcitxAddonsInit(UT_array* addons)
{
    utarray_init(addons, &addon_icd);
}

void* FcitxGetSymbol(void* handle, const char* addonName, const char* symbolName)
{
    char* escapedAddonName;
    asprintf(&escapedAddonName, "%s_%s", addonName, symbolName);
    char* p = escapedAddonName;
    while (*p) {
        if (*p == '-') {
            *p = '_';
        }
        p++;
    }
    void* result = dlsym(handle, escapedAddonName);
    if (!result)
        result = dlsym(handle, symbolName);

    return result;
}

/**
 * Load Addon Info
 */
FCITX_EXPORT_API
void FcitxAddonsLoad(UT_array* addons)
{
    char **addonPath;
    size_t len;
    size_t i = 0;
    utarray_clear(addons);

    FcitxStringHashSet* sset = FcitxXDGGetFiles(
                              "addon",
                              NULL,
                              ".conf"
                          );
    addonPath = FcitxXDGGetPathWithPrefix(&len, "addon");
    char **paths = malloc(sizeof(char*) * len);
    for (i = 0; i < len ; i ++)
        paths[i] = NULL;
    HASH_FOREACH(string, sset, FcitxStringHashSet) {
        int i = 0;
        for (i = len - 1; i >= 0; i--) {
            asprintf(&paths[i], "%s/%s", addonPath[len - i - 1], string->name);
            FcitxLog(DEBUG, "Load Addon Config File:%s", paths[i]);
        }
        FcitxLog(INFO, _("Load Addon Config File:%s"), string->name);
        FcitxConfigFile* cfile = FcitxConfigParseMultiConfigFile(paths, len, FcitxAddonGetConfigDesc());
        if (cfile) {
            utarray_extend_back(addons);
            FcitxAddon *a = (FcitxAddon*) utarray_back(addons);
            utarray_init(&a->functionList, &function_icd);
            FcitxAddonConfigBind(a, cfile, FcitxAddonGetConfigDesc());
            FcitxConfigBindSync((FcitxGenericConfig*)a);
            FcitxLog(DEBUG, _("Addon Config %s is %s"), string->name, (a->bEnabled) ? "Enabled" : "Disabled");
        }

        for (i = 0; i < len ; i ++) {
            free(paths[i]);
            paths[i] = NULL;
        }
    }
    free(paths);

    FcitxXDGFreePath(addonPath);

    fcitx_utils_free_string_hash_set(sset);

    utarray_sort(addons, AddonPriorityCmp);
}

FCITX_EXPORT_API
void FcitxInstanceResolveAddonDependency(FcitxInstance* instance)
{
    UT_array* addons = &instance->addons;
    boolean remove = true;
    FcitxAddon *addon;
    FcitxAddon *uiaddon = NULL, *uifallbackaddon = NULL;

    /* override the enable and disable option */
    for (addon = (FcitxAddon *) utarray_front(addons);
         addon != NULL;
         addon = (FcitxAddon *) utarray_next(addons, addon)) {
        if (instance->disableList && fcitx_utils_string_list_contains(instance->disableList, addon->name))
            addon->bEnabled = false;
        else if (instance->enableList && fcitx_utils_string_list_contains(instance->enableList, addon->name))
            addon->bEnabled = true;
    }

    /* choose ui */
    for (addon = (FcitxAddon *) utarray_front(addons);
            addon != NULL;
            addon = (FcitxAddon *) utarray_next(addons, addon)) {
        if (addon->category == AC_UI) {
            if (instance->uiname == NULL) {
                if (addon->bEnabled) {
                    uiaddon = addon;
                    break;
                }
            } else {
                if (strcmp(instance->uiname, addon->name) == 0) {
                    addon->bEnabled = true;
                    uiaddon = addon;
                    break;
                }
            }
        }
    }

    if (uiaddon && uiaddon->uifallback) {
        for (addon = (FcitxAddon *) utarray_front(addons);
                addon != NULL;
                addon = (FcitxAddon *) utarray_next(addons, addon)) {
            if (addon->category == AC_UI && addon->bEnabled && strcmp(uiaddon->uifallback, addon->name) == 0) {
                FcitxAddon temp;
                int uiidx = utarray_eltidx(addons, uiaddon);
                int fallbackidx = utarray_eltidx(addons, addon);
                if (fallbackidx < uiidx) {
                    temp = *uiaddon;
                    *uiaddon = *addon;
                    *addon = temp;

                    /* they swapped, addon is normal ui, and ui addon is fallback */
                    uifallbackaddon = uiaddon;
                    uiaddon = addon;
                }
                else {
                    uifallbackaddon = addon;
                }
                break;
            }
        }
    }

    for (addon = (FcitxAddon *) utarray_front(addons);
            addon != NULL;
            addon = (FcitxAddon *) utarray_next(addons, addon)) {
        if (addon->category == AC_UI && addon != uiaddon && addon != uifallbackaddon) {
            addon->bEnabled = false;
        }
    }

    while (remove) {
        remove = false;
        for (addon = (FcitxAddon *) utarray_front(addons);
                addon != NULL;
                addon = (FcitxAddon *) utarray_next(addons, addon)) {
            if (!addon->bEnabled)
                continue;
            UT_array* dependlist = fcitx_utils_split_string(addon->depend, ',');
            boolean valid = true;
            char **depend = NULL;
            for (depend = (char **) utarray_front(dependlist);
                    depend != NULL;
                    depend = (char **) utarray_next(dependlist, depend)) {
                if (!FcitxAddonsIsAddonAvailable(addons, *depend)) {
                    valid = false;
                    break;
                }
            }

            utarray_free(dependlist);
            if (!valid) {
                FcitxLog(WARNING, _("Disable addon %s, dependency %s can not be satisfied."), addon->name, addon->depend);
                addon->bEnabled = false;
            }
        }
    }
}

FCITX_EXPORT_API
boolean FcitxAddonsIsAddonAvailable(UT_array* addons, const char* name)
{
    FcitxAddon *addon;
    for (addon = (FcitxAddon *) utarray_front(addons);
            addon != NULL;
            addon = (FcitxAddon *) utarray_next(addons, addon)) {
        if (addon->bEnabled && strcmp(name, addon->name) == 0)
            return true;
    }
    return false;
}

FCITX_EXPORT_API
FcitxAddon* FcitxAddonsGetAddonByName(UT_array* addons, const char* name)
{
    FcitxAddon *addon;
    for (addon = (FcitxAddon *) utarray_front(addons);
            addon != NULL;
            addon = (FcitxAddon *) utarray_next(addons, addon)) {
        if (addon->bEnabled && strcmp(name, addon->name) == 0)
            return addon;
    }
    return NULL;
}

/**
 * Load addon.desc file
 *
 * @return the description of addon configure.
 */
FCITX_EXPORT_API
CONFIG_DESC_DEFINE(FcitxAddonGetConfigDesc, "addon.desc")

FCITX_EXPORT_API
void FcitxAddonFree(void* v)
{
    FcitxAddon *addon = (FcitxAddon*) v;
    if (!addon)
        return ;
    FcitxConfigFreeConfigFile(addon->config.configFile);
    free(addon->name);
    free(addon->library);
    free(addon->comment);
    free(addon->generalname);
    free(addon->depend);
    free(addon->subconfig);
}

boolean FcitxCheckABIVersion(void* handle, const char* addonName)
{
    int* version = (int*) FcitxGetSymbol(handle, addonName, "ABI_VERSION");
    if (!version)
        return false;
    if (*version < FCITX_ABI_VERSION)
        return false;
    return true;
}

// kate: indent-mode cstyle; space-indent on; indent-width 0;
