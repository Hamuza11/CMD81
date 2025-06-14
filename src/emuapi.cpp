#include "pico.h"
#include <stdlib.h>
#include <limits.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <stdio.h>
#include <string.h>
#include "tusb.h"
#include "hid_usb.h"

#include "emuapi.h"
#include "emuvideo.h"
#include "emusound.h"
#include "emupriv.h"

#include "ini.h"
#include "iopins.h"

#ifdef PICO_SPI_LCD_SD_SHARE
#include "display.h"
#endif

/********************************
 * Menu file loader UI
 ********************************/
#include "ff.h"
static FATFS fatfs;
static FIL file;
static bool fsinit = false;
static char screen_dir[MAX_DIRECTORY_LEN] = "/screenshots/";

/********************************
 * File IO
 ********************************/
bool emu_FileOpen(const char *filepath, const char *mode)
{
  BYTE access = (*mode == 'w') ? (FA_WRITE | FA_CREATE_ALWAYS) : FA_READ;

  bool retval = false;

  printf("FileOpen %s\n", filepath);
  FRESULT res = f_open(&file, filepath, access);
  if (res == FR_OK)
  {
    retval = true;
  }
  else
  {
    printf("FileOpen failed, reason %i\n", res);
  }
  return (retval);
}

int emu_FileRead(void* buf, int size, int offset)
{
  unsigned int read = 0;

  if (size > 0)
  {
    if (offset)
    {
      if (f_lseek(&file, offset) != FR_OK)
      {
        printf("emu_FileRead seek failed\n");
        return 0;
      }
    }

    if (f_read(&file, buf, size, &read) != FR_OK)
    {
      printf("emu_FileRead read failed\n");
      read = 0;
    }
  }
  else
  {
    printf("emu_FileRead illegal size %i\n", size);
  }
  return read;
}

int emu_FileReadBytes(void* buf, unsigned int size)
{
  unsigned int read = 0;

  if (size > 0)
  {

    if (f_read(&file, buf, size, &read) != FR_OK)
    {
      printf("emu_FileReadBytes read failed\n");
      read = 0;
    }
    else
    {
      if (size != read)
      {
        printf("emu_FileReadBytes, not all read\n");
      }
    }
  }
  else
  {
    printf("emu_FileReadBytes illegal size %i\n", size);
  }
  return read;
}

int emu_FileWriteBytes(const void* buf, unsigned int size)
{
  unsigned int write = 0;

  if (size > 0)
  {

    if (f_write(&file, buf, size, &write) != FR_OK)
    {
      printf("emu_FileWriteBytes write failed\n");
      write = 0;

    }
    else
    {
      if (size != write)
      {
        printf("emu_FileWriteBytes, not all written\n");
      }
    }
  }
  else
  {
    printf("emu_FileReadBytes illegal size %i\n", size);
  }
  return write;
}

void emu_FileClose(void)
{
  f_close(&file);
}

unsigned int emu_FileSize(const char *filepath)
{
  int filesize = 0;
  FILINFO entry;
  if (!f_stat(filepath, &entry))
  {
    filesize = entry.fsize;
  }
  printf("FileSize of %s is %i\n", filepath, filesize);
  return (filesize);
}

bool emu_SaveFile(const char *filepath, void *buf, int size)
{
  printf("SaveFile %s Length %i\n", filepath, size);
  if (!(f_open(&file, filepath, FA_CREATE_ALWAYS | FA_WRITE)))
  {
    unsigned int retval = 0;
    if ((f_write(&file, buf, size, &retval)))
    {
      printf("file write failed\n");
    }
    f_close(&file);
  }
  else
  {
    printf("file open failed\n");
    return false;
  }

  // Save the file name, so can be used in screenshots
  emu_SetFileName(filepath);
  return true;
}

bool emu_fileExists(const char* file_name)
{
  FILINFO entry;
  FRESULT fr = f_stat(file_name, &entry);
  return (fr == FR_OK);
}

bool emu_GetScreenShotDir(char* directory)
{
  FRESULT res = f_mkdir (screen_dir);

  if((res == FR_EXIST) || (res == FR_OK))
  {
    strcpy(directory, screen_dir);
    return true;
  }
  return false;
}

/********************************
 * Initialization
 ********************************/
