#ifndef PIF_SENSOR_EVENT_H
#define PIF_SENSOR_EVENT_H


/**
 * @fn PifEvtBaroRead
 * @brief Baro Sensor
 * @param pressure unit : hPa
 * @param temperature unit : degrees C
 */
typedef void (*PifEvtBaroRead)(float pressure, float temperature);

/**
 * @fn PifEvtSonarRead
 * @brief Sonar Sensor
 * @param distance unit : cm
 */
typedef void (*PifEvtSonarRead)(int32_t distance);


#endif  // PIF_SENSOR_EVENT_H
