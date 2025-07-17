#include "../nearby_files.h"
#include "nearby_files_scene.h"

void nearby_files_scene_file_list_on_enter(void* context) {
    NearbyFilesApp* app = context;
    
    // Populate menu with found files
    nearby_files_populate_menu(app);
    
    // Switch to menu view
    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewMenu);
}

bool nearby_files_scene_file_list_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    bool consumed = false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NearbyFilesCustomEventFileSelected) {
            consumed = true;
        }
    }
    
    return consumed;
}

void nearby_files_scene_file_list_on_exit(void* context) {
    NearbyFilesApp* app = context;
    menu_reset(app->menu);
}
