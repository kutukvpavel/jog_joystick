#include "nvs.h"

#include "i2c_sync.h"

#include "../Core/Inc/crc.h"

#define MY_EEPROM_SIZE 512 //bytes
#define MY_EEPROM_ADDR 0xA0
#define MY_NVS_I2C_ADDR(mem_addr) (MY_EEPROM_ADDR | ((mem_addr & 0x700) >> 7))
#define MY_NVS_VER_ADDR 0u
#define MY_NVS_START_ADDRESS 8u
#define MY_NVS_VERSION 4u
#define MY_NVS_PAGE_SIZE 8u
#define MY_NVS_TOTAL_PAGES 64u
#define MY_NVS_TOTAL_SIZE (MY_NVS_PAGE_SIZE * MY_NVS_TOTAL_PAGES)
#define MY_RETRIES 3
#define PACKED_FOR_NVS __packed __aligned(sizeof(float))

namespace nvs
{
    struct __packed storage_preamble_t
    {
        uint8_t version;
        uint32_t crc;
    };
    struct PACKED_FOR_NVS storage_t
    {
        float rapid_feed_rate[TOTAL_AXES];
        float max_feed_rate[TOTAL_AXES];
        float min_feed_rate[TOTAL_AXES];
        float pot_low_threshold;
    };
    static storage_t storage = {
        .rapid_feed_rate = {
            300
        },
        .max_feed_rate = {
            100
        },
        .min_feed_rate = {
            0.1
        },
        .pot_low_threshold = 0.02
    };

    static storage_preamble_t preamble = {};
    HAL_StatusTypeDef eeprom_read(uint16_t addr, uint8_t* buf, uint16_t len)
    {
        assert_param((addr + len) < MY_NVS_TOTAL_SIZE);
        
        HAL_StatusTypeDef ret = HAL_ERROR;
        int retries = MY_RETRIES;
        while (retries--)
        {
            ret = i2c::mem_read(MY_NVS_I2C_ADDR(addr), addr & 0xFF, buf, len);
            if (ret == HAL_OK) break;
        }
        return ret;
    }
    HAL_StatusTypeDef eeprom_write(uint16_t addr, uint8_t* buf, uint16_t len)
    {
        static const TickType_t page_write_delay = pdMS_TO_TICKS(12);
        assert_param((addr + len) < MY_NVS_TOTAL_SIZE);
        assert_param(addr % MY_NVS_PAGE_SIZE == 0);
        HAL_StatusTypeDef status = HAL_OK;

        size_t full_pages = len / MY_NVS_PAGE_SIZE;
        size_t remainder = len % MY_NVS_PAGE_SIZE;
        DBG("Writing NVS: Full pages = %u, Remainder = %u", full_pages, remainder);
        uint16_t current_page_addr = addr;
        vTaskDelay(page_write_delay / 2);
        for (size_t i = 0; i < full_pages; i++)
        {
            status = i2c::mem_write(MY_NVS_I2C_ADDR(current_page_addr), current_page_addr & 0xFF, buf, MY_NVS_PAGE_SIZE);
            if (status != HAL_OK) break;
            //DBG("Written page #%u...", i);
            buf += MY_NVS_PAGE_SIZE;
            current_page_addr += MY_NVS_PAGE_SIZE;
            vTaskDelay(page_write_delay);
        }
        if (status != HAL_OK) return status;
        DBG("Pages written OK");
        if (remainder > 0)
        {
            status = i2c::mem_write(MY_NVS_I2C_ADDR(current_page_addr), current_page_addr & 0xFF, buf, remainder);
            vTaskDelay(page_write_delay / 2);
            DBG("Remainder written OK");
        }
        return status;
    }
    HAL_StatusTypeDef calculate_crc(uint32_t* crc, storage_t* buf)
    {
        static const size_t crc_buf_size_bytes = sizeof(storage_t);
        static const size_t crc_buf_size_words = crc_buf_size_bytes / sizeof(uint32_t) + 
            (((crc_buf_size_bytes % sizeof(uint32_t)) != 0) ? 1 : 0);

        uint32_t* crc_buf = new uint32_t[crc_buf_size_words]();
        if (!crc_buf) return HAL_ERROR;
        memcpy(crc_buf, buf, crc_buf_size_bytes);
        //memset(crc_buf + crc_buf_size_bytes, 0, crc_buf_size_words * sizeof(uint32_t) - crc_buf_size_bytes);
        hcrc.Instance->DR = UINT32_MAX;
        *crc = HAL_CRC_Calculate(&hcrc, crc_buf, crc_buf_size_words);
        delete[] crc_buf;
        DBG("CRC calc: %0lX", *crc);
        return HAL_OK;
    }

