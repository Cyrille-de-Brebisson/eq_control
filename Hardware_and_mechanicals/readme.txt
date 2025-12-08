box esp32 files is a drawing for the "box" for the controler.
It is desinged to be laser cut out of 2mm thick wood or plastic.

eq3_5 dec.dxf is the declinaison motor holder for both EQ5 and EQ3. It is desinged to be laser cut out of 1.5mm thick metal. There is one bend to do, an "assembly" and a weld. Note that it is made as "one part" which will need to be separated in 2 at a tab as it reduces the costs for me here..

eq5ra.dxf is the eq5 ra motor holder. It also needs a "bend" and is also designed to be laser cut out of 1.5mm steel. Note that you will need to dril and tap 2 M4 holes in your mount to use it.

eq3_ra.svg is the EQ3 ra holder. Here again a bend and 3 peices to separate and place together.


PCB, schematics and Gerbers are the pure HW files.


About the HW design.

An esp32C3 super mini is the core.

The 9 key keyboard is read by the CPU using ADC and a resistance ladder due to the IO limitation.

The same ADC is used to read external power level using a divider bridge.

5V to ESP is provided using a DC-DC board for high power efficiency (regulator can drop significant power, especially if dropping from 20V to 5V).

3 TMC2209 stepper drivers are used to drive the motors. They are driven using the step pin and serial communication.

A 0.96 I²C OLED 128*32 LCD does the display (prefer blue, less agressive for the eyes)

Connection to the motors is done using USB2 connectors cause they are cheap and good connectors.
It is also easy to find USB cables, cut the device side and repurpose them for motor connection.

A 5.5x2.1 barrel connector is used for external power. Use a 12-15V, 1A power source or a 15V USB-C to barrel adaptor for power.

I usually place the ESP and drivers on low profile "carriages" to make them replacable in case of...

Appart from the drivers which are relatively expensive and for which you need 2 or 3, all the rest is dirt cheap. And the PCB will come as a 5 pack. So I advise that you take enough material to build at least 2 in case of!



Parts (all sourced on ali express)
Esp32C3 super mini board (<3 euros): https://fr.aliexpress.com/item/1005009776606859.html
9 6x6 low profile push buttons, SMT (<1 euro, usually by 20 packs): https://fr.aliexpress.com/item/1005003163947039.html
3 TMC2209 (~2.6 euros per driver): https://fr.aliexpress.com/item/1005009299841291.html
0.96 OLED 128*32 LCD (<2 euros): https://fr.aliexpress.com/item/1005008918700196.html
10k, 1.5k, 750R, 68K and 5.1K 0805 resistances will be needed (but yhou could probably get by without the 68K and 5.1K ones using 10k and 1.5k instead for the power divider bridge).: https://fr.aliexpress.com/item/1005004765509774.html
3x 100µF capacitors: https://fr.aliexpress.com/item/1005004506467517.html
header: Use low profiles ones! https://fr.aliexpress.com/item/1005004122312694.html
USB connectors (1.5euros by 10): https://fr.aliexpress.com/item/4001141222647.html
5.5 2.1 DC connecteor PCB: https://fr.aliexpress.com/item/32839712664.html
