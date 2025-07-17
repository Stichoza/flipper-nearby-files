#include "nearby_files_scene.h"
#include "../nearby_files.h"

typedef enum {
    NearbyFilesMenuItemRefreshList,
    NearbyFilesMenuItemAbout,
} NearbyFilesMenuItem;

void nearby_files_scene_menu_submenu_callback(void* context, uint32_t index) {
    NearbyFilesApp* app = context;
    
    switch(index) {
        case NearbyFilesMenuItemRefreshList:
            view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventRefreshList);
            break;
        case NearbyFilesMenuItemAbout:
            view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventAbout);
            break;
    }
}

void nearby_files_scene_menu_on_enter(void* context) {
    NearbyFilesApp* app = context;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Nearby Files v0.1");
    
    submenu_add_item(
        app->submenu,
        "Refresh List",
        NearbyFilesMenuItemRefreshList,
        nearby_files_scene_menu_submenu_callback,
        app);
    
    submenu_add_item(
        app->submenu,
        "About",
        NearbyFilesMenuItemAbout,
        nearby_files_scene_menu_submenu_callback,
        app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewSubmenu);
}

bool nearby_files_scene_menu_on_event(void* context, SceneManagerEvent event) {
    NearbyFilesApp* app = context;
    bool consumed = false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case NearbyFilesCustomEventRefreshList:
                // Refresh the file list and go back to file list scene
                nearby_files_refresh_and_populate(app);
                scene_manager_next_scene(app->scene_manager, NearbyFilesSceneFileList);
                consumed = true;
                break;
            case NearbyFilesCustomEventAbout:
                // Go to about scene
                scene_manager_next_scene(app->scene_manager, NearbyFilesSceneAbout);
                consumed = true;
                break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Exit the app when back button is pressed in menu
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }
    
    return consumed;
}

void nearby_files_scene_menu_on_exit(void* context) {
    NearbyFilesApp* app = context;
    submenu_reset(app->submenu);
}
