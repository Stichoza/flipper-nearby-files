#include "nearby_files.h"
#include "scenes/nearby_files_scene.h"
#include <furi_hal.h>
#include <math.h>

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
                FURI_LOG_I(TAG, "Filtering out directory: %s", path);
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
        // Check if path contains "/assets/" directory (additional protection)
        if(strstr(path, "/assets/") != NULL) {
            FURI_LOG_I(TAG, "Filtering out file from assets: %s", path);
            return false;
        }
        
        // Get filename from path
        const char* filename = strrchr(path, '/');
        if(filename) {
            filename++; // Skip the '/'
        } else {
            filename = path;
        }
        
        // Ignore files starting with dot
        if(filename[0] == '.') {
            FURI_LOG_I(TAG, "Filtering out dotfile: %s", path);
            return false;
        }
        
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
    
    // Initialize submenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NearbyFilesViewSubmenu, submenu_get_view(app->submenu));
    
    // Initialize about widget
    app->about_widget = widget_alloc();
    widget_add_text_scroll_element(
        app->about_widget,
        0,
        0,
        128,
        64,
        "\e#"
        NEARBY_FILES_APP_NAME " v" NEARBY_FILES_VERSION "\n"
        "\n"
        "Lists Sub-GHz, NFC and RFID\n"
        "files sorted by distance from\n"
        "current location.\n"
        "(GPS module required)\n"
        "\n"
        "Files must include Lat: and\n"
        "Lon: entries to be able to\n"
        "calculate the distance.\n"
        "\n"
        "Some custom firmwares like\n"
        "Momentum and RogueMaster\n"
        "add these coordinates at the\n"
        "time of recording if the GPS\n"
        "option is enabled.\n"
        "\n"
        "Click a file to launch it in the\n"
        "appropriate app.\n"
        "\n"
        "Author: @Stichoza\n"
        "\n"
        "For information or issues,\n"
        "go to https://github.com/Stichoza/flipper-nearby-files");
    view_dispatcher_add_view(app->view_dispatcher, NearbyFilesViewAbout, widget_get_view(app->about_widget));
    
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
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewAbout);
    variable_item_list_free(app->variable_item_list);
    widget_free(app->widget);
    submenu_free(app->submenu);
    widget_free(app->about_widget);
    
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
    
    // Create display name without extension
    FuriString* display_name = furi_string_alloc_set(name);
    const char* dot = strrchr(name, '.');
    if(dot != NULL) {
        // Remove extension from display name
        size_t name_len = dot - name;
        furi_string_set_strn(display_name, name, name_len);
    }
    item->name = display_name;
    item->app_name = app_name;
    
    // Initialize GPS fields - will be populated later for performance
    item->latitude = 0.0;
    item->longitude = 0.0;
    item->has_coordinates = false;
    item->distance = 0.0;
    
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
    
    // Process GPS coordinates for all files (filter out files without coordinates)
    nearby_files_process_gps_coordinates(app);
    
    // Sort files by distance from current location
    nearby_files_sort_by_distance(app);
    
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

void nearby_files_refresh_and_populate(NearbyFilesApp* app) {
    // Scan directories to refresh file list
    nearby_files_scan_directories(app);
    
    // Populate the UI list with refreshed files
    nearby_files_populate_list(app);
}

bool nearby_files_parse_coordinates(const char* file_path, double* latitude, double* longitude) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    
    char buffer[256];
    bool lat_found = false, lon_found = false;
    
    // Read file character by character to build lines
    FuriString* line = furi_string_alloc();
    
    while(storage_file_read(file, buffer, 1) == 1) {
        if(buffer[0] == '\n' || buffer[0] == '\r') {
            // Process the line
            const char* line_str = furi_string_get_cstr(line);
            
            // Check for latitude keys
            if(!lat_found) {
                if(strncmp(line_str, "Lat: ", 5) == 0) {
                    *latitude = strtod(line_str + 5, NULL);
                    lat_found = true;
                } else if(strncmp(line_str, "Latitude: ", 10) == 0) {
                    *latitude = strtod(line_str + 10, NULL);
                    lat_found = true;
                } else if(strncmp(line_str, "Latitute: ", 10) == 0) {  // Typo version
                    *latitude = strtod(line_str + 10, NULL);
                    lat_found = true;
                }
            }
            
            // Check for longitude keys
            if(!lon_found) {
                if(strncmp(line_str, "Lon: ", 5) == 0) {
                    *longitude = strtod(line_str + 5, NULL);
                    lon_found = true;
                } else if(strncmp(line_str, "Longitude: ", 11) == 0) {
                    *longitude = strtod(line_str + 11, NULL);
                    lon_found = true;
                }
            }
            
            // If both found, we can stop reading
            if(lat_found && lon_found) break;
            
            // Reset line for next iteration
            furi_string_reset(line);
        } else {
            // Add character to current line
            furi_string_push_back(line, buffer[0]);
        }
    }
    
    furi_string_free(line);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    
    return lat_found && lon_found;
}