    HAL_StatusTypeDef init()
    {
        static_assert(MY_NVS_START_ADDRESS % MY_NVS_PAGE_SIZE == 0);
        static_assert((sizeof(storage_t) + sizeof(storage_preamble_t)) < (MY_EEPROM_SIZE - MY_NVS_START_ADDRESS));
        static_assert(sizeof(storage_preamble_t) < MY_NVS_PAGE_SIZE);
        HAL_StatusTypeDef ret;

        if ((ret = eeprom_read(MY_NVS_VER_ADDR, reinterpret_cast<uint8_t*>(&preamble), sizeof(preamble))) != HAL_OK) return ret;
        DBG("Detected NVS ver: %u, crc = %0lX", preamble.version, preamble.crc);
        if (preamble.version != MY_NVS_VERSION)
        {
            ERR("NVS version mismatch, should be: %u!", MY_NVS_VERSION);
            return HAL_ERROR;
        }
        return HAL_OK;
    }
    HAL_StatusTypeDef load()
    {
        HAL_StatusTypeDef ret;
        DBG("Loading NVS...");

        if (preamble.version != MY_NVS_VERSION) return HAL_ERROR;
        if ((ret = eeprom_read(MY_NVS_START_ADDRESS, reinterpret_cast<uint8_t*>(&storage), sizeof(storage))) != HAL_OK) return ret;
        //Check CRC
        uint32_t crc;
        if (calculate_crc(&crc, &storage) != HAL_OK)
        {
            ERR("Failed to calculate NVS CRC");
            return HAL_ERROR;
        }
        if (crc == preamble.crc)
        {
            DBG("NVS CRC OK");
            return HAL_OK;
        }
        else
        {
            ERR("NVS CRC doesn't match, stored = %0lX, calc = %0lX", preamble.crc, crc);
            return HAL_ERROR;
        }
    }
    HAL_StatusTypeDef save()
    {
        static storage_t double_buffer;
        HAL_StatusTypeDef ret;
        
        //Make a copy (due to volatile variables)
        memcpy(&double_buffer, &storage, sizeof(storage_t));
        //Compute CRC
        uint32_t crc;
        if (calculate_crc(&crc, &double_buffer) != HAL_OK)
        {
            ERR("Failed to calculate NVS CRC");
            return HAL_ERROR;
        }
        preamble.crc = crc;
        //Store 
        if ((ret = eeprom_write(MY_NVS_START_ADDRESS, reinterpret_cast<uint8_t*>(&double_buffer), sizeof(double_buffer))) 
            != HAL_OK) return ret;
        DBG("NVS Data written OK. Writing NVS version...");
        preamble.version = MY_NVS_VERSION;
        return eeprom_write(MY_NVS_VER_ADDR, reinterpret_cast<uint8_t*>(&preamble), sizeof(preamble));
    }
    HAL_StatusTypeDef reset()
    {
        uint8_t zero[] = { 0 };
        return eeprom_write(MY_NVS_VER_ADDR, zero, 1);
    }
    void dump_hex()
    {
        for (size_t i = 0; i < sizeof(storage); i++)
        {
            printf("0x%02X\n", reinterpret_cast<uint8_t*>(&storage)[i]);
        }
    }
    HAL_StatusTypeDef test()
    {
        uint8_t* sequential_buffer = new uint8_t[sizeof(storage_t)];
        assert_param(sequential_buffer);

        for (size_t i = 0; i < sizeof(storage_t); i++)
        {
            sequential_buffer[i] = static_cast<uint8_t>(i);
        }
        eeprom_write(MY_NVS_START_ADDRESS, sequential_buffer, sizeof(storage_t));
        HAL_Delay(5);

        uint8_t* readback_buffer = new uint8_t[sizeof(storage_t)];
        assert_param(readback_buffer);
        eeprom_read(MY_NVS_START_ADDRESS, readback_buffer, sizeof(storage_t));
        HAL_StatusTypeDef res = (memcmp(sequential_buffer, readback_buffer, sizeof(storage_t)) == 0) ? HAL_OK : HAL_ERROR;

        delete[] sequential_buffer;
        HAL_Delay(5);
        save();
        return res;
    }

    uint8_t get_stored_version()
    {
        return preamble.version;
    }
    uint8_t get_required_version()
    {
        return MY_NVS_VERSION;
    }
    bool get_version_match()
    {
        return MY_NVS_VERSION == preamble.version;
    }

    float get_rapid_speed(axis::types t)
    {
        return storage.rapid_feed_rate[static_cast<size_t>(t)];
    }
    float get_max_speed(axis::types t)
    {
        return storage.max_feed_rate[static_cast<size_t>(t)];
    }
    float get_min_speed(axis::types t)
    {
        return storage.min_feed_rate[static_cast<size_t>(t)];
    }

    void set_rapid_speed(axis::types t, float s)
    {
        storage.rapid_feed_rate[static_cast<size_t>(t)] = s;
    }
    void set_max_speed(axis::types t, float s)
    {
        storage.max_feed_rate[static_cast<size_t>(t)] = s;
    }
    void set_min_speed(axis::types t, float s)
    {
        storage.min_feed_rate[static_cast<size_t>(t)] = s;
    }
    
    float get_low_pot_threshold()
    {
        return storage.pot_low_threshold;
    }
    void set_low_pot_threshold(float v)
    {
        storage.pot_low_threshold = v;
    }
} // namespace nvs
