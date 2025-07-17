#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <toolbox/dir_walk.h>
#include <loader/loader.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NearbyFilesApp NearbyFilesApp;

typedef enum {
    NearbyFilesViewVariableItemList,
    NearbyFilesViewWidget,
    NearbyFilesViewSubmenu,
    NearbyFilesViewAbout,
} NearbyFilesView;



typedef enum {
    NearbyFilesCustomEventNone,
    NearbyFilesCustomEventFileSelected,
    NearbyFilesCustomEventRefreshList,
    NearbyFilesCustomEventAbout,
} NearbyFilesCustomEvent;

typedef struct {
    FuriString* path;
    FuriString* name;
    const char* app_name;
} NearbyFileItem;

struct NearbyFilesApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    Widget* widget;
    Submenu* submenu;
    Widget* about_widget;
    Storage* storage;
    Loader* loader;
    
    NearbyFileItem* files;
    size_t file_count;
    size_t file_capacity;
};

// App lifecycle
NearbyFilesApp* nearby_files_app_alloc(void);
void nearby_files_app_free(NearbyFilesApp* app);
int32_t nearby_files_app(void* p);

// File scanning
bool nearby_files_scan_directories(NearbyFilesApp* app);
void nearby_files_add_file(NearbyFilesApp* app, const char* path, const char* name, const char* app_name);
void nearby_files_clear_files(NearbyFilesApp* app);

// UI helpers
void nearby_files_populate_list(NearbyFilesApp* app);
void nearby_files_refresh_and_populate(NearbyFilesApp* app);
void nearby_files_file_selected_callback(void* context, uint32_t index);

#ifdef __cplusplus
}
#endif
