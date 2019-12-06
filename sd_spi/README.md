# SPI_MMC Example

Uses the SPI MMC (SPI peripheral) port on ESP32 to retrieve from SQLite databases.

|ESP32 pin|SPI pin|Notes|
|:-:|:-:|:-:|
|GPIO14(MTMS)|SCK||
|GPIO15(MTDO)|MOSI|10k pull up if can't mount|
|GPIO2|MISO||
|GPIO13(MTCK)|CS|| 
|3.3V|VCC|Can't use 5V supply|
|GND|GND||

After formatting the SD card with FAT32, copy the following files:   
data/census.db   
data/mdr512.db   


For more information, please see README.md at https://github.com/siara-cc/esp32_arduino_sqlite3_lib/

