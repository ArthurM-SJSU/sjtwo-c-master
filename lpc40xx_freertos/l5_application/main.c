#include "FreeRTOS.h"
#include "boot_leg_spi.h"
#include "delay.h"
#include "ff.h"
#include "gpio.h"
#include "gpio_isr.h"
#include "queue.h"
#include "semphr.h"
#include "sj2_cli.h"
#include "song_name.h"
#include "string.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// SCI register address - page 37
#define SCI_MODE 0x0
#define SCI_STATUS 0x1
#define SCI_BASS 0x2
#define SCI_CLOCKF 0x3
#define SCI_DECODE_TOME 0x4
#define SCI_AUDATA 0x5
#define SCI_WRAM 0x6
#define SCI_WRAMADDR 0x7
#define SCI_HDAT0 0x8
#define SCI_HDAT1 0x9
#define SCI_AIADDR 0xA
#define SCI_VOL 0xB
#define SCI_AICTRL0 0xC
#define SCI_AICTRL1 0xD
#define SCI_AICTRL2 0xE
#define SCI_AICTRL3 0xF

// SCI_MODE register bits - explanation on page 38
// SM_DIFF 0x0001
// SM_LAYER12 0x0002
// SM_RESET 0x0004
// SM_CANCEL 0x0008
// SM_EARSPEAKER_LO 0x0010
// SM_TESTS 0x0020
// SM_STREAM 0x0040
// SM_EARSPEAKER_HI 0x0080
// SM_DACT 0x0100
// SM_SDIORD 0x0200
// SM_SDISHARE 0x0400
// SM_SDINEW 0x0800
// SM_ADPCM 0x1000
// SM_B13 0x2000
// SM_LINE1 0x4000
// SM_CLK_RANGE 0x8000

enum SW0_state { playing, menu, volume, powerup, load_next };

enum SW1_state { next, increase };

enum SW2_state { play, pause, select };

enum SW3_state { previous, decrease };

enum SW0_state SW0_state = powerup;
enum SW1_state SW1_state = next;
enum SW2_state SW2_state = play;
enum SW3_state SW3_state = previous;

SemaphoreHandle_t SW3_pressed;
SemaphoreHandle_t SW2_pressed;
SemaphoreHandle_t SW1_pressed;
SemaphoreHandle_t SW0_pressed;

#define max_number_of_songs 6
#define max_song_name 32

QueueHandle_t Q_song_name;

QueueHandle_t Q_song_data;

char **song_names;

uint8_t total_number_of_songs;

gpio_s DREQ, XDCS, XCS, XRESET, SW3, SW2, SW1, SW0;

void SCI_WRITE(uint8_t addr, uint16_t data) {
  gpio__reset(XCS);

  ssp0_byte_exchange(0x02);        // SPI Write opcode
  ssp0_byte_exchange(addr);        // Address to write to
  ssp0_byte_exchange(data >> 8);   // MSB
  ssp0_byte_exchange(data & 0xFF); // LSB

  gpio__set(XCS);

  while (!gpio__get(DREQ)) {
  }
}

uint16_t SCI_READ(uint8_t addr) {
  uint16_t temp = 0;

  gpio__reset(XCS);

  ssp0_byte_exchange(0x03);   // SPI Read opcode
  temp = ssp0_SCI_READ(addr); // Address to read from

  gpio__set(XCS);

  while (!gpio__get(DREQ)) {
  }

  return temp;
}

void SDI_WRITE(uint8_t data) {
  gpio__reset(XDCS);
  ssp0_byte_exchange(data);
  gpio__set(XDCS);
}

void MP3_init(void) {
  SCI_WRITE(SCI_MODE, 0x0804);
  SCI_WRITE(SCI_BASS, 0x0000);
  SCI_WRITE(SCI_CLOCKF, 0x6000);
  SCI_WRITE(SCI_AUDATA, 0xAC45);
  // SCI_WRITE(SCI_VOL, 0x0F0F);
  SCI_WRITE(SCI_VOL, 0x0000);
}

void MP3_RESET(void) {
  gpio__reset(XRESET);
  gpio__set(XRESET);
}

FRESULT scan_files(char *path) {
  FRESULT res;
  DIR dir;
  static FILINFO fno;

  res = f_opendir(&dir, path); /* Open the directory */
  if (res == FR_OK) {
    for (int i = 0; i < max_number_of_songs; i++) {
      res = f_readdir(&dir, &fno); /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0)
        break;
      else {
        strcpy(song_names[i], fno.fname);
      }
      total_number_of_songs = i;
    }
    f_closedir(&dir);
  }
  return res;
}

void read_task(void *param) {
  song_name name;
  uint8_t bytes_to_read[512];

  while (1) {
    // if (xQueueReceive(Q_song_name, &name, portMAX_DELAY)) {
    //   printf("name: %s\n", name);
    // }

    if (SW0_state == powerup) {
      strcpy(name, song_names[0]);
      SW0_state = playing;
    }
    // else
    // {
    //   SW0_state == load_next
    // }

    FIL file;
    UINT total_file_size = 0;
    UINT file_size_left = 0;

    FRESULT result = f_open(&file, name, FA_READ);

    total_file_size = f_size(&file);
    if (FR_OK == result) {
      UINT bytes_read;
      while (total_file_size > file_size_left) {
        if (FR_OK == f_read(&file, &bytes_to_read, 512, &bytes_read)) {
          file_size_left = file_size_left + bytes_read;
          xQueueSend(Q_song_data, bytes_to_read, portMAX_DELAY);
          vTaskDelay(1);
        } else {
          fprintf(stderr, "ERR");
        }
      }
      f_close(&file);
    } else {
      printf("ERROR: Failed to open: %s\n", name);
    }
  }
}

