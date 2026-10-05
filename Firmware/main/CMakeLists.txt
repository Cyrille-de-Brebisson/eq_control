idf_component_register(SRCS "main.cpp" "Alpaca.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES ble_uart esp_adc esp_driver_gptimer esp_driver_usb_serial_jtag esp_driver_uart esp_driver_i2c)