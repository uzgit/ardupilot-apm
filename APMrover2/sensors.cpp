// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "Rover.h"

void Rover::init_barometer(void)
{
    gcs_send_text_P(SEVERITY_LOW, PSTR("Calibrating barometer"));    
    barometer.calibrate();
    gcs_send_text_P(SEVERITY_LOW, PSTR("barometer calibration complete"));
}

void Rover::init_sonar(void)
{
    sonar.init();
}

// read_battery - reads battery voltage and current and invokes failsafe
// should be called at 10hz
void Rover::read_battery(void)
{
    battery.read();
}

// read the receiver RSSI as an 8 bit number for MAVLink
// RC_CHANNELS_SCALED message
void Rover::read_receiver_rssi(void)
{
    rssi_analog_source->set_pin(g.rssi_pin);
    float ret = rssi_analog_source->voltage_average() * 50;
    receiver_rssi = constrain_int16(ret, 0, 255);
}

// read the sonars
void Rover::read_sonars(void)
{
    sonar.update();

    if (sonar.status() == RangeFinder::RangeFinder_NotConnected) {
        // this makes it possible to disable sonar at runtime
        return;
    }

    if (sonar.has_data(1)) {
        // we have two sonars
        obstacle.sonar1_distance_cm = sonar.distance_cm(0);
        obstacle.sonar2_distance_cm = sonar.distance_cm(1);
        if (obstacle.sonar1_distance_cm <= (uint16_t)g.sonar_trigger_cm &&
            obstacle.sonar1_distance_cm <= (uint16_t)obstacle.sonar2_distance_cm)  {
            // we have an object on the left
            if (obstacle.detected_count < 127) {
                obstacle.detected_count++;
            }
            if (obstacle.detected_count == g.sonar_debounce) {
                gcs_send_text_fmt(PSTR("Sonar1 obstacle %u cm"),
                                  (unsigned)obstacle.sonar1_distance_cm);
            }
            obstacle.detected_time_ms = hal.scheduler->millis();
            obstacle.turn_angle = g.sonar_turn_angle;
        } else if (obstacle.sonar2_distance_cm <= (uint16_t)g.sonar_trigger_cm) {
            // we have an object on the right
            if (obstacle.detected_count < 127) {
                obstacle.detected_count++;
            }
            if (obstacle.detected_count == g.sonar_debounce) {
                gcs_send_text_fmt(PSTR("Sonar2 obstacle %u cm"),
                                  (unsigned)obstacle.sonar2_distance_cm);
            }
            obstacle.detected_time_ms = hal.scheduler->millis();
            obstacle.turn_angle = -g.sonar_turn_angle;
        }
    } else {
        // we have a single sonar
        obstacle.sonar1_distance_cm = sonar.distance_cm(0);
        obstacle.sonar2_distance_cm = 0;
        if (obstacle.sonar1_distance_cm <= (uint16_t)g.sonar_trigger_cm)  {
            // obstacle detected in front 
            if (obstacle.detected_count < 127) {
                obstacle.detected_count++;
            }
            if (obstacle.detected_count == g.sonar_debounce) {
                gcs_send_text_fmt(PSTR("Sonar obstacle %u cm"),
                                  (unsigned)obstacle.sonar1_distance_cm);
            }
            obstacle.detected_time_ms = hal.scheduler->millis();
            obstacle.turn_angle = g.sonar_turn_angle;
        }
    }

    Log_Write_Sonar();

    // no object detected - reset after the turn time
    if (obstacle.detected_count >= g.sonar_debounce &&
        hal.scheduler->millis() > obstacle.detected_time_ms + g.sonar_turn_time*1000) { 
        gcs_send_text_fmt(PSTR("Obstacle passed"));
        obstacle.detected_count = 0;
        obstacle.turn_angle = 0;
    }
}

void Rover::read_external_data(void)
{
    uint8_t * local_buffer;
    local_buffer = new uint8_t[19];

//    virtual uint8_t read(uint8_t addr, uint8_t len, uint8_t* data) = 0;

//    AP_HAL::OwnPtr<AP_HAL::I2CDevice> arduino = hal.i2c_mgr->get_device(1, 7);
//    if( arduino->transfer(nullptr, 0, local_buffer, i2c_buffer_length) )
    if( hal.i2c->read(7, i2c_buffer_length, local_buffer) )
    {
        for(uint32_t i = 0; i < i2c_buffer_length; i ++)
            i2c_buffer[i] = local_buffer[i];
        new_data_received = true;
    }

    delete local_buffer;
}