void player_task(void *param) {
  uint8_t bytes_to_send[512];

  while (1) {

    // if (xQueueReceive(Q_song_data, &bytes_to_send, portMAX_DELAY)) {
    //   for (int I = 0; I < 512; I++) {
    //     fprintf(stderr, "QR: %c\n", bytes_to_send[I]);
    //   }
    // }

    xQueueReceive(Q_song_data, &bytes_to_send, portMAX_DELAY);

    gpio__set(XCS);

    for (int i = 0; i < 512; i++) {
      SDI_WRITE(bytes_to_send[i]);
      if ((i % 32) == 0) {
        while (!gpio__get(DREQ)) {
          vTaskDelay(1);
        }
      }
    }
  }
}

void suspend_task(void *param) {
  while (1) {
    if (xSemaphoreTake(SW2_pressed, portMAX_DELAY)) {
      vTaskSuspend(play);
      fprintf(stderr, "pause");
    }
  }
}

void resume_task(void *param) {
  while (1) {
    if (xSemaphoreTake(SW2_pressed, portMAX_DELAY)) {
      vTaskResume(play);
      fprintf(stderr, "play");
    }
  }
}

void GPIO_init() {
  // decoder
  DREQ = gpio__construct_as_input(2, 0);
  XDCS = gpio__construct_as_output(2, 1);
  XCS = gpio__construct_as_output(2, 2);
  XRESET = gpio__construct_as_output(2, 4);

  // switches
  SW3 = gpio__construct_as_input(0, 29);
  SW2 = gpio__construct_as_input(0, 30);
  SW1 = gpio__construct_as_input(0, 1);
  SW0 = gpio__construct_as_input(0, 0);
}

void pin29_isr() {
  if (SW0_state == menu) {
    SW3_state = previous;
    // use the previous function call here
  } else if (SW0_state == volume) {
    SW3_state = decrease;
    // use the decrease function call here
  }
}

void pin30_isr() {
  if (SW0_state == playing && SW2_state == play) {
    SW2_state = pause; // block from sending more data
    fprintf(stderr, "block");
    xSemaphoreGiveFromISR(SW3_pressed, NULL);
  } else if (SW0_state == playing && SW2_state == pause) {
    SW2_state = play; // stop blocking
    fprintf(stderr, "unblock");
    xSemaphoreGiveFromISR(SW2_pressed, NULL);
  }
}

void pin1_isr() {
  if (SW0_state == menu) {
    SW1_state = next;
    // use the next function call here
  } else if (SW0_state == volume) {
    SW1_state = increase;
    // use the increase function call here
  }
}

void pin0_isr() {
  if (SW0_state == playing) {
    SW0_state = menu; // allow redrawing of the 16x2 LCD
  } else if (SW0_state == menu) {
    SW0_state = volume; // do an SCI_WRITE to vol; decrease it by X amount
  } else if (SW0_state == volume) {
    SW0_state = playing; // Return to current song
  }
}

int main(void) {
  Q_song_name = xQueueCreate(1, 32);
  Q_song_data = xQueueCreate(1, sizeof(uint8_t) * 512);

  song_names = malloc(max_number_of_songs * sizeof(char *));

  for (int i = 0; i < max_number_of_songs; i++) {
    song_names[i] = malloc(32 * sizeof(char));
  }

  scan_files("/");

  for (int x = 0; x < total_number_of_songs; x++) {
    song_names[x] = song_names[x + 1];
  }

  GPIO_init();

  ssp0_init(5);

  MP3_RESET();
  MP3_init();

  // sj2_cli__init();

  gpio0__attach_interrupt(30, GPIO_INTR__RISING_EDGE, pin30_isr);
  gpio0__attach_interrupt(29, GPIO_INTR__RISING_EDGE, pin29_isr);
  gpio0__attach_interrupt(1, GPIO_INTR__RISING_EDGE, pin1_isr);
  gpio0__attach_interrupt(0, GPIO_INTR__RISING_EDGE, pin0_isr);

  SW3_pressed = xSemaphoreCreateBinary();
  SW2_pressed = xSemaphoreCreateBinary();
  SW1_pressed = xSemaphoreCreateBinary();
  SW0_pressed = xSemaphoreCreateBinary();

  NVIC_EnableIRQ(GPIO_IRQn);

  TaskHandle_t play;

  TaskHandle_t read;

  xTaskCreate(read_task, "read", 2048, NULL, PRIORITY_LOW, &read);

  xTaskCreate(player_task, "play", 2048, NULL, PRIORITY_LOW, &play);

  xTaskCreate(suspend_task, "suspend", 512, NULL, PRIORITY_LOW, NULL);

  xTaskCreate(resume_task, "resume", 512, NULL, PRIORITY_LOW, NULL);

  vTaskStartScheduler();

  return 0;
}
