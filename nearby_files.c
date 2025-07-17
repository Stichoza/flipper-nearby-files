#include "nearby_files.h"
#include "scenes/nearby_files_scene.h"
#include <furi_hal.h>

#define TAG "NearbyFiles"

// File extensions to scan for
static const char* file_extensions[] = {".sub", ".nfc", ".rfid"};
static const size_t file_extensions_count = sizeof(file_extensions) / sizeof(file_extensions[0]);

// Root directories to scan
static const char* scan_directories[] = {"/ext/subghz", "/ext/nfc", "/ext/lfrfid"};
static const size_t scan_directories_count = sizeof(scan_directories) / sizeof(scan_directories[0]);

// App names for launching
static const char* app_names[] = {"Sub-GHz", "NFC", "125 kHz RFID"};

// Directory filter callback
static bool nearby_files_dir_filter(const char* path, FileInfo* file_info, void* context) {
    UNUSED(context);
    
    if(file_info->flags & FSF_DIRECTORY) {
        // Get directory name
        const char* dir_name = strrchr(path, '/');
        if(dir_name) {
            dir_name++; // Skip the '/'
            
            // Exclude directories named "assets" or starting with "."
            if(strcmp(dir_name, "assets") == 0 || dir_name[0] == '.') {
                return false;
            }
        }
    }
    
    return true;
}

// File filter callback
static bool nearby_files_file_filter(const char* path, FileInfo* file_info, void* context) {
    UNUSED(context);
    
    if(!(file_info->flags & FSF_DIRECTORY)) {
        // Check if file has one of the target extensions
        for(size_t i = 0; i < file_extensions_count; i++) {
            size_t path_len = strlen(path);
            size_t ext_len = strlen(file_extensions[i]);
            if(path_len >= ext_len && 
               strcmp(path + path_len - ext_len, file_extensions[i]) == 0) {
                return true;
            }
        }
    }
    
    return false;
}

// Navigation callback wrapper
static bool nearby_files_navigation_callback(void* context) {
    NearbyFilesApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// Custom event callback wrapper
static bool nearby_files_custom_event_callback(void* context, uint32_t event) {
    NearbyFilesApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

NearbyFilesApp* nearby_files_app_alloc(void) {
    NearbyFilesApp* app = malloc(sizeof(NearbyFilesApp));
    
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->loader = furi_record_open(RECORD_LOADER);
    
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nearby_files_scene_handlers, app);
    
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, nearby_files_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, nearby_files_navigation_callback);
    
    // Initialize variable item list
    app->variable_item_list = variable_item_list_alloc();
    variable_item_list_set_enter_callback(
        app->variable_item_list, nearby_files_file_selected_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, 
        NearbyFilesViewVariableItemList, 
        variable_item_list_get_view(app->variable_item_list));
    
    // Initialize widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NearbyFilesViewWidget, widget_get_view(app->widget));
    
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    
    // Initialize file list
    app->files = NULL;
    app->file_count = 0;
    app->file_capacity = 0;
    
    return app;
}

void nearby_files_app_free(NearbyFilesApp* app) {
    furi_assert(app);
    
    // Free file list
    nearby_files_clear_files(app);
    
    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewWidget);
    variable_item_list_free(app->variable_item_list);
    widget_free(app->widget);
    
    // Free managers
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    
    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_LOADER);
    
    free(app);
}

void nearby_files_add_file(NearbyFilesApp* app, const char* path, const char* name, const char* app_name) {
    // Expand capacity if needed
    if(app->file_count >= app->file_capacity) {
        app->file_capacity = app->file_capacity == 0 ? 16 : app->file_capacity * 2;
        app->files = realloc(app->files, app->file_capacity * sizeof(NearbyFileItem));
    }
    
    // Add new file
    NearbyFileItem* item = &app->files[app->file_count];
    item->path = furi_string_alloc_set(path);
    item->name = furi_string_alloc_set(name);
    item->app_name = app_name;
    
    app->file_count++;
}

void nearby_files_clear_files(NearbyFilesApp* app) {
    if(app->files) {
        for(size_t i = 0; i < app->file_count; i++) {
            furi_string_free(app->files[i].path);
            furi_string_free(app->files[i].name);
        }
        free(app->files);
        app->files = NULL;
    }
    app->file_count = 0;
    app->file_capacity = 0;
}

bool nearby_files_scan_directories(NearbyFilesApp* app) {
    bool success = true;
    
    // Clear existing files
    nearby_files_clear_files(app);
    
    // Scan each root directory
    for(size_t dir_idx = 0; dir_idx < scan_directories_count; dir_idx++) {
        const char* root_dir = scan_directories[dir_idx];
        const char* app_name = app_names[dir_idx];
        
        // Check if directory exists
        if(!storage_dir_exists(app->storage, root_dir)) {
            continue;
        }
        
        // Use dir_walk for recursive scanning
        DirWalk* dir_walk = dir_walk_alloc(app->storage);
        dir_walk_set_recursive(dir_walk, true);
        dir_walk_set_filter_cb(dir_walk, nearby_files_dir_filter, app);
        
        if(dir_walk_open(dir_walk, root_dir)) {
            FuriString* path = furi_string_alloc();
            FileInfo file_info;
            
            while(dir_walk_read(dir_walk, path, &file_info) == DirWalkOK) {
                // Check if it's a file with target extension
                if(!(file_info.flags & FSF_DIRECTORY) && 
                   nearby_files_file_filter(furi_string_get_cstr(path), &file_info, app)) {
                    
                    // Extract filename from path
                    const char* full_path = furi_string_get_cstr(path);
                    const char* filename = strrchr(full_path, '/');
                    if(filename) {
                        filename++; // Skip the '/'
                        nearby_files_add_file(app, full_path, filename, app_name);
                    }
                }
            }
            
            furi_string_free(path);
            dir_walk_close(dir_walk);
        } else {
            success = false;
        }
        
        dir_walk_free(dir_walk);
    }
    
    FURI_LOG_I(TAG, "Found %zu files", app->file_count);
    return success;
}

void nearby_files_populate_list(NearbyFilesApp* app) {
    variable_item_list_reset(app->variable_item_list);
    
    for(size_t i = 0; i < app->file_count; i++) {
        variable_item_list_add(
            app->variable_item_list,
            furi_string_get_cstr(app->files[i].name),
            0,  // No values count for simple list items
            NULL,  // No change callback
            NULL); // No item context needed
    }
}

void nearby_files_file_selected_callback(void* context, uint32_t index) {
    NearbyFilesApp* app = context;
    
    if(index < app->file_count) {
        NearbyFileItem* item = &app->files[index];
        
        FURI_LOG_I(TAG, "Opening %s with %s", furi_string_get_cstr(item->path), item->app_name);
        
        // Queue the app launch to happen after our app exits
        loader_enqueue_launch(
            app->loader, 
            item->app_name, 
            furi_string_get_cstr(item->path),
            LoaderDeferredLaunchFlagGui);
        
        FURI_LOG_I(TAG, "Queued launch of %s with file %s", item->app_name, furi_string_get_cstr(item->path));
        
        // Exit our app to allow the queued app to launch
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
    }
}

int32_t nearby_files_app(void* p) {
    UNUSED(p);
    
    NearbyFilesApp* app = nearby_files_app_alloc();
    
    scene_manager_next_scene(app->scene_manager, NearbyFilesSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    
    nearby_files_app_free(app);
    
    return 0;
}
