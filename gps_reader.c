#include <string.h>
#include <minmea.h>
#include "gps_reader.h"

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

static const int gps_baudrates[] = {4800, 9600, 19200, 38400, 57600, 115200};
static const int default_baudrate_index = 1; // 9600

// Forward declarations
static void gps_reader_parse_nmea(GpsReader* gps_reader, char* line);
static size_t gps_reader_process_buffer_lines(GpsReader* gps_reader, uint8_t* buffer, size_t buffer_len);

static void gps_reader_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* context) {
    GpsReader* gps_reader = (GpsReader*)context;

    if(ev == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(gps_reader->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(gps_reader->thread), WorkerEvtRxDone);
    }
}

static bool gps_reader_serial_init(GpsReader* gps_reader) {
    furi_assert(!gps_reader->serial_handle);

    FURI_LOG_I("GPS", "Attempting to acquire GPS UART channel");
    
    // Try to acquire GPS UART channel with error handling
    gps_reader->serial_handle = furi_hal_serial_control_acquire(GPS_UART_CH);
    if(!gps_reader->serial_handle) {
        FURI_LOG_W("GPS", "Failed to acquire GPS UART channel");
        return false;
    }
    
    FURI_LOG_I("GPS", "GPS UART channel acquired, initializing");
    furi_hal_serial_init(gps_reader->serial_handle, gps_reader->baudrate);
    furi_hal_serial_async_rx_start(gps_reader->serial_handle, gps_reader_on_irq_cb, gps_reader, false);
    
    FURI_LOG_I("GPS", "GPS serial initialization complete");
    return true;
}

static void gps_reader_serial_deinit(GpsReader* gps_reader) {
    if(gps_reader->serial_handle) {
        FURI_LOG_I("GPS", "Stopping serial async RX");
        furi_hal_serial_async_rx_stop(gps_reader->serial_handle);
        
        FURI_LOG_I("GPS", "Deinitializing serial");
        furi_hal_serial_deinit(gps_reader->serial_handle);
        
        FURI_LOG_I("GPS", "Releasing serial control");
        furi_hal_serial_control_release(gps_reader->serial_handle);
        gps_reader->serial_handle = NULL;
        
        FURI_LOG_I("GPS", "Serial cleanup complete");
    }
}

// Process complete lines in buffer and return new buffer offset
static size_t gps_reader_process_buffer_lines(GpsReader* gps_reader, uint8_t* buffer, size_t buffer_len) {
    char* line_current = (char*)buffer;
    char* buffer_end = (char*)buffer + buffer_len;
    
    while(line_current < buffer_end) {
        // Skip null characters
        while(*line_current == '\0' && line_current < buffer_end - 1) {
            line_current++;
        }
        
        // Check if we've reached the end
        if(line_current >= buffer_end) {
            break;
        }
        
        // Find next newline
        char* newline = strchr(line_current, '\n');
        if(newline && newline < buffer_end) {
            *newline = '\0';
            gps_reader_parse_nmea(gps_reader, line_current);
            line_current = newline + 1;
        } else {
            // No complete line found, move leftover data to start of buffer
            size_t leftover_len = buffer_len - (line_current - (char*)buffer);
            if(leftover_len > 0 && line_current > (char*)buffer) {
                memmove(buffer, line_current, leftover_len);
                return leftover_len;
            }
            break;
        }
    }
    
    return 0; // All data processed
}

static void gps_reader_parse_nmea(GpsReader* gps_reader, char* line) {
    switch(minmea_sentence_id(line, false)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc frame;
        if(minmea_parse_rmc(&frame, line) && frame.valid) {
            furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
            gps_reader->coordinates.valid = true;
            gps_reader->coordinates.latitude = minmea_tocoord(&frame.latitude);
            gps_reader->coordinates.longitude = minmea_tocoord(&frame.longitude);
            furi_mutex_release(gps_reader->mutex);
        }
    } break;

    case MINMEA_SENTENCE_GGA: {
        struct minmea_sentence_gga frame;
        if(minmea_parse_gga(&frame, line) && frame.fix_quality > 0) {
            furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
            gps_reader->coordinates.valid = true;
            gps_reader->coordinates.latitude = minmea_tocoord(&frame.latitude);
            gps_reader->coordinates.longitude = minmea_tocoord(&frame.longitude);
            furi_mutex_release(gps_reader->mutex);
        }
    } break;

    case MINMEA_SENTENCE_GLL: {
        struct minmea_sentence_gll frame;
        if(minmea_parse_gll(&frame, line) && frame.status == 'A') {
            furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
            gps_reader->coordinates.valid = true;
            gps_reader->coordinates.latitude = minmea_tocoord(&frame.latitude);
            gps_reader->coordinates.longitude = minmea_tocoord(&frame.longitude);
            furi_mutex_release(gps_reader->mutex);
        }
    } break;

    default:
        break;
    }
}

