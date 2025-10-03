#ifndef PMBUS_DATA_CONVERSION_H
#define PMBUS_DATA_CONVERSION_H

#include "pmbus/protocol.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pmbus_data_conversion.h
 * @brief PMBus Data Conversion Framework
 * 
 * This module provides a pluggable data conversion system for PMBus,
 * supporting all standard data formats and enabling custom formats.
 */

/* Forward declarations */
typedef struct pmbus_data_converter pmbus_data_converter_t;
typedef struct pmbus_conversion_manager pmbus_conversion_manager_t;

/* Conversion flags */
typedef enum {
    PMBUS_CONVERSION_FLAG_NONE = 0x00,
    PMBUS_CONVERSION_FLAG_ROUND_NEAREST = 0x01,
    PMBUS_CONVERSION_FLAG_CLAMP_TO_RANGE = 0x02,
    PMBUS_CONVERSION_FLAG_PRESERVE_PRECISION = 0x04,
    PMBUS_CONVERSION_FLAG_FAST_CONVERSION = 0x08
} pmbus_conversion_flags_t;

/* Conversion context */
typedef struct {
    pmbus_data_format_t source_format;
    pmbus_data_format_t target_format;
    pmbus_conversion_flags_t flags;
    void *format_params;
    int32_t scale_factor;
    int32_t offset;
    int32_t min_value;
    int32_t max_value;
    uint32_t precision;
} pmbus_conversion_context_t;

/* Conversion result */
typedef struct {
    pmbus_error_t result;
    int32_t converted_value;
    uint32_t precision_loss;
    bool clamped;
    bool rounded;
    uint32_t conversion_time_us;
} pmbus_conversion_result_t;

/* Data converter interface */
struct pmbus_data_converter {
    pmbus_data_format_t format;
    const char *name;
    
    /* Conversion functions */
    pmbus_error_t (*to_raw)(int32_t value, const pmbus_conversion_context_t *ctx, 
                           uint16_t *raw);
    pmbus_error_t (*from_raw)(uint16_t raw, const pmbus_conversion_context_t *ctx,
                             int32_t *value);
    
    /* Validation functions */
    bool (*validate_range)(int32_t value, const pmbus_conversion_context_t *ctx);
    int32_t (*get_precision)(const pmbus_conversion_context_t *ctx);
    
    /* Utility functions */
    pmbus_error_t (*scale_value)(int32_t value, int32_t from_scale, 
                                int32_t to_scale, int32_t *result);
    pmbus_error_t (*optimize_conversion)(const pmbus_conversion_context_t *ctx);
    
    /* Converter-specific data */
    void *converter_data;
};

/* Specific converter implementations */

/* LINEAR11 converter */
typedef struct {
    int8_t preferred_exponent;
    bool auto_optimize_exponent;
    int8_t min_exponent;
    int8_t max_exponent;
    bool preserve_sign_bit;
} pmbus_linear11_config_t;

extern const pmbus_data_converter_t pmbus_linear11_converter;

/* LINEAR16 converter */
typedef struct {
    int8_t exponent;
    bool validate_exponent_range;
    int8_t min_exponent;
    int8_t max_exponent;
} pmbus_linear16_config_t;

extern const pmbus_data_converter_t pmbus_linear16_converter;

/* DIRECT converter */
typedef struct {
    pmbus_direct_coefficients_t coefficients;
    bool validate_coefficients;
    int32_t min_raw_value;
    int32_t max_raw_value;
} pmbus_direct_config_t;

extern const pmbus_data_converter_t pmbus_direct_converter;

/* IEEE754 converter */
typedef struct {
    bool half_precision;
    bool denormal_support;
    bool infinity_support;
    bool nan_support;
} pmbus_ieee754_config_t;

extern const pmbus_data_converter_t pmbus_ieee754_converter;

/* VID converter */
typedef struct {
    uint8_t vid_table_version;
    bool intel_vid;
    bool amd_vid;
    uint16_t vcc_max_mv;
    uint16_t vcc_min_mv;
} pmbus_vid_config_t;

extern const pmbus_data_converter_t pmbus_vid_converter;

/* Literal/Raw data converter */
extern const pmbus_data_converter_t pmbus_literal_converter;

/* Conversion manager */
struct pmbus_conversion_manager {
    pmbus_data_converter_t *converters[16]; /* Max 16 different formats */
    size_t converter_count;
    
    /* Conversion cache */
    struct {
        pmbus_data_format_t source_format;
        pmbus_data_format_t target_format;
        uint16_t raw_value;
        int32_t converted_value;
        uint32_t timestamp;
        bool valid;
    } cache[64]; /* Cache for recent conversions */
    size_t cache_index;
    
    /* Statistics */
    struct {
        uint64_t total_conversions;
        uint64_t cache_hits;
        uint64_t cache_misses;
        uint64_t conversion_errors;
        uint32_t total_conversion_time_us;
        uint32_t max_conversion_time_us;
        uint32_t min_conversion_time_us;
    } statistics;
    
    /* Configuration */
    bool enable_caching;
    bool enable_statistics;
    uint32_t cache_timeout_ms;
    pmbus_conversion_flags_t default_flags;
};

/* Conversion manager functions */
pmbus_conversion_manager_t* pmbus_conversion_manager_create(void);
pmbus_error_t pmbus_conversion_manager_destroy(pmbus_conversion_manager_t *manager);
pmbus_error_t pmbus_conversion_manager_init(pmbus_conversion_manager_t *manager);

/* Converter registration */
pmbus_error_t pmbus_register_converter(pmbus_conversion_manager_t *manager,
                                      const pmbus_data_converter_t *converter);
pmbus_error_t pmbus_unregister_converter(pmbus_conversion_manager_t *manager,
                                        pmbus_data_format_t format);
