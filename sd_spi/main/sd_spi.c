/*
    This example opens Sqlite3 databases from SD Card and
    retrieves data from them.
    Before running please copy following files to SD Card:
    data/mdr512.db
    data/census2000names.db
*/
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sqlite3.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sqlite3_sdmmc";

// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13

const char* data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName){
   int i;
   printf("%s: ", (const char*)data);
   for (i = 0; i<argc; i++){
       printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

int openDb(const char *filename, sqlite3 **db) {
   int rc = sqlite3_open(filename, db);
   if (rc) {
       printf("Can't open database: %s\n", sqlite3_errmsg(*db));
       return rc;
   } else {
       printf("Opened database successfully\n");
   }
   return rc;
}

char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql) {
   printf("%s\n", sql);
   int64_t start = esp_timer_get_time();
   int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
   if (rc != SQLITE_OK) {
       printf("SQL error: %s\n", zErrMsg);
       sqlite3_free(zErrMsg);
   } else {
       printf("Operation done successfully\n");
   }
   printf("Time taken: %lld\n", esp_timer_get_time()-start);
   return rc;
}

void app_main()
{
    sqlite3 *db1;
    sqlite3 *db2;
    int rc;

    ESP_LOGI(TAG, "Initializing SD card");

    ESP_LOGI(TAG, "Using SPI peripheral");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck  = PIN_NUM_CLK;
    slot_config.gpio_cs   = PIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    struct stat st;
    if (stat("/sdcard/mdr512.db", &st) == 0) {
		printf("/sdcard/mdr512.db exist\n");
    } else {
		printf("/sdcard/mdr512.db NOT exist\n");
    }

    if (stat("/sdcard/census.db", &st) == 0) {
		printf("/sdcard/census.db exist\n");
    } else {
		printf("/sdcard/census.db NOT exist\n");
    }

    sqlite3_initialize();

    // Open database 1
    if (openDb("/sdcard/mdr512.db", &db2))
        return;
    if (openDb("/sdcard/census.db", &db1))
        return;

    rc = db_exec(db1, "Select * from surnames where name = 'MICHELLE'");
    if (rc != SQLITE_OK) {
        sqlite3_close(db1);
        sqlite3_close(db2);
        return;
    }
    rc = db_exec(db2, "Select * from domain_rank where domain between 'google.com' and 'google.com.z'");
    if (rc != SQLITE_OK) {
        sqlite3_close(db1);
        sqlite3_close(db2);
        return;
    }
    rc = db_exec(db1, "Select * from surnames where name = 'SPRINGER'");
    if (rc != SQLITE_OK) {
       sqlite3_close(db1);
        sqlite3_close(db2);
        return;
    }
    rc = db_exec(db2, "Select * from domain_rank where domain = 'zoho.com'");
    if (rc != SQLITE_OK) {
        sqlite3_close(db1);
        sqlite3_close(db2);
        return;
    }

    sqlite3_close(db1);
    sqlite3_close(db2);

    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(TAG, "Card unmounted");

   //while(1);
}