static int32_t gps_reader_worker(void* context) {
    GpsReader* gps_reader = (GpsReader*)context;
    
    FURI_LOG_I("GPS", "GPS worker thread started");
    
    // Try to initialize GPS serial, exit gracefully if it fails
    if(!gps_reader_serial_init(gps_reader)) {
        FURI_LOG_W("GPS", "GPS serial initialization failed, worker thread exiting");
        gps_reader->thread_running = false;
        return -1;
    }
    
    size_t rx_offset = 0;

    FURI_LOG_I("GPS", "GPS worker thread entering main loop");
    
    while(1) {
        uint32_t events = furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEvtStop) {
            FURI_LOG_I("GPS", "GPS worker thread received stop signal");
            gps_reader->thread_running = false;
            break;
        }

        if(events & WorkerEvtRxDone) {
            size_t len = 0;
            // Process all available data in batches to prevent buffer overflow
            size_t total_processed = 0;
            do {
                // Ensure we don't overflow the buffer
                if(rx_offset >= GPS_RX_BUF_SIZE - 1) {
                    FURI_LOG_W("GPS", "RX buffer full, processing and resetting");
                    // Try to process any complete lines before resetting
                    gps_reader->rx_buf[rx_offset] = '\0';
                    gps_reader_process_buffer_lines(gps_reader, gps_reader->rx_buf, rx_offset);
                    rx_offset = 0;
                }
                
                size_t available_space = GPS_RX_BUF_SIZE - 1 - rx_offset;
                
                len = furi_stream_buffer_receive(
                    gps_reader->rx_stream,
                    gps_reader->rx_buf + rx_offset,
                    available_space,
                    0);
                    
                if(len > 0) {
                    rx_offset += len;
                    total_processed += len;
                    gps_reader->rx_buf[rx_offset] = '\0';
                    
                    // Process complete lines and update rx_offset
                    rx_offset = gps_reader_process_buffer_lines(gps_reader, gps_reader->rx_buf, rx_offset);
                }
                
                // Limit processing to prevent blocking for too long
                if(total_processed > GPS_RX_BUF_SIZE * 4) {
                    FURI_LOG_D("GPS", "Processed %zu bytes, yielding to prevent blocking", total_processed);
                    break;
                }
            } while(len > 0);
        }
    }

    FURI_LOG_I("GPS", "GPS worker thread cleaning up serial");
    gps_reader_serial_deinit(gps_reader);
    
    // Free rx_stream and set to NULL to prevent double-free
    if(gps_reader->rx_stream) {
        FURI_LOG_I("GPS", "GPS worker thread freeing rx_stream");
        furi_stream_buffer_free(gps_reader->rx_stream);
        gps_reader->rx_stream = NULL;
    }

    FURI_LOG_I("GPS", "GPS worker thread exiting");
    return 0;
}

GpsReader* gps_reader_alloc(void) {
    GpsReader* gps_reader = malloc(sizeof(GpsReader));
    
    gps_reader->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    gps_reader->rx_stream = furi_stream_buffer_alloc(GPS_RX_BUF_SIZE, 1);
    gps_reader->serial_handle = NULL;
    gps_reader->baudrate = gps_baudrates[default_baudrate_index];
    gps_reader->thread_running = false;
    
    // Initialize coordinates as invalid
    gps_reader->coordinates.valid = false;
    gps_reader->coordinates.latitude = 0.0f;
    gps_reader->coordinates.longitude = 0.0f;
    
    // Start worker thread
    gps_reader->thread = furi_thread_alloc_ex("GpsReaderWorker", 1024, gps_reader_worker, gps_reader);
    furi_thread_start(gps_reader->thread);
    gps_reader->thread_running = true;
    
    return gps_reader;
}

void gps_reader_free(GpsReader* gps_reader) {
    furi_assert(gps_reader);
    
    FURI_LOG_I("GPS", "Starting GPS reader cleanup");
    
    // Stop worker thread
    FURI_LOG_I("GPS", "Stopping worker thread (running: %s)", gps_reader->thread_running ? "yes" : "no");
    
    if(gps_reader->thread_running) {
        furi_thread_flags_set(furi_thread_get_id(gps_reader->thread), WorkerEvtStop);
    }
    
    furi_thread_join(gps_reader->thread);
    FURI_LOG_I("GPS", "Worker thread stopped");
    furi_thread_free(gps_reader->thread);
    gps_reader->thread = NULL;
    gps_reader->thread_running = false;
    
    // Clean up resources that might not have been freed by worker thread
    if(gps_reader->rx_stream) {
        FURI_LOG_I("GPS", "Freeing rx_stream");
        furi_stream_buffer_free(gps_reader->rx_stream);
        gps_reader->rx_stream = NULL;
    }
    
    FURI_LOG_I("GPS", "Freeing mutex");
    furi_mutex_free(gps_reader->mutex);
    gps_reader->mutex = NULL;
    
    FURI_LOG_I("GPS", "GPS reader cleanup complete");
    free(gps_reader);
}

GpsCoordinates gps_reader_get_coordinates(GpsReader* gps_reader) {
    furi_assert(gps_reader);
    
    // Use timeout instead of blocking forever to prevent deadlocks
    if(furi_mutex_acquire(gps_reader->mutex, 100) == FuriStatusOk) {
        GpsCoordinates coords = gps_reader->coordinates;
        furi_mutex_release(gps_reader->mutex);
        return coords;
    } else {
        // Return invalid coordinates if mutex timeout
        GpsCoordinates invalid_coords = {.valid = false, .latitude = 0.0f, .longitude = 0.0f};
        return invalid_coords;
    }
}
