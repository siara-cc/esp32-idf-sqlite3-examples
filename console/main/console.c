/*
  Open and execute SQL statements throught this console.
  Output is in tab-delimited format.
    Connections for SD Card in SPI Mode:
      * SD Card   | ESP32
      *  DAT2        -
      *  DAT3        SS (D5)
      *  CMD         MOSI (D23)
      *  VSS         GND
      *  VDD         3.3V
      *  CLK         SCK (D18)
      *  DAT0        MISO (D19)
      *  DAT1        -
    Connections for SD Card in SD_MMC Mode:
      * SD Card   | ESP32
      *  DAT2 (1)    D12
      *  DAT3 (2)    D13
      *  CMD  (3)    D15
      *  VDD  (4)    3.3V
      *  CLK  (5)    D14
      *  VSS  (6)    GND
      *  DAT0 (7)    D2
      *  DAT1 (8)    D4
*/
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "dirent.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ff.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sqlite3.h"
#include "sdmmc_cmd.h"

#define MAX_FILE_NAME_LEN 100
#define MAX_STR_LEN 500

static const char *TAG = "sqlite3_console";

char db_file_name[MAX_FILE_NAME_LEN] = "\0";
sqlite3 *db = NULL;
int rc;
char str[MAX_STR_LEN];

const char* data = "Output:";
bool first_time = false;
static int callback(void *data, int argc, char **argv, char **azColName) {
  int i;
  if (first_time) {
     printf((const char *) data);
     printf("\n");
     for (i = 0; i<argc; i++) {
         if (i)
           printf("%c", (char) '\t');
         printf("%s\n", azColName[i]);
     }
     printf("\n");
     first_time = false;
  }
  for (i = 0; i<argc; i++) {
    if (i)
      printf("%c", (char) '\t');
    printf("%s\n", argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

int db_open() {
  if (db != NULL)
    sqlite3_close(db);
  int rc = sqlite3_open(db_file_name, &db);
  if (rc) {
    printf("Can't open database: ");
    printf(sqlite3_errmsg(db));
    printf("\n");
    return rc;
  } else
    printf("Opened database successfully\n");
  return rc;
}

char *zErrMsg = 0;
int db_exec(const char *sql) {
  if (db == NULL) {
    printf("No database open\n");
    return 0;
  }
  first_time = true;
  long start = esp_timer_get_time();
  int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    printf("SQL error: ");
    printf(zErrMsg);
    sqlite3_free(zErrMsg);
  } else
    printf("Operation done successfully\n");
  printf("Time taken:");
  printf("%lld", esp_timer_get_time() - start);
  printf(" us\n");
  return rc;
}

void input_string(char *str, int max_len) {
  max_len--;
  int ctr = 0;
  str[ctr] = getchar();
  if (str[ctr] == '\r' || str[ctr] == '\n')
    str[ctr] = getchar();
  while (str[ctr] != '\n') {
    if (str[ctr] >= ' ' && str[ctr] <= '~') {
      putchar(str[ctr]);
      ctr++;
    }
    if (ctr >= max_len)
      break;
    str[ctr] = getchar();
  }
  str[ctr] = 0;
  printf("\n");
}

int input_num() {
  char in[20];
  int ctr = 0;
  in[ctr] = getchar();
  if (in[ctr] == '\r' || in[ctr] == '\n')
    in[ctr] = getchar();
  while (in[ctr] != '\n') {
    if (in[ctr] >= '0' && in[ctr] <= '9') {
      putchar(in[ctr]);
      ctr++;
    }
    if (ctr >= sizeof(in))
      break;
    in[ctr] = getchar();
  }
  printf("\n");
  in[ctr] = 0;
  int ret = atoi(in);
  return ret;
}

void listDir(const char * dirname) {
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir (dirname)) != NULL) {
    while ((ent = readdir (dir)) != NULL) {
      printf ("%s\n", ent->d_name);
    }
    closedir (dir);
  } else {
    perror ("");
  }
}

void renameFile(const char *path1, const char *path2) {
  printf("Renaming file %s to %s\n", path1, path2);
  if (!rename(path1, path2)) {
    printf("File renamed\n");
  } else {
    printf("Rename failed\n");
  }
}

void deleteFile(const char *path) {
  printf("Deleting file: %s\n", path);
  if (!unlink(path)) {
    printf("File deleted\n");
  } else {
    printf("Delete failed\n");
  }
}

enum {CHOICE_OPEN_DB = 1, CHOICE_EXEC_SQL, CHOICE_EXEC_MULTI_SQL, CHOICE_CLOSE_DB,
    CHOICE_LIST_FOLDER, CHOICE_RENAME_FILE, CHOICE_DELETE_FILE};
