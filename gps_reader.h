#pragma once

#include <furi_hal.h>

#define GPS_RX_BUF_SIZE 512
#define GPS_UART_CH (FuriHalSerialIdUsart)

typedef struct {
    bool valid;
    float latitude;
    float longitude;
    bool module_detected;  // True if any NMEA sentences received
    int satellite_count;   // Number of satellites in view
} GpsCoordinates;

typedef struct {
    FuriMutex* mutex;
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    uint8_t rx_buf[GPS_RX_BUF_SIZE];
    FuriHalSerialHandle* serial_handle;
    uint32_t baudrate;
    GpsCoordinates coordinates;
} GpsReader;

// Initialize GPS reader
GpsReader* gps_reader_alloc(void);

// Free GPS reader
void gps_reader_free(GpsReader* gps_reader);

// Get current coordinates (thread-safe)
GpsCoordinates gps_reader_get_coordinates(GpsReader* gps_reader);
