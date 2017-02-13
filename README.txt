README

MSP432 Network Alarm Clock

Files:
main.c
wifi_chip.lua
README.txt

This low-power usage alarm clock uses the MSP432P401R microcontroller, BOOSTXL-EDUMKII BoosterPack, and the Adafruit Huzzah ESP8266 Wi-Fi microcontroller.
main.c is run on the MSP432 (attached to the BoosterPack) and wifi_chip.lua is run on the ESP8266.

Peripherals on the MSP432 and BoosterPack include the LCD to display the time, the joystick to set the alarm time,
the buttons to switch between modes, and the buzzer to output the alarm sound.
Every two seconds, the ESP8266 initiates a TCP connection to 198.111.152.100
on port 37 to request the time using the TIME protocol (RFC 868).
The ESP8266 then transmits the 32-bit integer representing the number of seconds since 1900 over UART to the MSP432.

To make our alarm clock more power efficient, we turn on/off certain interrupt handlers (ADC, WDT) when there is no use for them.

The main.c file is broken down into the main function, GPIO interrupt handlers for the buttons,
the ADC interrupt handler for the joystick, the UART interrupt handler for receiving data,
the WDT interrupt handler to play the alarm sound, and functions to initialize and map TimerA to the buzzer
and display the time to the LCD. The default state of the system is to display the current time.
Every two seconds, the ESP8266 sends a 32-bit integer, using UART, to the MSP432.
After the integer has been fully received, the MSP432 converts the time received from the “time since 1900” to the “time since 1970”
by subtracting 2208988800 seconds. It then uses time.h (the standard c time library)
to convert this time into a tm struct used for printing the time in a human readable form.
The ADC interrupt is disabled by default and is only enabled when a button is pressed sending the system into alarm set mode.
When this occurs, the alarm time is displayed instead of the current time and the joystick can be used to set the alarm time.
Moving the joystick up and down moves the selected position (hour, minute, am/pm) up and down, respectively.
Moving the joystick to the right changes the selected position.
When the second button is pushed, the ADC handler is disabled and the screen goes back to displaying the current time supplied by the ESP8266.
In the event that the alarm time and current time become equal, the WDT handler is turned on,
TimerA’s output is turned from off to upmode, and the alarm sounds plays until a button is pushed.
The alarm sound is driven by production of a square wave whose frequency is determined by the value in TimerA’s control register.
