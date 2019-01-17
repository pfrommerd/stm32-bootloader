#include "Flash.hpp"

#include <stm32f7xx_hal.h>

static size_t SECTOR_OFFSETS[] = {
/*32kb*/    0x08000000,
/*32kb*/    0x08000000 + 1*(0x8000), /* 32kb offset */
/*32kb*/    0x08000000 + 2*(0x8000), /* 64kb offset */
/*32kb*/    0x08000000 + 3*(0x8000), /* 96kb offset */
/*128kb*/   0x08000000 + 1*(0x20000),/* 128kb offset */
/*256kb*/   0x08000000 + 1*(0x40000),/* 256kb offset */
/*256kb*/   0x08000000 + 2*(0x40000),/* 512kb offset */
/*256kb*/   0x08000000 + 3*(0x40000),/* 1024kb offset */
/*256kb*/   0x08000000 + 4*(0x40000),/* 1536kb offset */
/*256kb*/   0x08000000 + 5*(0x40000),/* 2048kb offset */
/*256kb*/   0x08000000 + 6*(0x40000) /* 2560kb offset */
};

static uint32_t SECTOR_INDICES[] = {
    FLASH_SECTOR_0,
    FLASH_SECTOR_1,
    FLASH_SECTOR_2,
    FLASH_SECTOR_3,
    FLASH_SECTOR_4,
    FLASH_SECTOR_5,
    FLASH_SECTOR_6,
    FLASH_SECTOR_7,
    FLASH_SECTOR_8,
    FLASH_SECTOR_9,
    FLASH_SECTOR_10,
    FLASH_SECTOR_11
};

namespace bootloader { namespace flash {
    void unlock() {
        HAL_FLASH_Unlock();
    }
    void lock() {
        HAL_FLASH_Lock();
    }

    int write(uint8_t* ptr, uint32_t data) {
        uint64_t loc = (size_t) ptr;

        for (int i = 0; i < 4; i++) { // because little-endian
            int ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,
                                        i + loc, (data >> 8*i) & 0xFF);
            if (ret != HAL_OK) return -1;
        }
        if (*((uint32_t*) ptr) != data) return -1;
        return 0;
    }


    int erase(uint8_t* start, size_t length) {
        FLASH_EraseInitTypeDef eraseDef;
        eraseDef.TypeErase = FLASH_TYPEERASE_SECTORS;

        int startIdx = -1;
        int endIdx = -1;
        for (int i = 0; i < 11; i++) {
            if (SECTOR_OFFSETS[i] == (size_t) start) startIdx = i;
        }
        for (int i = startIdx; i < 11; i++) {
            if (SECTOR_OFFSETS[i] >= (size_t) start + length) endIdx = i;
        }
        if (startIdx < 0 || endIdx < 0) return 1;

        eraseDef.Sector = SECTOR_INDICES[startIdx];
        eraseDef.NbSectors = endIdx = startIdx;
        eraseDef.VoltageRange = FLASH_VOLTAGE_RANGE_3;


        uint32_t error = HAL_FLASH_ERROR_NONE;
        HAL_FLASH_Unlock();
        HAL_FLASHEx_Erase(&eraseDef, &error);
        HAL_FLASH_Lock();

        return error != 0xFFFFFFFFU;
    }
}}
