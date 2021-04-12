# A function generator based in DDS, the AD9833

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/zaaheF0XjLw/0.jpg)](https://www.youtube.com/watch?v=zaaheF0XjLw )


## Core features

- +/- 10V 
- linear power supply, builtin
- Sweep function
- Random function
- Update OTA feature
- MQTT status and settings

## Credits

The whole stuff is based on the work by Youtuber GreatScott, whos instructions you can find here: https://www.instructables.com/DIY-FunctionWaveform-Generator/

Voice in Video by https://www.text2speech.org/

## BOM
### power circuit

- Transformer 230V to 2x 15V
- 4 diodes 1n4007
- 2 2200uF caps, 35V
- 2 330uF caps, 35V
- C14 socket with fuse and switch
- some wires to connect C14 with transformer
- perfboard to mount the power circuit onto

### logic curciut

- 10u Capacitor
- 0.33u Capacitor
- 0.33u Capacitor
- 2.2u Capacitor
- 22u Capacitor
- 0.1u Capacitor
- 0.1u Capacitor
- 1u Capacitor
- 4x 100n Capacitor_SMD:C_0805
- LED 5mm
- Fuse 2A PTC
- 3x Screw Terminal Block
- GY9833 / AD9833 breakout board 3.3V
- 270 Ohms Resistor SMD: 0805
- 10k Trim Poti
- 10k Poti
- 50k Poti 
- AMS1117-3.3
- L7805
- L7812
- L7912
- ESP32 30 pin devboard
- Rotary Encoder
- OLED 0.96" display
- pinheaders male and female, dupont wires, stuff to wire power circuit and logic circuit together
- optional: MCP100-300D Voltage supervisor

### Case

- printed STLs from folder
- 10 screws M3x20 or similar
- 3 screws M3x12 or similar
- 1 screw M3x16 or similar
- 4 screws M4x20 or similar, 4 nuts of the same size

## How to build the case

Mount Back, bottom, front and topA to left, using M3x20 screws. Mount topB to topA by using M3x20 screws. Mount right to this whole assembly using M3x20 screws. Basically, this is the case for our Function Generator.

The lever is held to the front by an M3x16 screw, and secures the OLED.

## Logic PCB

I was creating a single sided PCB, so everyone with equipment should be able to create the PCB as well. However, I do not recommend using my board design. I had some parts laying around, and was recycling them by mixing SMD and THT with no bad feelings. 

## License

https://creativecommons.org/licenses/by-nc-sa/4.0/

Most obvious limits of this license:

- You are not allowed to use this stuff for commercial purposes
- You can download, reupload and change the stuff, but have to release it under the same license again
- You have the mention the origin, like "Original by Peter Stimpel, https://github.com/pstimpel/functiongeneratordds"
