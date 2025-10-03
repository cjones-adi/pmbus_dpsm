#include "pmbus/data_conversion.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/**
 * @file data_conversion.c
 * @brief PMBus Data Conversion Framework Implementation
 */

/* Internal helper functions */
static uint32_t get_timestamp_ms(void);
static pmbus_error_t cache_conversion(pmbus_conversion_manager_t *manager,
                                     pmbus_data_format_t source_format,
                                     pmbus_data_format_t target_format,
                                     uint16_t raw_value,
                                     int32_t converted_value);
static bool lookup_cache(pmbus_conversion_manager_t *manager,
                        pmbus_data_format_t source_format,
                        pmbus_data_format_t target_format,
                        uint16_t raw_value,
                        int32_t *converted_value);

/* LINEAR11 Converter Implementation */
static pmbus_error_t linear11_to_raw(int32_t value, const pmbus_conversion_context_t *ctx, 
                                    uint16_t *raw) {
    if (raw == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    return pmbus_convert_int32_to_linear11(value, raw);
}

static pmbus_error_t linear11_from_raw(uint16_t raw, const pmbus_conversion_context_t *ctx,
                                      int32_t *value) {
    if (value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    return pmbus_convert_linear11_to_int32(raw, value);
}

static bool linear11_validate_range(int32_t value, const pmbus_conversion_context_t *ctx) {
    /* LINEAR11 can represent values from approximately -1024 * 2^15 to 1023 * 2^15 */
    const int32_t LINEAR11_MAX = 1023 * (1 << 15);
    const int32_t LINEAR11_MIN = -1024 * (1 << 15);
    
    return (value >= LINEAR11_MIN && value <= LINEAR11_MAX);
}

static int32_t linear11_get_precision(const pmbus_conversion_context_t *ctx) {
    /* Worst case precision is about 0.1% for LINEAR11 format */
    return 1000; /* 0.1% */
}

const pmbus_data_converter_t pmbus_linear11_converter = {
    .format = PMBUS_DATA_FORMAT_LINEAR11,
    .name = "LINEAR11",
    .to_raw = linear11_to_raw,
    .from_raw = linear11_from_raw,
    .validate_range = linear11_validate_range,
    .get_precision = linear11_get_precision,
    .scale_value = NULL,
    .optimize_conversion = NULL,
    .converter_data = NULL
};

/* LINEAR16 Converter Implementation */
static pmbus_error_t linear16_to_raw(int32_t value, const pmbus_conversion_context_t *ctx, 
                                    uint16_t *raw) {
    if (raw == NULL || ctx == NULL || ctx->format_params == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    const pmbus_linear16_config_t *config = (const pmbus_linear16_config_t*)ctx->format_params;
    return pmbus_convert_int32_to_linear16(value, config->exponent, raw);
}

static pmbus_error_t linear16_from_raw(uint16_t raw, const pmbus_conversion_context_t *ctx,
                                      int32_t *value) {
    if (value == NULL || ctx == NULL || ctx->format_params == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    const pmbus_linear16_config_t *config = (const pmbus_linear16_config_t*)ctx->format_params;
    return pmbus_convert_linear16_to_int32(raw, config->exponent, value);
}

static bool linear16_validate_range(int32_t value, const pmbus_conversion_context_t *ctx) {
    if (ctx == NULL || ctx->format_params == NULL) {
        return false;
    }
    
    const pmbus_linear16_config_t *config = (const pmbus_linear16_config_t*)ctx->format_params;
    
    /* Calculate the range based on the exponent */
    int32_t max_value, min_value;
    if (config->exponent >= 0) {
        max_value = 32767 << config->exponent;
        min_value = -32768 << config->exponent;
    } else {
        max_value = 32767 >> (-config->exponent);
        min_value = -32768 >> (-config->exponent);
    }
    
    return (value >= min_value && value <= max_value);
}

static int32_t linear16_get_precision(const pmbus_conversion_context_t *ctx) {
    if (ctx == NULL || ctx->format_params == NULL) {
        return 0;
    }
    
    const pmbus_linear16_config_t *config = (const pmbus_linear16_config_t*)ctx->format_params;
    
    /* Precision depends on the exponent - higher exponents have lower precision */
    if (config->exponent >= 0) {
        return (1 << config->exponent);
    } else {
        return 1; /* Best precision when exponent is negative */
    }
}

const pmbus_data_converter_t pmbus_linear16_converter = {
    .format = PMBUS_DATA_FORMAT_LINEAR16,
    .name = "LINEAR16",
    .to_raw = linear16_to_raw,
    .from_raw = linear16_from_raw,
    .validate_range = linear16_validate_range,
    .get_precision = linear16_get_precision,
    .scale_value = NULL,
    .optimize_conversion = NULL,
    .converter_data = NULL
};

/* DIRECT Converter Implementation */
static pmbus_error_t direct_to_raw(int32_t value, const pmbus_conversion_context_t *ctx, 
                                  uint16_t *raw) {
    if (raw == NULL || ctx == NULL || ctx->format_params == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    const pmbus_direct_config_t *config = (const pmbus_direct_config_t*)ctx->format_params;
    return pmbus_convert_int32_to_direct(value, &config->coefficients, raw);
}

static pmbus_error_t direct_from_raw(uint16_t raw, const pmbus_conversion_context_t *ctx,
                                    int32_t *value) {
    if (value == NULL || ctx == NULL || ctx->format_params == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    const pmbus_direct_config_t *config = (const pmbus_direct_config_t*)ctx->format_params;
    return pmbus_convert_direct_to_int32(raw, &config->coefficients, value);
}

static bool direct_validate_range(int32_t value, const pmbus_conversion_context_t *ctx) {
    if (ctx == NULL || ctx->format_params == NULL) {
        return false;
    }
    
    const pmbus_direct_config_t *config = (const pmbus_direct_config_t*)ctx->format_params;
    
    /* Convert to raw and back to check if it's in valid range */
    uint16_t raw;
    pmbus_error_t result = pmbus_convert_int32_to_direct(value, &config->coefficients, &raw);
    if (result != PMBUS_SUCCESS) {
        return false;
    }
    
    /* Check if raw value is within valid range */
    return (raw >= config->min_raw_value && raw <= config->max_raw_value);
}

static int32_t direct_get_precision(const pmbus_conversion_context_t *ctx) {
    if (ctx == NULL || ctx->format_params == NULL) {
        return 0;
    }
    
    const pmbus_direct_config_t *config = (const pmbus_direct_config_t*)ctx->format_params;
    
    /* Precision depends on the m coefficient and R exponent */
    int32_t precision = 1;
    if (config->coefficients.R >= 0) {
        precision = pmbus_power_of_ten(config->coefficients.R);
    }
    precision = precision / abs(config->coefficients.m);
    
    return (precision > 0) ? precision : 1;
}

const pmbus_data_converter_t pmbus_direct_converter = {
    .format = PMBUS_DATA_FORMAT_DIRECT,
    .name = "DIRECT",
    .to_raw = direct_to_raw,
    .from_raw = direct_from_raw,
    .validate_range = direct_validate_range,
    .get_precision = direct_get_precision,
    .scale_value = NULL,
    .optimize_conversion = NULL,
    .converter_data = NULL
};

/* IEEE754 Converter Implementation */
static pmbus_error_t ieee754_to_raw(int32_t value, const pmbus_conversion_context_t *ctx, 
                                   uint16_t *raw) {
    if (raw == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    float float_value = (float)value / 1000.0f; /* Assume milli-units */
    return pmbus_ieee754_float_to_half(float_value, raw);
}

static pmbus_error_t ieee754_from_raw(uint16_t raw, const pmbus_conversion_context_t *ctx,
                                     int32_t *value) {
    if (value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    float float_value;
    pmbus_error_t result = pmbus_ieee754_half_to_float(raw, &float_value);
    if (result == PMBUS_SUCCESS) {
        *value = (int32_t)(float_value * 1000.0f); /* Convert to milli-units */
    }
    return result;
}

static bool ieee754_validate_range(int32_t value, const pmbus_conversion_context_t *ctx) {
    /* IEEE754 half precision can represent a wide range of values */
    const float MAX_HALF_PRECISION = 65504.0f;
    const float MIN_HALF_PRECISION = -65504.0f;
    
    float float_value = (float)value / 1000.0f;
    return (float_value >= MIN_HALF_PRECISION && float_value <= MAX_HALF_PRECISION);
}

static int32_t ieee754_get_precision(const pmbus_conversion_context_t *ctx) {
    /* IEEE754 half precision has about 3-4 decimal digits of precision */
    return 1; /* Best precision */
}

const pmbus_data_converter_t pmbus_ieee754_converter = {
    .format = PMBUS_DATA_FORMAT_IEEE754,
    .name = "IEEE754",
    .to_raw = ieee754_to_raw,
    .from_raw = ieee754_from_raw,
    .validate_range = ieee754_validate_range,
    .get_precision = ieee754_get_precision,
    .scale_value = NULL,
    .optimize_conversion = NULL,
    .converter_data = NULL
};

/* Literal Converter Implementation (pass-through) */
static pmbus_error_t literal_to_raw(int32_t value, const pmbus_conversion_context_t *ctx, 
                                   uint16_t *raw) {
    if (raw == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (value < 0 || value > 65535) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    *raw = (uint16_t)value;
    return PMBUS_SUCCESS;
}

static pmbus_error_t literal_from_raw(uint16_t raw, const pmbus_conversion_context_t *ctx,
                                     int32_t *value) {
    if (value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    *value = (int32_t)raw;
    return PMBUS_SUCCESS;
}

static bool literal_validate_range(int32_t value, const pmbus_conversion_context_t *ctx) {
    return (value >= 0 && value <= 65535);
}

static int32_t literal_get_precision(const pmbus_conversion_context_t *ctx) {
    return 1; /* Perfect precision - no conversion */
}

const pmbus_data_converter_t pmbus_literal_converter = {
    .format = PMBUS_DATA_FORMAT_LITERAL,
    .name = "LITERAL",
    .to_raw = literal_to_raw,
    .from_raw = literal_from_raw,
    .validate_range = literal_validate_range,
    .get_precision = literal_get_precision,
    .scale_value = NULL,
    .optimize_conversion = NULL,
    .converter_data = NULL
};

/* Conversion Manager Implementation */
pmbus_conversion_manager_t* pmbus_conversion_manager_create(void) {
    pmbus_conversion_manager_t *manager = calloc(1, sizeof(pmbus_conversion_manager_t));
    if (manager == NULL) {
        return NULL;
    }
    
    /* Set default configuration */
    manager->enable_caching = true;
    manager->enable_statistics = true;
    manager->cache_timeout_ms = 1000; /* 1 second cache timeout */
    manager->default_flags = PMBUS_CONVERSION_FLAG_ROUND_NEAREST;
    
    /* Initialize statistics */
    manager->statistics.min_conversion_time_us = UINT32_MAX;
    
    return manager;
}

pmbus_error_t pmbus_conversion_manager_destroy(pmbus_conversion_manager_t *manager) {
    if (manager == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    free(manager);
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_conversion_manager_init(pmbus_conversion_manager_t *manager) {
    if (manager == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Register default converters */
    pmbus_register_converter(manager, &pmbus_linear11_converter);
    pmbus_register_converter(manager, &pmbus_linear16_converter);
    pmbus_register_converter(manager, &pmbus_direct_converter);
    pmbus_register_converter(manager, &pmbus_ieee754_converter);
    pmbus_register_converter(manager, &pmbus_literal_converter);
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_register_converter(pmbus_conversion_manager_t *manager,
                                      const pmbus_data_converter_t *converter) {
    if (manager == NULL || converter == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (manager->converter_count >= 16) {
        return PMBUS_ERROR_NO_MEMORY;
    }
    
    /* Check if converter for this format already exists */
    for (size_t i = 0; i < manager->converter_count; i++) {
        if (manager->converters[i]->format == converter->format) {
            return PMBUS_ERROR_INVALID_STATE; /* Already registered */
        }
    }
    
    manager->converters[manager->converter_count++] = (pmbus_data_converter_t*)converter;
    return PMBUS_SUCCESS;
}

const pmbus_data_converter_t* pmbus_get_converter(pmbus_conversion_manager_t *manager,
                                                 pmbus_data_format_t format) {
    if (manager == NULL) {
        return NULL;
    }
    
    for (size_t i = 0; i < manager->converter_count; i++) {
        if (manager->converters[i]->format == format) {
            return manager->converters[i];
        }
    }
    
    return NULL;
}

pmbus_error_t pmbus_convert_value(pmbus_conversion_manager_t *manager,
                                 uint16_t raw_value,
                                 pmbus_data_format_t source_format,
                                 int32_t *converted_value,
                                 const pmbus_conversion_context_t *context) {
    if (manager == NULL || converted_value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    uint32_t start_time = get_timestamp_ms();
    pmbus_error_t result = PMBUS_SUCCESS;
    
    /* Check cache first */
    if (manager->enable_caching) {
        if (lookup_cache(manager, source_format, PMBUS_DATA_FORMAT_LITERAL, 
                        raw_value, converted_value)) {
            manager->statistics.cache_hits++;
            return PMBUS_SUCCESS;
        }
        manager->statistics.cache_misses++;
    }
    
    /* Find appropriate converter */
    const pmbus_data_converter_t *converter = pmbus_get_converter(manager, source_format);
    if (converter == NULL) {
        result = PMBUS_ERROR_UNSUPPORTED;
        goto update_stats;
    }
    
    /* Perform conversion */
    result = converter->from_raw(raw_value, context, converted_value);
    
    /* Cache the result if successful */
    if (result == PMBUS_SUCCESS && manager->enable_caching) {
        cache_conversion(manager, source_format, PMBUS_DATA_FORMAT_LITERAL, 
                        raw_value, *converted_value);
    }
    
update_stats:
    /* Update statistics */
    if (manager->enable_statistics) {
        uint32_t conversion_time = get_timestamp_ms() - start_time;
        manager->statistics.total_conversions++;
        manager->statistics.total_conversion_time_us += conversion_time;
        
        if (conversion_time > manager->statistics.max_conversion_time_us) {
            manager->statistics.max_conversion_time_us = conversion_time;
        }
        if (conversion_time < manager->statistics.min_conversion_time_us) {
            manager->statistics.min_conversion_time_us = conversion_time;
        }
        
        if (result != PMBUS_SUCCESS) {
            manager->statistics.conversion_errors++;
        }
    }
    
    return result;
}

pmbus_error_t pmbus_convert_to_raw(pmbus_conversion_manager_t *manager,
                                  int32_t value,
                                  pmbus_data_format_t target_format,
                                  uint16_t *raw_value,
                                  const pmbus_conversion_context_t *context) {
    if (manager == NULL || raw_value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    uint32_t start_time = get_timestamp_ms();
    pmbus_error_t result = PMBUS_SUCCESS;
    
    /* Find appropriate converter */
    const pmbus_data_converter_t *converter = pmbus_get_converter(manager, target_format);
    if (converter == NULL) {
        result = PMBUS_ERROR_UNSUPPORTED;
        goto update_stats;
    }
    
    /* Perform conversion */
    result = converter->to_raw(value, context, raw_value);
    
update_stats:
    /* Update statistics */
    if (manager->enable_statistics) {
        uint32_t conversion_time = get_timestamp_ms() - start_time;
        manager->statistics.total_conversions++;
        manager->statistics.total_conversion_time_us += conversion_time;
        
        if (conversion_time > manager->statistics.max_conversion_time_us) {
            manager->statistics.max_conversion_time_us = conversion_time;
        }
        if (conversion_time < manager->statistics.min_conversion_time_us) {
            manager->statistics.min_conversion_time_us = conversion_time;
        }
        
        if (result != PMBUS_SUCCESS) {
            manager->statistics.conversion_errors++;
        }
    }
    
    return result;
}

/* Helper Functions */
pmbus_error_t pmbus_linear11_find_optimal_exponent(int32_t value, int8_t *exponent) {
    if (exponent == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    *exponent = 0;
    int32_t temp_value = abs(value);
    
    /* Scale down to fit in 11-bit mantissa (range: -1024 to 1023) */
    while (temp_value > 1023 && *exponent < 15) {
        temp_value >>= 1;
        (*exponent)++;
    }
    
    /* Scale up for better precision if possible */
    while (temp_value <= 511 && *exponent > -16) {
        temp_value <<= 1;
        (*exponent)--;
    }
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_convert_int32_to_linear16(int32_t value, int8_t exponent, uint16_t *raw) {
    if (raw == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    int32_t mantissa;
    
    /* Calculate mantissa based on exponent */
    if (exponent >= 0) {
        mantissa = value >> exponent;
    } else {
        mantissa = value << (-exponent);
    }
    
    /* Check if mantissa fits in 16 bits */
    if (mantissa > 32767 || mantissa < -32768) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    *raw = (uint16_t)mantissa;
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_convert_int32_to_direct(int32_t value,
                                           const pmbus_direct_coefficients_t *coeff,
                                           uint16_t *raw) {
    if (coeff == NULL || raw == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Calculate: raw = (value * 10^R + b) / m */
    int64_t temp = value;
    
    /* Apply R exponent */
    if (coeff->R >= 0) {
        temp = temp * pmbus_power_of_ten(coeff->R);
    } else {
        temp = temp / pmbus_power_of_ten(-coeff->R);
    }
    
    /* Add offset and divide by slope */
    temp = (temp + coeff->b) / coeff->m;
    
    /* Check range */
    if (temp < 0 || temp > 65535) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    *raw = (uint16_t)temp;
    return PMBUS_SUCCESS;
}

/* Mathematical utility functions */
int32_t pmbus_power_of_two(int8_t exponent) {
    if (exponent >= 0) {
        return 1 << exponent;
    } else {
        return 1; /* For negative exponents, return 1 (division handled elsewhere) */
    }
}

int32_t pmbus_power_of_ten(int8_t exponent) {
    static const int32_t powers_of_ten[] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
    };
    
    if (exponent >= 0 && exponent < 10) {
        return powers_of_ten[exponent];
    } else if (exponent >= 10) {
        return 1000000000; /* Cap at 10^9 */
    } else {
        return 1; /* For negative exponents, return 1 (division handled elsewhere) */
    }
}

int32_t pmbus_clamp_value(int32_t value, int32_t min_val, int32_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/* IEEE754 Helper Functions */
pmbus_error_t pmbus_ieee754_float_to_half(float value, uint16_t *half) {
    if (half == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Simple implementation - not handling all IEEE754 edge cases */
    union { float f; uint32_t i; } conv;
    conv.f = value;
    
    uint32_t sign = (conv.i >> 31) & 0x1;
    uint32_t exponent = (conv.i >> 23) & 0xFF;
    uint32_t mantissa = conv.i & 0x7FFFFF;
    
    /* Convert to half precision */
    if (exponent == 0) {
        /* Zero or denormal */
        *half = (uint16_t)(sign << 15);
    } else if (exponent == 0xFF) {
        /* Infinity or NaN */
        *half = (uint16_t)((sign << 15) | 0x7C00 | (mantissa ? 0x200 : 0));
    } else {
        /* Normal number */
        int32_t half_exp = (int32_t)exponent - 127 + 15;
        if (half_exp <= 0) {
            /* Underflow to zero */
            *half = (uint16_t)(sign << 15);
        } else if (half_exp >= 31) {
            /* Overflow to infinity */
            *half = (uint16_t)((sign << 15) | 0x7C00);
        } else {
            /* Normal conversion */
            uint32_t half_mantissa = mantissa >> 13;
            *half = (uint16_t)((sign << 15) | (half_exp << 10) | half_mantissa);
        }
    }
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_ieee754_half_to_float(uint16_t half, float *value) {
    if (value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    uint32_t sign = (half >> 15) & 0x1;
    uint32_t exponent = (half >> 10) & 0x1F;
    uint32_t mantissa = half & 0x3FF;
    
    union { float f; uint32_t i; } conv;
    
    if (exponent == 0) {
        if (mantissa == 0) {
            /* Zero */
            conv.i = sign << 31;
        } else {
            /* Denormal - convert to normal float */
            uint32_t float_exp = 127 - 15 + 1;
            while ((mantissa & 0x400) == 0) {
                mantissa <<= 1;
                float_exp--;
            }
            mantissa &= 0x3FF;
            conv.i = (sign << 31) | (float_exp << 23) | (mantissa << 13);
        }
    } else if (exponent == 0x1F) {
        /* Infinity or NaN */
        conv.i = (sign << 31) | 0x7F800000 | (mantissa << 13);
    } else {
        /* Normal number */
        uint32_t float_exp = exponent - 15 + 127;
        conv.i = (sign << 31) | (float_exp << 23) | (mantissa << 13);
    }
    
    *value = conv.f;
    return PMBUS_SUCCESS;
}

/* Internal helper functions */
static uint32_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static bool lookup_cache(pmbus_conversion_manager_t *manager,
                        pmbus_data_format_t source_format,
                        pmbus_data_format_t target_format,
                        uint16_t raw_value,
                        int32_t *converted_value) {
    uint32_t current_time = get_timestamp_ms();
    
    for (size_t i = 0; i < 64; i++) {
        if (manager->cache[i].valid &&
            manager->cache[i].source_format == source_format &&
            manager->cache[i].target_format == target_format &&
            manager->cache[i].raw_value == raw_value &&
            (current_time - manager->cache[i].timestamp) < manager->cache_timeout_ms) {
            *converted_value = manager->cache[i].converted_value;
            return true;
        }
    }
    
    return false;
}

static pmbus_error_t cache_conversion(pmbus_conversion_manager_t *manager,
                                     pmbus_data_format_t source_format,
                                     pmbus_data_format_t target_format,
                                     uint16_t raw_value,
                                     int32_t converted_value) {
    size_t index = manager->cache_index % 64;
    
    manager->cache[index].source_format = source_format;
    manager->cache[index].target_format = target_format;
    manager->cache[index].raw_value = raw_value;
    manager->cache[index].converted_value = converted_value;
    manager->cache[index].timestamp = get_timestamp_ms();
    manager->cache[index].valid = true;
    
    manager->cache_index++;
    
    return PMBUS_SUCCESS;
}