const pmbus_data_converter_t* pmbus_get_converter(pmbus_conversion_manager_t *manager,
                                                 pmbus_data_format_t format);

/* High-level conversion functions */
pmbus_error_t pmbus_convert_value(pmbus_conversion_manager_t *manager,
                                 uint16_t raw_value,
                                 pmbus_data_format_t source_format,
                                 int32_t *converted_value,
                                 const pmbus_conversion_context_t *context);

pmbus_error_t pmbus_convert_to_raw(pmbus_conversion_manager_t *manager,
                                  int32_t value,
                                  pmbus_data_format_t target_format,
                                  uint16_t *raw_value,
                                  const pmbus_conversion_context_t *context);

pmbus_error_t pmbus_convert_between_formats(pmbus_conversion_manager_t *manager,
                                           uint16_t source_raw,
                                           pmbus_data_format_t source_format,
                                           uint16_t *target_raw,
                                           pmbus_data_format_t target_format,
                                           const pmbus_conversion_context_t *context);

/* Batch conversion functions */
pmbus_error_t pmbus_batch_convert_values(pmbus_conversion_manager_t *manager,
                                        const uint16_t *raw_values,
                                        size_t count,
                                        pmbus_data_format_t source_format,
                                        int32_t *converted_values,
                                        const pmbus_conversion_context_t *context);

pmbus_error_t pmbus_batch_convert_to_raw(pmbus_conversion_manager_t *manager,
                                        const int32_t *values,
                                        size_t count,
                                        pmbus_data_format_t target_format,
                                        uint16_t *raw_values,
                                        const pmbus_conversion_context_t *context);

/* Validation functions */
pmbus_error_t pmbus_validate_conversion(pmbus_conversion_manager_t *manager,
                                       int32_t value,
                                       pmbus_data_format_t format,
                                       const pmbus_conversion_context_t *context);

pmbus_error_t pmbus_get_conversion_error(pmbus_conversion_manager_t *manager,
                                        pmbus_data_format_t from_format,
                                        pmbus_data_format_t to_format,
                                        int32_t *max_error);

/* Utility functions */
pmbus_error_t pmbus_create_conversion_context(pmbus_conversion_context_t *context,
                                             pmbus_data_format_t source_format,
                                             pmbus_data_format_t target_format,
                                             pmbus_conversion_flags_t flags);

pmbus_error_t pmbus_optimize_conversion_path(pmbus_conversion_manager_t *manager,
                                            pmbus_data_format_t source_format,
                                            pmbus_data_format_t target_format,
                                            pmbus_conversion_context_t *optimized_context);

/* Cache management */
pmbus_error_t pmbus_clear_conversion_cache(pmbus_conversion_manager_t *manager);
pmbus_error_t pmbus_get_cache_statistics(pmbus_conversion_manager_t *manager,
                                        uint64_t *hits, uint64_t *misses, 
                                        double *hit_ratio);

/* Statistics */
pmbus_error_t pmbus_get_conversion_statistics(pmbus_conversion_manager_t *manager,
                                             uint64_t *total_conversions,
                                             uint64_t *errors,
                                             uint32_t *avg_time_us);
pmbus_error_t pmbus_reset_conversion_statistics(pmbus_conversion_manager_t *manager);

/* Format-specific helper functions */

/* LINEAR11 helpers */
pmbus_error_t pmbus_linear11_find_optimal_exponent(int32_t value, int8_t *exponent);
pmbus_error_t pmbus_linear11_pack(int16_t mantissa, int8_t exponent, uint16_t *raw);
pmbus_error_t pmbus_linear11_unpack(uint16_t raw, int16_t *mantissa, int8_t *exponent);

/* LINEAR16 helpers */
pmbus_error_t pmbus_linear16_convert_with_exponent(int32_t value, int8_t exponent, uint16_t *raw);
pmbus_error_t pmbus_linear16_extract_with_exponent(uint16_t raw, int8_t exponent, int32_t *value);

/* DIRECT helpers */
pmbus_error_t pmbus_direct_calculate_coefficients(int32_t real_value_1, uint16_t raw_value_1,
                                                 int32_t real_value_2, uint16_t raw_value_2,
                                                 pmbus_direct_coefficients_t *coefficients);
pmbus_error_t pmbus_direct_validate_coefficients(const pmbus_direct_coefficients_t *coefficients);

/* IEEE754 helpers */
pmbus_error_t pmbus_ieee754_float_to_half(float value, uint16_t *half);
pmbus_error_t pmbus_ieee754_half_to_float(uint16_t half, float *value);
pmbus_error_t pmbus_ieee754_check_special_values(uint16_t half, bool *is_special);

/* VID helpers */
pmbus_error_t pmbus_vid_to_voltage(uint8_t vid_code, uint8_t vid_table_version, 
                                  uint16_t *voltage_mv);
pmbus_error_t pmbus_voltage_to_vid(uint16_t voltage_mv, uint8_t vid_table_version,
                                  uint8_t *vid_code);

/* Mathematical utility functions */
int32_t pmbus_power_of_two(int8_t exponent);
int32_t pmbus_power_of_ten(int8_t exponent);
int32_t pmbus_round_to_nearest(int64_t value);
int32_t pmbus_clamp_value(int32_t value, int32_t min_val, int32_t max_val);
uint32_t pmbus_calculate_precision_loss(int32_t original, int32_t converted);

/* Error reporting */
const char* pmbus_conversion_error_to_string(pmbus_error_t error);
pmbus_error_t pmbus_get_last_conversion_error(pmbus_conversion_manager_t *manager);

#ifdef __cplusplus
}
#endif

#endif /* PMBUS_DATA_CONVERSION_H */