void emu_init(void)
{
    // Initialise sd card and read config
#ifdef PICO_LCD_CS_PIN
    gpio_init(PICO_LCD_CS_PIN);
    gpio_set_dir(PICO_LCD_CS_PIN, GPIO_OUT);
    gpio_put(PICO_LCD_CS_PIN, 1);
#endif

#ifdef PICO_TS_CS_PIN
    gpio_init(PICO_TS_CS_PIN);
    gpio_set_dir(PICO_TS_CS_PIN, GPIO_OUT);
    gpio_put(PICO_TS_CS_PIN, 1);
#endif

  FRESULT fr = f_mount(&fatfs, "0:", 1);

  if (fr != FR_OK)
  {
    printf("SDCard mount failed. Error %i\n", fr);
  }
  else
  {
    fsinit = true;
  }
  emu_ReadDefaultValues();

  // The config in the root directory can set the initial
  // directory, so need to read that directory too
  if (emu_GetDirectory()[0])
  {
    emu_ReadDefaultValues();
  }
#ifdef FLASH_LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

// Return if file system initialised
bool emu_fsInitialised(void)
{
  return fsinit;
}

/********************************
 * Control SD Card Access
 ********************************/
#ifdef PICO_SPI_LCD_SD_SHARE

// Obtain the SPI bus for the SD Card
void emu_lockSDCard(void)
{
  displayRequestSPIBus();
}

// Return the SPI Bus to the display
void emu_unlockSDCard(void)
{
  displayGrantSPIBus();
}

#endif

/********************************
 * Configuration File
 ********************************/
#define CONFIG_FILE "config.ini"
#define REBOOT_FILE "reboot.ini"

typedef struct
{
  char up;
  char down;
  char left;
  char right;
  char button;
  int memory;
  int sound;
  ComputerType_T computer;
  bool NTSC;
  uint16_t VTol;
  bool centre;
  bool WRX;
  bool QSUDG;
  bool CHR128;
  bool M1NOT;
  bool lowRAM;
  bool acb;
  bool doubleShift;
  bool extendFile;
  int menuBorder;
  bool allFiles;
  FiveSevenSix_T fiveSevenSix;
  FrameSync_T frameSync;
  bool lcdInvertColour;
  bool lcdskipFrame;
  bool lcdRotate;
  bool lcdReflect;
  bool lcdBGR;
  bool vga;
  bool ninePinJoystick;
  bool loadUsingROM;
  bool saveUsingROM;
} Configuration_T;

typedef struct
{
  const char *section;
  Configuration_T *conf;
  bool root;
} ConfHandler_T;

static char selection[MAX_FILENAME_LEN] = "";
static char dirPath[MAX_DIRECTORY_LEN] = "";

static Configuration_T specific;    // The specific settings for the file to be loaded
static Configuration_T general;     // The settings specified in the default section of directory config file
static Configuration_T root_config; // The settings specified in the default section of the root config file

static bool resetNeeded = false;

static bool setDirectory(const char* dir);
static char convert(const char *val);
static bool isEnabled(const char* val);
static int handler(void *user, const char *section, const char *name,
                   const char *value);
static int ini_parse_fatfs(const char *filename, ini_handler handler, void *user);

int emu_SoundRequested(void)
{
#ifndef PICO_NO_SOUND
  return specific.sound;
#else
  return SOUND_TYPE_NONE;
#endif
}

bool emu_ACBRequested(void)
{
  // Do not allow stereo on a mono board
#if defined(I2S) || defined(SOUND_HDMI)
  return specific.acb;
#else
  return specific.acb && (AUDIO_PIN_L != AUDIO_PIN_R);
#endif
}

bool emu_ACBPossible(void)
{
    return (AUDIO_PIN_L != AUDIO_PIN_R);
}

bool emu_ZX80Requested(void)
{
  return ((specific.computer == ZX80_4K) ||((specific.computer == ZX80_8K)));
}

bool emu_ROM4KRequested(void)
{
  return (specific.computer == ZX80_4K);
}

ComputerType_T emu_ComputerRequested(void)
{
  return specific.computer;
}

int emu_MemoryRequested(void)
{
  // Do not allow more than 16k with QSUDG graphics
  return ((emu_QSUDGRequested()) && (specific.memory>16)) ? 16 : specific.memory;
}

bool emu_NTSCRequested(void)
{
  return specific.NTSC;
}

bool emu_Centre(void)
{
  return specific.centre;
}

#define ZX80_PIXEL_OFFSET 6

int emu_CentreX(void)
{
  if (specific.centre)
      return (disp.adjust_x + (zx80 ? ZX80_PIXEL_OFFSET : 0));
  else
    return 0;
}

int emu_CentreY(void)
{
  if (specific.centre)
    return specific.NTSC ? 16 : -8;
  else
    return 0;
}

uint16_t emu_VTol(void)
{
  return specific.VTol;
}

bool emu_WRXRequested(void)
{
  // WRX always enabled if no memory expansion
  return ((specific.WRX || (specific.memory <= 2)) && (!specific.CHR128));
}

bool emu_M1NOTRequested(void)
{
  return (specific.M1NOT && (specific.memory>=32) && (!emu_QSUDGRequested()));
}

bool emu_LowRAMRequested(void)
{
  return (specific.lowRAM || specific.CHR128);
}

bool emu_QSUDGRequested(void)
{
  return (specific.QSUDG && (!specific.CHR128));
}

bool emu_CHR128Requested(void)
{
  return specific.CHR128;
}

bool emu_DoubleShiftRequested(void)
{
  return specific.doubleShift;
}

bool emu_ExtendFileRequested(void)
{
  return specific.extendFile;
}

bool emu_AllFilesRequested(void)
{
  return specific.allFiles;
}

bool emu_NinePinJoystickRequested(void)
{
#ifdef NINEPIN_JOYSTICK
  return specific.ninePinJoystick;
#else
  return false;
#endif
}

bool emu_loadUsingROMRequested(void)
{
  return specific.loadUsingROM;
}

bool emu_saveUsingROMRequested(void)
{
  return specific.saveUsingROM;
}

int emu_MenuBorderRequested(void)
{
  return specific.menuBorder;
}

bool emu_ResetNeeded(void)
{
  return resetNeeded;
}

FrameSync_T emu_FrameSyncRequested(void)
{
  if (specific.lcdskipFrame && (specific.frameSync == SYNC_ON_INTERLACED))
  {
    return SYNC_ON;
  }
  else
  {
    return specific.frameSync;
  }
}

FiveSevenSix_T emu_576Requested(void)
{
  return specific.fiveSevenSix;
}

bool emu_lcdInvertColourRequested(void)
{
  return specific.lcdInvertColour;
}

bool emu_lcdSkipFrameRequested(void)
{
  return specific.lcdskipFrame;
}

bool emu_lcdRotateRequested(void)
{
  return specific.lcdRotate;
}

bool emu_lcdReflectRequested(void)
{
  return specific.lcdReflect;
}

bool emu_lcdBGRRequested(void)
{
  return specific.lcdBGR;
}

bool emu_vgaRequested(void)
{
  return specific.vga;
}

const char* emu_GetDirectory(void)
{
  return dirPath;
}

const char* emu_GetFileName(void)
{
  if (selection[0])
    return selection;
  else
    return NULL;
}

void emu_SetFileName(const char* name)
{
  if (name != NULL && name[0])
  {
    // Extract the file name from the full path
    const char* nameStrt = strrchr(name, '/');
    nameStrt = (nameStrt == NULL) ? name : nameStrt + 1;

    strncpy(selection, nameStrt, MAX_FILENAME_LEN-1);
    selection[MAX_FILENAME_LEN-1] = 0;
  }
  else
  {
    selection[0] = 0;
  }
}

void emu_SetDirectory(const char* dir)
{
  if (setDirectory(dir))
  {
    emu_ReadDefaultValues();
  }
}

static bool setDirectory(const char* dir)
{
  bool retVal = false;

  if (dir!=NULL)
  {
    if (dir[0])
    {
      // Append a final forward slash if necessary
      if (dir[strlen(dir) - 1] != '/')
      {
        if (strncasecmp(dirPath, dir, strlen(dir) - 1) ||
            (strlen(dir) != (strlen(dirPath) - 1)))
        {
          strncpy(dirPath, dir, MAX_DIRECTORY_LEN-2);
          dirPath[MAX_DIRECTORY_LEN-2] = 0;
          strcat(dirPath, "/");
          retVal = true;
        }
      }
      else
      {
        if (strncasecmp(dirPath, dir, strlen(dir)) ||
            (strlen(dir) != strlen(dirPath)))
        {
          strncpy(dirPath, dir, MAX_DIRECTORY_LEN-1);
          dirPath[MAX_DIRECTORY_LEN-1] = 0;
          retVal = true;
        }
      }
    }
    else
    {
      if (dirPath[0])
      {
        // Moving to root directory
        dirPath[0] = 0;
        retVal = true;
      }
    }

    // If changed, prune the directory of any navigation, do this after
    // the copy, as the string passed in is const
    if (retVal)
    {
      char* updir = 0;
      do
      {
        // search for "../"
        // Note that if "../" is present then we know that the path is valid
        updir = strstr(dirPath, "../");

        if (updir)
        {
          // manually search back for the previous directory separator
          // There will not be one for root
          char* start = updir-2;

          while ((*start != '/') && (start != dirPath))
          {
            --start;
          }

          // Start of path does not have '/'
          start += (start != dirPath) ? 1 : 0;

          // Now close the gap, copying up to and including the null terminator
          do
          {
            *start++ = updir[3];
          } while (updir++[3]);
        }
      } while (updir != 0);   // In case moved up multiple directories

      // Strip out all "./"
      do
      {
        updir = strstr(dirPath, "./");

        if (updir)
        {
          // Close the gap
          do
          {
            updir[0] = updir[2];
          } while (updir++[2]);

          printf("Modified . dir %s\n", dirPath);
        }
      } while (updir != 0);
    }
  }
  return retVal;
}

static void setScreenshotDirectory(const char* dir)
{
  // Prepend a slash if necessary
  if (dir[0] != '/')
  {
    strcpy(screen_dir, "/");
    strncat(screen_dir, dir, MAX_DIRECTORY_LEN-1);
    screen_dir[MAX_DIRECTORY_LEN-1] = 0;

    // And the end too
    if ((dir[strlen(dir) - 1] != '/') && (strlen(screen_dir) < MAX_DIRECTORY_LEN-1))
    {
      strcat(screen_dir,"/");
    }
  }
}

void emu_SetRom4K(bool Rom4k)
{
  if (Rom4k)
  {
    general.computer = ZX80_4K;
    specific.computer = ZX80_4K;
  }
  else if (general.computer == ZX80_4K)
  {
    general.computer = ZX81;
    specific.computer = ZX81;
  }
}

void emu_SetComputer(ComputerType_T computer)
{
  general.computer = computer;
  specific.computer = computer;
}

void emu_SetFrameSync(FrameSync_T fsync)
{
  general.frameSync = fsync;
  specific.frameSync = fsync;
}

extern void emu_SetNTSC(bool ntsc)
{
  general.NTSC = ntsc;
  specific.NTSC = ntsc;
}

void emu_SetVTol(uint16_t vTol)
{
  general.VTol = vTol;
  specific.VTol = vTol;
}

void emu_SetWRX(bool wrx)
{
  general.WRX = wrx;
  specific.WRX = wrx;
}

void emu_SetCentre(bool centre)
{
  general.centre = centre;
  specific.centre = centre;
}

void emu_SetSound(int soundType)
{
#ifndef PICO_NO_SOUND
  general.sound = soundType;
  specific.sound = soundType;
#else
  (void)soundType;

  general.sound = SOUND_TYPE_NONE;
  specific.sound = SOUND_TYPE_NONE;
#endif
}

void emu_SetACB(bool stereo)
{
  general.acb = stereo;
  specific.acb = stereo;
}

void emu_SetMemory(int memory)
{
  general.memory = memory;
  specific.memory = memory;
}

void emu_SetLowRAM(bool lowRAM)
{
  general.lowRAM = lowRAM;
  specific.lowRAM = lowRAM;
}

void emu_SetM1NOT(bool m1NOT)
{
  general.M1NOT = m1NOT;
  specific.M1NOT = m1NOT;
}

void emu_SetLoadROM(bool loadROM)
{
  general.loadUsingROM = loadROM;
  specific.loadUsingROM = loadROM;
}

void emu_SetSaveROM(bool saveROM)
{
  general.saveUsingROM = saveROM;
  specific.saveUsingROM = saveROM;
}

void emu_SetQSUDG(bool qsudg)
{
  general.QSUDG = qsudg;
  specific.QSUDG = qsudg;
}

void emu_SetCHR128(bool chr128)
{
  general.CHR128 = chr128;
  specific.CHR128 = chr128;
}

void emu_SetRebootMode(FiveSevenSix_T mode, const char* dirname, const char* filename)
{
  char filepath[MAX_FULLPATH_LEN];

  strcpy(filepath, "/");
  strcat(filepath, REBOOT_FILE);

  // Open the file
  EMU_LOCK_SDCARD
  FRESULT res = f_open(&file, filepath, FA_CREATE_ALWAYS|FA_WRITE);
  if (res == FR_OK)
  {
    UINT bw;
    strcpy(filepath, "[Default]\n");
    f_write(&file, filepath, strlen(filepath), &bw);
    sprintf(filepath, "FiveSevenSix = %s\n", (mode == MATCH) ? "MATCH" : (mode == ON) ? "ON" : "OFF");
    f_write(&file, filepath, strlen(filepath), &bw);
    if (filename)
    {
      sprintf(filepath, "Load = %s\n", filename);
      f_write(&file, filepath, strlen(filepath), &bw);

      if (dirname)
      {
        sprintf(filepath, "Dir = %s\n", dirname);
        f_write(&file, filepath, strlen(filepath), &bw);
      }
    }
    f_close(&file);

    EMU_UNLOCK_SDCARD

    // Set the watchdog to trigger a reboot
    watchdog_enable(10, true);

    // Stop until reboot
    sleep_ms(50);
  }
  else
  {
    EMU_UNLOCK_SDCARD
  }
}

static char convert(const char *val)
{
  if (!strcasecmp(val, "ENTER"))
  {
    return 0xd;
  }
  else if (!strcasecmp(val, "SPACE"))
  {
    return ' ';
  }
  else
  {
    return val[0];
  }
}

static bool isEnabled(const char* val)
{
  return (strcasecmp(val, "OFF") && strcasecmp(val, "0") && strcasecmp(val, "FALSE"));
}

static int handler(void *user, const char *section, const char *name,
                   const char *value)
{
  ConfHandler_T *c = (ConfHandler_T *)user;
  if (!strcasecmp(section, c->section))
  {
    if (!strcasecmp(name, "UP"))
    {
      c->conf->up = convert(value);
    }
    else if (!strcasecmp(name, "DOWN"))
    {
      c->conf->down = convert(value);
    }
    else if (!strcasecmp(name, "LEFT"))
    {
      c->conf->left = convert(value);
    }
    else if (!strcasecmp(name, "RIGHT"))
    {
      c->conf->right = convert(value);
    }
    else if (!strcasecmp(name, "BUTTON"))
    {
      c->conf->button = convert(value);
    }
    else if (!strcasecmp(name, "SOUND"))
    {
      // Sound enabled to ZonX if entry exists and is not "OFF", "NONE" or 0
      if ((strcasecmp(value, "OFF") == 0) || (strcasecmp(value, "0") == 0) || (strcasecmp(value, "NONE") == 0))
      {
        c->conf->sound = SOUND_TYPE_NONE;
      }
      else if ((strcasecmp(value, "QUICKSILVA") == 0) || (strcasecmp(value, "QS") == 0))
      {
        c->conf->sound = SOUND_TYPE_QUICKSILVA;
      }
      else if ((strcasecmp(value, "TV") == 0) || (strcasecmp(value, "VSYNC") == 0))
      {
        c->conf->sound = SOUND_TYPE_VSYNC;
      }
      else if (strcasecmp(value, "CHROMA") == 0)
      {
        c->conf->sound = SOUND_TYPE_CHROMA;
      }
      else
      {
        c->conf->sound = SOUND_TYPE_ZONX;
      }
    }
    else if (!strcasecmp(name, "ACB"))
    {
      // ACB stereo enabled if entry exists and is not "OFF" or 0
      c->conf->acb = isEnabled(value);
    }
    else if (!strcasecmp(name, "COMPUTER"))
    {
      // ZX81 enabled unless entry exists and is set to "ZX80" or "ZX81x2"
      if ((strcasecmp(value, "ZX804K") == 0) || (strcasecmp(value, "ZX80-4K") == 0) || (strcasecmp(value, "ZX80") == 0))
      {
        c->conf->computer = ZX80_4K;
      }
      else if ((strcasecmp(value, "ZX808K") == 0) || (strcasecmp(value, "ZX80-8K") == 0))
      {
        c->conf->computer = ZX80_8K;
      }
      else if (strcasecmp(value, "ZX81x2") == 0)
      {
        c->conf->computer = ZX81X2;
      }
      else
      {
        c->conf->computer = ZX81;
      }
    }
    else if (!strcasecmp(name, "M1NOT"))
    {
      // M1NOT enabled if entry exists and is not "OFF" or 0
      c->conf->M1NOT = isEnabled(value);
    }
    else if (!strcasecmp(name, "WRX"))
    {
      // WRX enabled if entry exists and is not "OFF" or 0
      c->conf->WRX = isEnabled(value);
    }
    else if (!strcasecmp(name, "LowRAM"))
    {
      // RAM expansion in 0x2000 to 0x3FFF range enabled
      // if entry exists and is not "OFF" or 0
      c->conf->lowRAM = isEnabled(value);
    }
    else if (!strcasecmp(name, "QSUDG"))
    {
      // QSUDG at 0x8400 to 0x87FF enabled
      // if entry exists and is not "OFF" or 0
      c->conf->QSUDG = isEnabled(value);
    }
    else if (!strcasecmp(name, "CHR128"))
    {
      // CHR128 enabled in LowRAM, with QSUDG and WRX disabled
      // if entry exists and is not "OFF" or 0
      c->conf->CHR128 = isEnabled(value);
    }
    else if (!strcasecmp(name, "NTSC"))
    {
      // Defaults to off
      c->conf->NTSC = isEnabled(value);
    }
    else if (!strcasecmp(name, "VTOL"))
    {
      // If it is not a positive number in range 1 to 200 then use default
      long res=strtol(value, NULL, 10);
      if ((res == LONG_MIN || res == LONG_MAX  || res <= 0 || res > 200))
      {
        res = VTOL;
      }
      c->conf->VTol = (uint16_t)res;
    }
    else if (!strcasecmp(name, "Centre"))
    {
      // Defaults to on, set to Off or 0 to turn off
      c->conf->centre = isEnabled(value);
    }
    else if (!strcasecmp(name, "FrameSync"))
    {
      if ((!strcasecmp(value, "Interlaced")) || (!strcasecmp(value, "Interlace")))
      {
        c->conf->frameSync = SYNC_ON_INTERLACED;
      }
      else if (isEnabled(value))
      {
        c->conf->frameSync = SYNC_ON;
      }
      else
      {
        // Defaults to off
        c->conf->frameSync = SYNC_OFF;
      }
    }
    else if (!strcasecmp(name, "MEMORY"))
    {
      long res=strtol(value, NULL, 10);

      if ((res == LONG_MIN || res == LONG_MAX  || res <= 0))
      {
        // Default to 16 if entry is not convertible
        res = 16;
      }
      else if (res >= 48)
      {
        // Never allow more than 48k (0x2000 to 0x3FFFF specified
        // through WRX flag)
        res = 48;
      }
      else if (res >=32)
      {
        res = 32;
      }
      else if (res >= 16)
      {
        res = 16;
      }
      else if (res >= 4)
      {
        res = 4;
      }
      c->conf->memory = (int)res;
    }
    else if (!strcasecmp(name, "ExtendFile"))
    {
        // Defaults to off
        c->conf->extendFile = isEnabled(value);
    }
    else if (!strcasecmp(name, "FiveSevenSix"))
    {
      if (!strcasecmp(value, "Match"))
      {
        c->conf->fiveSevenSix = MATCH;
      }
      else if (isEnabled(value))
      {
        c->conf->fiveSevenSix = ON;
      }
      else
      {
        // Defaults to off
        c->conf->fiveSevenSix = OFF;
      }
    }
    else if ((!strcasecmp(section, "default")) && c->root)
    {
      // Following only allowed in default section of root config
      if (!strcasecmp(name, "Load"))
      {
        strcpy(selection, value);
      }
      else if (!strcasecmp(name, "Dir"))
      {
        setDirectory(value);
      }
      else if (!strcasecmp(name, "ScreenshotDir"))
      {
        setScreenshotDirectory(value);

      }
      else if (!strcasecmp(name, "doubleShift"))
      {
        // Defaults to on, set to Off or 0 to turn off
        c->conf->doubleShift = isEnabled(value);
      }
      else if (!strcasecmp(name, "AllFiles"))
      {
        // Defaults to off
        c->conf->allFiles = isEnabled(value);
      }
      else if (!strcasecmp(name, "menuBorder"))
      {
        long res=strtol(value, NULL, 10);

        if ((res == LONG_MIN) || (res == LONG_MAX)  || (res < 0))
        {
          // Defaults to 1
          res = 1;
        }
        if (res > 2)
        {
          res = 1;
        }
        c->conf->menuBorder = res;
      }
      else if ((!strcasecmp(name, "NinePinJoystick")))
      {
        // Defaults to off
        c->conf->ninePinJoystick = isEnabled(value);
      }
      else if ((!strcasecmp(name, "LoadUsingROM")))
      {
        // Defaults to off
        c->conf->loadUsingROM = isEnabled(value);
      }
      else if ((!strcasecmp(name, "SaveUsingROM")))
      {
        // Defaults to off
        c->conf->saveUsingROM = isEnabled(value);
      }
#ifdef PICO_LCD_CS_PIN
      else if (!strcasecmp(name, "LCDInvertColour"))
      {
        // Defaults to off
        c->conf->lcdInvertColour = isEnabled(value);
      }
      else if (!strcasecmp(name, "LCDSkipFRame"))
      {
        // Defaults to off
        c->conf->lcdskipFrame = isEnabled(value);
      }
      else if (!strcasecmp(name, "LCDRotate"))
      {
        // Defaults to off
        c->conf->lcdRotate = isEnabled(value);
      }
      else if (!strcasecmp(name, "LCDReflect"))
      {
        // Defaults to off
        c->conf->lcdReflect = isEnabled(value);
      }
      else if (!strcasecmp(name, "LCDBGR"))
      {
        // Defaults to off
        c->conf->lcdBGR = isEnabled(value);
      }
      else if (!strcasecmp(name, "VGA"))
      {
        // Defaults to off
        c->conf->vga = isEnabled(value);
      }
#endif
      else
      {
        return 0;
      }
    }
    else
    {
      return 0;
    }
  }
  else
  {
    return 0;
  }
  return 1;
}

static int ini_parse_fatfs(const char *filename, ini_handler handler, void *user)
{
  FRESULT result;
  FIL file;
  int error;

  result = f_open(&file, filename, FA_OPEN_EXISTING | FA_READ);
  if (result != FR_OK)
    return -1;
  error = ini_parse_stream((ini_reader)f_gets, &file, handler, user);
  f_close(&file);
  return error;
}

void emu_ReadDefaultValues(void)
{
  ConfHandler_T hand;
  char name[] = "default";
  static bool first = true;

  if (first)
  {
    // Set values in case ini file cannot be read
    general.up = '7';
    general.down = '6';
    general.left = '5';
    general.right = '8';
    general.button = '0';
    general.sound = SOUND_TYPE_NONE;
    general.acb = false;
    general.computer = ZX81;
    general.M1NOT = false;
    general.WRX = false;
    general.CHR128 = false;
    general.QSUDG = false;
    general.lowRAM = false;
    general.memory = 16;
    general.NTSC = false;
    general.VTol = VTOL;
    general.centre = true;
    general.doubleShift = true;
    general.extendFile = true;
    general.allFiles = false;
    general.menuBorder = 1;
    general.fiveSevenSix = OFF;
    general.frameSync = SYNC_OFF;
    general.lcdInvertColour = false;
    general.lcdReflect = false;
    general.lcdRotate = false;
    general.lcdskipFrame = false;
    general.lcdBGR = false;
    general.vga = false;
    general.ninePinJoystick = false;
    general.loadUsingROM = false;
    general.saveUsingROM = false;

#ifdef PICO_LCDWS28_BOARD
    general.lcdInvertColour = true;
    general.lcdReflect = true;
    general.lcdRotate = true;
#endif

    selection[0] = 0;
  }
  else
  {
    memcpy(&general, &root_config, sizeof(general));
  }

  hand.conf = &general;
  hand.section = name;
  hand.root = first;

  // Read the config file from the current directory as the file
  char config[MAX_FULLPATH_LEN];
  strcpy(config, dirPath);
  strcat(config, CONFIG_FILE);

  ini_parse_fatfs(config, handler, &hand);

  if (first)
  {
    // Check for reboot
    config[0] = 0;
    strcat(config, REBOOT_FILE);
    if (emu_FileSize(config))
    {
      // Parse reboot.ini if it exists
      ini_parse_fatfs(config, handler, &hand);

      // remove the file
      f_unlink(config);
    }
    memcpy(&root_config, &general, sizeof(general));
  }

  // Set the specific values in the first instance, in case a file is not loaded
  if (first)
  {
    memcpy(&specific, &general, sizeof(specific));
    first = false;
  }
}

// Note this should be called after the filename has been validated
// If a display resolution or refresh change is needed this may trigger
// a reboot
void emu_ReadSpecificValues(const char *filename)
{
  ConfHandler_T hand;
  Configuration_T used;

  // Store the settings we have before
  memcpy(&used, &specific, (sizeof(specific)));

  // Reset settings to the defaults before attempting to specialise
  memcpy(&specific, &general, (sizeof(specific)));
  resetNeeded = false;

  if (filename)
  {
    hand.conf = &specific;
    hand.root = false;

    // Read the config file from the same directory as the file
    hand.section = &filename[strlen(dirPath)];
    char config[MAX_FULLPATH_LEN];
    strcpy(config, dirPath);
    strcat(config, CONFIG_FILE);

    ini_parse_fatfs(config, handler, &hand);

    // Determine whether a reboot is required
    if (specific.fiveSevenSix != used.fiveSevenSix)
    {
      emu_SetRebootMode(specific.fiveSevenSix, emu_GetDirectory(), &filename[strlen(dirPath)]);
      // emu_SetRebootMode will never return
      // The board will reboot and load the file
    }

    // determine whether a reset is required
    resetNeeded =  ((specific.M1NOT != used.M1NOT) ||
                    (specific.loadUsingROM != used.loadUsingROM) ||
                    (specific.saveUsingROM != used.saveUsingROM) ||
                    (specific.memory != used.memory) ||
                    (specific.computer != used.computer) ||
                    (specific.CHR128 != used.CHR128) ||
                    (specific.QSUDG != used.QSUDG) ||
                    (specific.lowRAM != used.lowRAM));
  }
}

/********************************
 * Snapshot
 ********************************/
#define SNAPSHOT_ID       0x50414E53          // Little endian 'SNAP'
#define SUPPORTED_VERSION 0x00010001          // Major and minor versions
#define SECOND_OFFSET     57                  // Start of second data section

#ifdef __cplusplus
extern "C" {
#endif
extern bool save_snap_zx8x(void);
extern bool load_snap_zx8x(uint32_t version);
extern bool save_snap_z80(void);
extern bool load_snap_z80(uint32_t version);
extern bool display_save_snap(void);
extern bool display_load_snap(uint32_t version);

#ifdef __cplusplus
}
#endif

bool emu_loadSnapshotSpecific(const char* filename, const char* fullpathname)
{
  bool ret = false;
  FiveSevenSix_T state;
  printf("emu_loadSnapshotSpecific %s \n", fullpathname);

  EMU_LOCK_SDCARD
  if (!(f_open(&file, fullpathname, FA_READ)))
  {
    // Check identifer
    uint32_t id;
    if (!emu_FileReadBytes(&id, sizeof(id)) || id != SNAPSHOT_ID)
    {
      printf("emu_loadSnapshotSpecific wrong id\n");
    }
    else if (!emu_FileReadBytes(&id, sizeof(id)) || (id != SUPPORTED_VERSION))
    {
      printf("emu_loadSnapshotSpecific wrong version %li\n", id);
    }
    else if (!emu_FileReadBytes(&state, sizeof(state)) || state != emu_576Requested())
    {
      printf("emu_loadSnapshotSpecific wrong display type - triggering reboot\n");
      emu_SetRebootMode(state, emu_GetDirectory(), filename);
    }
    else if (!emu_FileReadBytes(&specific, sizeof(specific)))
    {
      printf("emu_loadSnapshotSpecific read specific failed\n");
    }
    else if (f_tell(&file) != SECOND_OFFSET)
    {
      printf("emu_loadSnapshotSpecific wrong data size %lli\n", f_tell(&file));
    }
    else
    {
      ret = true;
    }
    f_close(&file);
  }
  else
  {
    printf("file open failed\n");
  }
  EMU_UNLOCK_SDCARD
  return ret;
}

bool emu_loadSnapshotData(const char* fullpathname)
{
  bool ret = false;
  printf("emu_loadSnapshotData %s \n", fullpathname);

  EMU_LOCK_SDCARD
  if (!(f_open(&file, fullpathname, FA_READ)))
  {
    // Check identifer
    uint32_t id;
    uint32_t version;

    if (!emu_FileReadBytes(&id, sizeof(id)) || id != SNAPSHOT_ID)
    {
      printf("emu_loadSnapshotData wrong id\n");
    }
    else if (!emu_FileReadBytes(&version, sizeof(version)) || version != SUPPORTED_VERSION)
    {
      printf("emu_loadSnapshotData wrong version %li\n", version);
    }
    else if (f_lseek(&file, SECOND_OFFSET) || (f_tell(&file) != SECOND_OFFSET))
    {
      printf("emu_loadSnapshotData move to start of second data failed\n");
    }
    else if (!display_load_snap(version))
    {
      printf("display_load_snap failed\n");
    }
    else if (!load_snap_z80(version))
    {
      printf("load_snap_z80 failed\n");
    }
    else if (!load_snap_zx8x(version))
    {
      printf("load_snap_zx8x failed\n");
    }
    else if (!emu_sndLoadSnap(version))
    {
      printf("emu_sndLoadSnap failed\n");
    }
    else
    {
      ret = true;
    }
    f_close(&file);
  }
  else
  {
    printf("file open failed\n");
  }
  EMU_UNLOCK_SDCARD
  return ret;
}

bool emu_saveSnapshot(const char* fullpathname)
{
  bool ret = false;
  FiveSevenSix_T state = emu_576Requested();

  printf("saveSnapshot %s \n", fullpathname);

  EMU_LOCK_SDCARD
  if (!(f_open(&file, fullpathname, FA_CREATE_ALWAYS | FA_WRITE)))
  {
    // write identifer
    uint32_t id = SNAPSHOT_ID;
    uint32_t version = SUPPORTED_VERSION;
    if (!emu_FileWriteBytes(&id, sizeof(id)))
    {
      printf("emu_saveSnapshot write id failed\n");
    }
    else if (!emu_FileWriteBytes(&version, sizeof(version)))
    {
      printf("emu_saveSnapshot write version failed\n");
    }
    else if (!emu_FileWriteBytes(&state, sizeof(state)))
    {
      printf("emu_saveSnapshot write display state failed\n");
    }
    else if (!emu_FileWriteBytes(&specific, sizeof(specific)))
    {
      printf("emu_saveSnapshot write specific failed\n");
    }
    else if (f_tell(&file) != SECOND_OFFSET)
    {
      printf("emu_saveSnapshot wrong offset - %lli\n", f_tell(&file));
    }
    else if (!display_save_snap())
    {
      printf("display_save_snap failed\n");
    }
    else if (!save_snap_z80())
    {
      printf("save_snap_z80 failed\n");
    }
    else if (!save_snap_zx8x())
    {
      printf("save_snap_zx8x failed\n");
    }
    else if (!emu_sndSaveSnap())
    {
      printf("emu_sndSaveSnap failed\n");
    }
    else
    {
      ret = true;
    }
    f_close(&file);
  }
  else
  {
    printf("file open failed\n");
  }
  EMU_UNLOCK_SDCARD
  return ret;
}

/********************************
 * Joystick and file test
 ********************************/
#ifdef NINEPIN_JOYSTICK
enum joystick_t {
    UP = 0,
    DOWN = 1,
    LEFT = 2,
    RIGHT = 3,
    BUTTON = 4
};

static uint gpmap[5];
static void ninePinJoystickToKeyboard(void);
#endif

void emu_JoystickInitialiseNinePin(void)
{
  // Only initialise joystick if defined for build and selected in config
#ifdef NINEPIN_JOYSTICK
  if (emu_NinePinJoystickRequested())
  {
    gpmap[UP] = NINEPIN_UP;
    gpmap[DOWN] = NINEPIN_DOWN;
    gpmap[LEFT] = NINEPIN_LEFT;
    gpmap[RIGHT] = NINEPIN_RIGHT;
    gpmap[BUTTON] = NINEPIN_BUTTON;

    for (int i = 0; i < 5; ++i)
    {
      // Set pins to input and pull up to 3.3V
      gpio_init(gpmap[i]);
      gpio_set_dir(gpmap[i], GPIO_IN);
      gpio_pull_up(gpmap[i]);
    }
  }
#endif
}


void emu_JoystickParse(void)
{
#ifdef NINEPIN_JOYSTICK
  if (emu_NinePinJoystickRequested())
  {
    ninePinJoystickToKeyboard();
  }
#endif
  hidJoystickToKeyboard(1,
                        specific.up,
                        specific.down,
                        specific.left,
                        specific.right,
                        specific.button);

}

void emu_JoystickDeviceParse(bool up, bool down, bool left, bool right, bool button)
{
  // Handle device joystick
  if (up) hidInjectKey(specific.up);
  if (down) hidInjectKey(specific.down);
  if (left) hidInjectKey(specific.left);
  if (right) hidInjectKey(specific.right);
  if (button) hidInjectKey(specific.button);
}

#ifdef NINEPIN_JOYSTICK
static void ninePinJoystickToKeyboard(void)
{
  if (!gpio_get(gpmap[RIGHT]))
  {
    hidInjectKey(specific.right);
  } else if (!gpio_get(gpmap[LEFT]))
  {
    hidInjectKey(specific.left);
  }
  if (!gpio_get(gpmap[UP]))
  {
    hidInjectKey(specific.up);
  } else if (!gpio_get(gpmap[DOWN]))
  {
    hidInjectKey(specific.down);
  }
  if (!gpio_get(gpmap[BUTTON]))
  {
    hidInjectKey(specific.button);
  }
}
#endif

bool emu_EndsWith(const char * s, const char * suffix)
{
  bool retval = false;
  int len = strlen(s);
  int slen = strlen(suffix);
  if (len > slen)
  {
    if (!strcasecmp(&s[len-slen], suffix))
    {
      retval = true;
    }
  }
  return (retval);
}

/********************************
 * Chroma
 ********************************/
bool emu_chromaSupported(void)
{
    // Chroma supported for VGA, LCD and colour HDMI
#ifdef DVI_MONOCHROME_TMDS
    return false;
#else
    return true;
#endif
}

/********************************
 * Clock
 ********************************/
extern semaphore_t timer_sem;

void emu_WaitFor50HzTimer(void)
{
#ifdef TIME_SPARE
  static uint32_t count = 0;
  static uint64_t total_time;
  static uint32_t underrun;
  static int32_t  sound_prev = 0;
  static int64_t  int_prev = 0;

  uint64_t start = time_us_64();
#endif
  // Wait for the fifty Hz timer to fire
  sem_acquire_blocking(&timer_sem);

#ifdef TIME_SPARE
  uint64_t taken = (time_us_64() - start);
  if (taken < 100)
    underrun++;
  total_time += taken;

  if (++count == 500)
  {
    count = 0;
    int64_t ints = int_count + int_prev ;
    int_prev = -int_count;
    int32_t sound = sound_count + sound_prev;
    sound_prev = -sound_count;

    printf("ms: %lld U: %lu\n", total_time / 1000, underrun);
    printf("I: %lld S: %ld\n", ints, sound);
    total_time = 0;
    underrun = 0;
#ifdef FLASH_LED
    static bool led_on = false;
    led_on = !led_on;
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#endif
  }
#endif
}
