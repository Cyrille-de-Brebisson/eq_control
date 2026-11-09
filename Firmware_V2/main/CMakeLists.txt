idf_component_register(SRCS "main.cpp" "asteroids.cpp" "bno.cpp" "scope.cpp" "system.cpp" "ui.cpp"
                    "libsgp4/CoordGeodetic.cpp" "libsgp4/CoordTopocentric.cpp" "libsgp4/CsvTleLoader.cpp" "libsgp4/DateTime.cpp" "libsgp4/DecayedException.cpp" 
                    "libsgp4/Eci.cpp" "libsgp4/Globals.cpp" "libsgp4/Observer.cpp" "libsgp4/OrbitalElements.cpp" "libsgp4/SatelliteException.cpp" 
                    "libsgp4/SGP4.cpp" "libsgp4/SolarPosition.cpp" "libsgp4/TimeSpan.cpp" "libsgp4/Tle.cpp" "libsgp4/TleException.cpp" "libsgp4/Util.cpp" 
                    "libsgp4/Vector.cpp"
                    INCLUDE_DIRS "." 
                    REQUIRES ble_uart esp_adc esp_driver_gptimer esp_driver_usb_serial_jtag esp_driver_uart esp_driver_i2c)