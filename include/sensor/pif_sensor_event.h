// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_sensor_event.h
 * @brief Events shared by the barometer and sonar drivers.
 */
#ifndef PIF_SENSOR_EVENT_H
#define PIF_SENSOR_EVENT_H


/**
 * @brief Baro Sensor
 * @param pressure unit : hPa
 * @param temperature unit : degrees C
 */
typedef void (*PifEvtBaroRead)(float pressure, float temperature);

/**
 * @brief Sonar Sensor
 * @param distance unit : cm
 */
typedef void (*PifEvtSonarRead)(int32_t distance);


#endif  // PIF_SENSOR_EVENT_H