int askChoice() {
  printf("\n");
  printf("Welcome to SQLite console!!\n");
  printf("---------------------------\n");
  printf("\n");
  printf("Database file: ");
  printf(db_file_name);
  printf("\n");
  printf("1. Open database\n");
  printf("2. Execute SQL\n");
  printf("3. Execute Multiple SQL\n");
  printf("4. Close database\n");
  printf("5. List folder contents\n");
  printf("6. Rename file\n");
  printf("7. Delete file\n");
  printf("8. Exit\n");
  printf("\n");
  printf("Enter choice: ");
  return input_num();
}

void displayPrompt(const char *title) {
  printf("(prefix /spiffs/ or /sd/ or /sdcard/ for\n");
  printf(" SPIFFS or SD_SPI or SD_MMC respectively)\n");
  printf("Enter ");
  printf(title);
  printf("\n");
}

void app_main() {

  ESP_LOGI(TAG, "Initializing SPIFFS");
  esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = "storage",
    .max_files = 5,
    .format_if_mount_failed = true
  };
  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t reg = esp_vfs_spiffs_register(&conf);
  if (reg != ESP_OK) {
      if (reg == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount or format filesystem");
      } else if (reg == ESP_ERR_NOT_FOUND) {
          ESP_LOGE(TAG, "Failed to find SPIFFS partition");
      } else {
          ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(reg));
      }
      return;
  }
  size_t total = 0, used = 0;
  reg = esp_spiffs_info(NULL, &total, &used);
  if (reg != ESP_OK) {
      ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(reg));
  } else {
      ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

  ESP_LOGI(TAG, "Initializing SD card");
  ESP_LOGI(TAG, "Using SDMMC peripheral");
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  // To use 1-line SD mode, uncomment the following line:
  // slot_config.width = 1;
  // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
  // Internal pull-ups are not sufficient. However, enabling internal pull-ups
  // does make a difference some boards, so we do that here.
  gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
  gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
  gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
  gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
  gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes
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
  reg = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
  if (reg != ESP_OK) {
      if (reg == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount filesystem. "
              "If you want the card to be formatted, set format_if_mount_failed = true.");
      } else {
          ESP_LOGE(TAG, "Failed to initialize the card (%s). "
              "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(reg));
      }
      return;
  }
  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);

  sqlite3_initialize();

  int choice = 0;
  while (choice != 8) {
    choice = askChoice();
    switch (choice) {
      case CHOICE_OPEN_DB:
        displayPrompt("file name: ");
        input_string(str, MAX_FILE_NAME_LEN);
        if (str[0] != 0) {
          strncpy(db_file_name, str, MAX_FILE_NAME_LEN);
          db_open();
        }
        break;
      case CHOICE_EXEC_SQL:
        printf("Enter SQL (max %d characters):\n", MAX_STR_LEN);
        input_string(str, MAX_STR_LEN);
        if (str[0] != 0)
          db_exec(str);
        break;
      case CHOICE_EXEC_MULTI_SQL:
        printf("(Copy paste may not always work due to limited serial buffer)\n");
        printf("Keep entering SQL, empty to stop:\n");
        do {
          input_string(str, MAX_STR_LEN);
          if (str[0] != 0)
            db_exec(str);
        } while (str[0] != 0);
        break;
      case CHOICE_CLOSE_DB:
        if (db_file_name[0] != 0) {
            db_file_name[0] = 0;
            sqlite3_close(db);
        }
        break;
      case CHOICE_LIST_FOLDER:
      case CHOICE_RENAME_FILE:
      case CHOICE_DELETE_FILE:
        displayPrompt("path: ");
        input_string(str, MAX_STR_LEN);
        if (str[0] != 0) {
          int fs_prefix_len = 0;
          char str1[MAX_FILE_NAME_LEN];
            switch (choice) {
              case CHOICE_LIST_FOLDER:
                listDir(str + fs_prefix_len);
                break;
              case CHOICE_RENAME_FILE:
                displayPrompt("path to rename as: ");
                input_string(str1, MAX_STR_LEN);
                if (str1[0] != 0)
                  renameFile(str + fs_prefix_len, str1 + fs_prefix_len);
                break;
              case CHOICE_DELETE_FILE:
                deleteFile(str + fs_prefix_len);
                break;
            }
        }
        break;
      case 8:
        break;
      default:
        printf("Invalid choice. Try again.\n");
    }
  }

  // All done, unmount partition and disable SDMMC or SPI peripheral
  esp_vfs_fat_sdmmc_unmount();
  ESP_LOGI(TAG, "Card unmounted");
  esp_vfs_spiffs_unregister(NULL);
  ESP_LOGI(TAG, "SPIFFS unmounted");

  while(1);

}
