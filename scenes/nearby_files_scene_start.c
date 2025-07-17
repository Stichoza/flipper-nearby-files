#include "../nearby_files.h"
#include "nearby_files_scene.h"

void nearby_files_scene_start_on_enter(void* context) {
    NearbyFilesApp* app = context;
    
    // Show loading widget while scanning
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Scanning nearby files...");
    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewWidget);
    
    // Scan directories for files
    if(nearby_files_scan_directories(app)) {
        if(app->file_count > 0) {
            scene_manager_next_scene(app->scene_manager, NearbyFilesSceneFileList);
        } else {
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No files found");
            widget_add_string_element(
                app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Press Back to exit");
        }
    } else {
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Scan failed");
        widget_add_string_element(
            app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Press Back to exit");
    }
}

bool nearby_files_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nearby_files_scene_start_on_exit(void* context) {
    NearbyFilesApp* app = context;
    widget_reset(app->widget);
}
