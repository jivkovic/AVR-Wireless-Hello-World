AVR and RF communication test (Atmega8 and nRF24L01+)
==============

**The most basic AVR Wireless hello world working code.**

You need two AVRs and 2 nRFs. Compile reciever (RX) and transmitter (TX) using any popular compiler (I used Atmel Studio 6.1), burn them on chips and wire them up as shown [here](http://gizmosnack.blogspot.com/2013/04/tutorial-nrf24l01-and-avr.html).

![alt tag](http://www.electrokit.com/public/upload/productimage/49981-10364-4.jpg)

**How does it work**

The transmitter sends packets looped, and reciever will blink LED upon recieve in listen loop. Interrupts and optimization excluded. Just simple procedural code you can use to kickstart your own code / project. For any additional questions or troubleshouting, check blog by [Kalle Löfgren](http://gizmosnack.blogspot.com/2013/04/tutorial-nrf24l01-and-avr.html).

If it doesn't work, check your power supply regulators and wiring.
