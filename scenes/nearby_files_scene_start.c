#include "../nearby_files.h"
#include "nearby_files_scene.h"

void nearby_files_scene_start_on_enter(void* context) {
    NearbyFilesApp* app = context;
    
    // Show waiting for GPS widget
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting for GPS...");
    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewWidget);
    
    // Start GPS waiting process
    nearby_files_start_gps_wait(app);
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
