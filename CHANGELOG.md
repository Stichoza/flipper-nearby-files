## [v1.2]

### Changed
- Minor changes to comply with Flipper App Catalog.

## [v1.1]

### Added
- GPS module information display in waiting screen
- Enhanced GPS status feedback

### Changed
- App category moved to GPIO for better organization
- Improved app relaunching mechanism (fixed hardcoded paths)

### Fixed
- Files with zero GPS coordinates are now properly filtered out
- App relaunching after launching external files now works correctly

## [v1.0]

### Added
- Comprehensive documentation and README
- Screenshots and video preview
- Build workflows for multiple firmware variants
- Funding information
- MIT License
- Flipper Map link in documentation

### Changed
- Removed firmware-specific code for broader compatibility
- Updated app icon and visual elements
- Enhanced documentation with detailed setup instructions

### Fixed
- Back button now opens menu instead of immediately exiting
- Removed leftover hardcoded GPS coordinates

## [v0.7]

### Added
- Satellite count display in GPS waiting screen
- Enhanced GPS status text messages
- Better user feedback during GPS acquisition

## [v0.6]

### Added
- Distance display in file list (e.g., [45m], [1.3km], [23km])
- Formatted distance labels with proper units and precision

### Fixed
- Reverted problematic GPS parsing changes that caused instability

## [v0.5]

### Fixed
- Major stability improvements addressing crashes and reboots
- GPS module integration stability issues resolved

### Changed
- Renamed file selection screen for better clarity

## [v0.4]

### Added
- Real GPS module integration using minmea library
- GPS coordinate acquisition and validation
- Distance-based file sorting using actual GPS location

### Fixed
- GPS acquisition now works reliably
- File sorting by proximity implemented and functional

## [v0.3]

### Added
- GPS coordinate parsing from files (Lat, Lon, Latitude, Longitude, Latitute)
- Distance calculation using Haversine formula
- File sorting by distance from hardcoded coordinates (temporary)
- Exit button in main menu
- minmea library integration as submodule

### Changed
- Files without GPS coordinates are now excluded from display
- Optimized sorting algorithm for better performance

## [v0.2]

### Added
- Main menu with "Refresh List" and "About" options
- About screen with app information
- Scene management system

### Changed
- Enhanced menu title display (larger and bold)
- Improved navigation flow with back button support

## [v0.1]

### Added
- Initial app implementation with file browsing
- Support for .sub, .nfc, and .rfid file types
- Recursive directory scanning with filtering
- VariableItemList UI for compact file display
- File launching functionality for external apps
- Directory filtering (excludes "assets" folders and dotfiles)

### Features
- File extension hiding in display
- App restoration after launching external files
- Back button exits app
- Flat file list from multiple directories (/ext/subghz, /ext/nfc, /ext/lfrfid)

---

## Version History Summary

- **v1.2**: Flipper App Catalog compliance.
- **v1.1**: GPS filtering improvements and app category changes
- **v1.0**: Production release with documentation and multi-firmware support
- **v0.7**: Enhanced GPS status display
- **v0.6**: Distance display in file list
- **v0.5**: Stability fixes for GPS integration
- **v0.4**: Real GPS integration and distance sorting
- **v0.3**: GPS coordinate parsing and distance calculation
- **v0.2**: Menu system and navigation improvements
- **v0.1**: Initial implementation with basic file browsing