double nearby_files_calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    // Haversine formula for calculating distance between two GPS coordinates
    const float R = 6371000.0f; // Earth's radius in meters
    const float PI = 3.14159265f; // Define PI as float
    const float DEG_TO_RAD = PI / 180.0f;
    
    // Convert degrees to radians
    float lat1_rad = (float)lat1 * DEG_TO_RAD;
    float lat2_rad = (float)lat2 * DEG_TO_RAD;
    float delta_lat = ((float)lat2 - (float)lat1) * DEG_TO_RAD;
    float delta_lon = ((float)lon2 - (float)lon1) * DEG_TO_RAD;
    
    float half_delta_lat = delta_lat / 2.0f;
    float half_delta_lon = delta_lon / 2.0f;
    
    float sin_half_delta_lat = sin(half_delta_lat);
    float cos_lat1 = cos(lat1_rad);
    float cos_lat2 = cos(lat2_rad);
    float sin_half_delta_lon = sin(half_delta_lon);
    
    float a = sin_half_delta_lat * sin_half_delta_lat +
              cos_lat1 * cos_lat2 * sin_half_delta_lon * sin_half_delta_lon;
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    
    return (double)(R * c); // Distance in meters, return as double
}

void nearby_files_process_gps_coordinates(NearbyFilesApp* app) {
    if(app->file_count == 0) return;
    
    size_t valid_files = 0;
    
    // Process each file to extract GPS coordinates
    for(size_t i = 0; i < app->file_count; i++) {
        NearbyFileItem* item = &app->files[i];
        double latitude, longitude;
        
        // Parse GPS coordinates from file
        if(nearby_files_parse_coordinates(furi_string_get_cstr(item->path), &latitude, &longitude)) {
            // File has GPS coordinates
            item->latitude = latitude;
            item->longitude = longitude;
            item->has_coordinates = true;
            // Calculate distance from current location
            item->distance = nearby_files_calculate_distance(
                CURRENT_LATITUDE, CURRENT_LONGITUDE,
                latitude, longitude
            );
            
            // Move valid file to the front of the array
            if(valid_files != i) {
                app->files[valid_files] = app->files[i];
            }
            valid_files++;
        } else {
            // File doesn't have GPS coordinates, free its resources
            furi_string_free(item->path);
            furi_string_free(item->name);
        }
    }
    
    // Update file count to only include files with GPS coordinates
    app->file_count = valid_files;
    
    FURI_LOG_I(TAG, "Processed GPS coordinates, %zu files with coordinates", valid_files);
}

void nearby_files_sort_by_distance(NearbyFilesApp* app) {
    if(app->file_count <= 1) return;
    
    // Insertion sort - faster than bubble sort, especially for small datasets
    for(size_t i = 1; i < app->file_count; i++) {
        NearbyFileItem key = app->files[i];
        size_t j = i;
        
        // Move elements that are greater than key one position ahead
        while(j > 0 && app->files[j - 1].distance > key.distance) {
            app->files[j] = app->files[j - 1];
            j--;
        }
        
        app->files[j] = key;
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
        
        // Queue our app to launch again after the file handler exits
        loader_enqueue_launch(
            app->loader,
            EXT_PATH("apps/Tools/nearby_files.fap"),
            NULL,
            LoaderDeferredLaunchFlagNone);
        
        FURI_LOG_I(TAG, "Queued launch of %s with file %s, then return to Nearby Files", item->app_name, furi_string_get_cstr(item->path));
        
        // Exit our app to allow the queued apps to launch
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
