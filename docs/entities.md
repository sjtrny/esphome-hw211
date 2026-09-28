# HW211 entity guide

Names are presentation metadata from `components/hw211/entity_metadata.py`.
The original workbook and `protocol/hw211_modbus.json` remain unchanged.
Registers use the address in the selected sheet; do not interchange telemetry
addresses between `dtu_wifi` and `hw211`.

## Interpreting the names

- **Operating state** is a derived, read-only activity summary, separate from
  the selected operating mode. See the [input registers and decision rules](../README.md#derived-operating-state).
- **Water temperature** is the controller's app/display temperature. It is
  separate from the lower and upper tank probes.
- **Electric heater** is the resistive booster. **Expansion valve** is the EEV.
- **Defrost interval** is the interval/cycle parameter, not the duration of an
  active defrost. Maximum and minimum defrost duration are separate settings.
- **Reheat differential** is the restart temperature difference (hysteresis),
  not another target temperature.
- **Clock ... setting** and **Clock update flag** are the clock-setting
  registers. A zero readback does not establish the controller's actual time.
  The component does not synchronize the controller clock.
- **Component status flags**, **Operating status flags**, and **Fault flags**
  are bitmasks, not counts. `create_binary_sensors` exposes the documented bits
  when the selected sheet marks the register as binary. Input names state their
  polarity; no bits are inverted by these naming changes.
- **Thermostat satisfied** means thermostat shutdown, not mains power off.
  **Hot water mode active** is a function flag, not proof that the compressor
  is running. Use **Compressor running** for the compressor output state.
- **Output O05/O06 active** keeps the output number because these outputs have
  configurable functions. A label must not assume a fan or solar pump is fitted.
- **Solar** names describe the controller's solar inputs/settings; their
  presence does not establish that a solar system is connected.
- Compressor high-temperature cutoff labels deliberately do not identify an
  ambient/tank sensor: the two source sheets differ on this point.
- The `hw211` sheet repeats the main-firmware label for its second firmware pair;
  those fields are called **Secondary controller firmware** rather than assuming
  which physical controller they identify.

## Default Home Assistant sections

Everyday operating readings use the Sensors card. Everyday writable controls use
Controls. Timers and writable engineering settings use Configuration. Read-only
engineering settings, firmware, counters, raw registers, fault flags and input
signals use Diagnostics. Functional prefixes keep related fields together in
alphabetical lists. These are component defaults, not local HA customizations.

The following read-only sensors use Diagnostics where present in the selected
register sheet:

- Electric heater independent operation
- Electric heater target temperature
- Operating mode
- Operating mode active
- Power
- Refrigerant suction temperature
- Solar sensor temperature
- Ventilation mode

Optional writable controls for these settings retain their existing categories.
The derived Operating state sensor remains in Sensors.

Entity creation flags and disabled-by-default behavior are unchanged. Raw
register sensors keep the `Raw` suffix and remain disabled by default. Unknown
reserved registers keep an address-based name. ESPHome IDs and API keys retain
their original names internally so existing Home Assistant entities can migrate.
User-defined HA names remain in effect until cleared.

If an existing installation shows the new names in the old sections, reload
that device's ESPHome integration entry to refresh Home Assistant's cached
categories. This does not require local category overrides.

The exception to legacy key retention is the HW211 sheet's optional raw timer
fields (1134–1141). Their old Chinese names collapsed to duplicate ASCII IDs and
prevented raw configurations from compiling. They now use distinct,
address-qualified API keys; there were no valid separate keys to retain.

The DTU sheet's defrost interval and heater delay had copied temperature units;
these are minutes. Expansion-valve positions are steps and accumulated run time
is hours. These are display-unit corrections, not changes to numeric scaling.
The HW211 sheet's missing scaling metadata is not inferred from the DTU sheet.

## Register names

Some registers are exposed only with the corresponding creation flags; this
reference is not a list of entities enabled in every configuration.

### `dtu_wifi`

| Register | Workbook name | Display name |
| --- | --- | --- |
| 1011 | Power on/off | Power |
| 1012 | Mode | Operating mode |
| 1013 | Actual operation mode | Operating mode active |
| 1014 | Booster works independently | Electric heater independent operation |
| 1015 | Compulsive defrosting | Defrost forced operation |
| 1016 | Ventilation mode | Ventilation mode |
| 1020 | Usage of O05 | Output O05 function |
| 1021 | Usage of O06 | Output O06 function |
| 1034 | Defrosting startup temp | Defrost start temperature |
| 1035 | Defrosting shutdown temp | Defrost stop temperature |
| 1036 | Duration of defrosting | Defrost interval |
| 1037 | Longest duration of defrosting | Defrost maximum duration |
| 1038 | Shortest duration of defrosting | Defrost minimum duration |
| 1039 | Defrosting way | Defrost mode |
| 1040 | Intelligent defrosting judgement | Defrost smart mode temperature threshold |
| 1046 | Disinfection target temp | Disinfection target temperature |
| 1047 | Duration of disinfection | Disinfection hold time |
| 1048 | Startup point of disinfection | Disinfection start hour |
| 1049 | Circle of disinfection | Disinfection interval |
| 1055 | EEV adjustment mode | Expansion valve control mode |
| 1056 | Target degree of supreheat | Expansion valve target superheat |
| 1057 | Original position of EEV | Expansion valve initial position |
| 1058 | Minimal opening position of EEV | Expansion valve minimum position |
| 1059 | Position of EEV for defrosting | Expansion valve defrost position |
| 1066 | Circle of submitting data to Cloud | Cloud reporting interval |
| 1067 | Remenber the status of device when power down | Restore state after power loss |
| 1069 | Heating source | Heat source type |
| 1073 | Temperature unit | Display temperature unit |
| 1074 | Model mode parameter | Control panel model code |
| 1075 | Compensate to the shown temp | Display temperature compensation |
| 1076 | Device address | Modbus address |
| 1077 | Intelligent control mode | Control interface |
| 1080 | The sensor to control solar water pump | Solar pump control sensor |
| 1081 | Longest running time of solar water pump | Solar pump maximum run time |
| 1082 | Temp hysteresis of solar water pump | Solar pump start temperature difference |
| 1083 | Activate the nighttime temp decreases mode | Night cooling enabled |
| 1084 | Startup point of the nighttime temp decreases mode | Night cooling start hour |
| 1085 | Shutdown point of the nighttime temp decreases mode | Night cooling stop hour |
| 1086 | Startup temp of decreasing solar water temp | Night cooling start temperature |
| 1087 | Temp hysteresis of stopping  decreasing solar water temp | Night cooling stop temperature difference |
| 1088 | Solar water releasing temp | Solar drain valve temperature threshold |
| 1089 | Shutdown temp of solar water pump | Solar pump stop temperature |
| 1090 | Working mode of solar water pump | Solar pump independent operation |
| 1104 | Target temp | Water target temperature |
| 1106 | Return difference of heat pump startup(bottom sensor) | Tank lower reheat differential |
| 1107 | Enable R05 as setpoint of booster? | Electric heater separate target enabled |
| 1108 | Setpoint of booster | Electric heater target temperature |
| 1109 | Booster startup delay | Electric heater start delay |
| 1110 | Booster replaces heat pump? | Electric heater takeover enabled |
| 1111 | Setpoint of ambient temp to activate booster to replace heat pump | Electric heater takeover ambient temperature |
| 1112 | Setpoint of ambient temp to activate booster without delay | Electric heater immediate start ambient temperature |
| 1113 | Setpoint of ambient temp to activate booster with delay | Electric heater delayed start ambient temperature |
| 1115 | Ambient temp of shutting down compressor compulsively | Compressor low ambient cutoff |
| 1117 | The target temp of second heating source | Auxiliary heat source target temperature |
| 1118 | Maximal ambient temp of working compressor | Compressor high temperature cutoff |
| 1120 | Enable top sensor to control compressor? | Tank upper sensor control enabled |
| 1121 | Return difference of heat pump startup(top sensor) | Tank upper reheat differential |
| 1122 | Setpoint 1 of ambient temp to stop compressor | Compressor high temperature cutoff 1 |
| 1123 | Setpoint 2 of ambient temp to stop compressor | Compressor high temperature cutoff 2 |
| 1129 | Enable vacation mode | Vacation date enabled |
| 1130 | Vacation-year | Vacation year |
| 1131 | Vacation-month | Vacation month |
| 1132 | Vacation-day | Vacation day |
| 1133 | Enable timer mode | Timer enable flags |
| 1134 | Hour setting of turning on timer 1 | Timer 1 start hour |
| 1135 | Minute setting of turning on timer 1 | Timer 1 start minute |
| 1136 | Hour setting of turning off timer 1 | Timer 1 stop hour |
| 1137 | Minute setting of turning off timer 1 | Timer 1 stop minute |
| 1138 | Hour setting of turning on timer 2 | Timer 2 start hour |
| 1139 | Minute setting of turning on timer 2 | Timer 2 start minute |
| 1140 | Hour setting of turning off timer 2 | Timer 2 stop hour |
| 1141 | Minute setting of turning off timer 2 | Timer 2 stop minute |
| 1151 | System Clock Modify Enable | Clock update flag |
| 1152 | System Current minute | Clock minute setting |
| 1153 | System current o'clock | Clock hour setting |
| 1154 | System current date | Clock day setting |
| 1155 | System current month | Clock month setting |
| 1156 | System current year | Clock year setting |
| 1158 | APP Online Heartbeat Package | Network module heartbeat |
| 2011 | Main program version | Main controller firmware version |
| 2012 | Software code | Main controller firmware code |
| 2013 | Controller program version | Control panel firmware version |
| 2014 | Software code | Control panel firmware code |
| 2019 | Ambient temperature | Ambient temperature |
| 2020 | bottom temperature | Tank temperature lower |
| 2021 | top temperature | Tank temperature upper |
| 2022 | coil temperature | Coil temperature |
| 2023 | suction temperature | Refrigerant suction temperature |
| 2024 | solar temperature | Solar sensor temperature |
| 2025 | APP/display interface temperature | Water temperature |
| 2026 | Enter parameter out of range protection times | Parameter out of range count |
| 2027 | Memory chip EEPROM number of times stored | EEPROM write count |
| 2050 | Slave component status | Component status flags |
| 2051 | Slave function status | Operating status flags |
| 2060 | EEV current position | Expansion valve position |
| 2061 | Accumulative running time of compressor | Compressor run time |
| 2062 | Accumulative running time of booster | Electric heater run time |
| 2085 | Failure（W10） | Fault flags |

#### Binary fields

| Register | Bit | Display name |
| --- | --- | --- |
| 1129 | 0 | Vacation date enabled |
| 1133 | 0 | Timer 1 start enabled |
| 1133 | 1 | Timer 1 stop enabled |
| 1133 | 2 | Timer 2 start enabled |
| 1133 | 3 | Timer 2 stop enabled |
| 2050 | 0 | Remote switch input open |
| 2050 | 2 | Low pressure switch input open |
| 2050 | 3 | High pressure switch input open |
| 2050 | 4 | Shortened timing input open |
| 2050 | 5 | External heat source input active |
| 2050 | 6 | Component reserved bit 6 |
| 2050 | 7 | Component reserved bit 7 |
| 2050 | 8 | Compressor running |
| 2050 | 9 | Electric heater running |
| 2050 | 10 | Reversing valve active |
| 2050 | 11 | Fan high speed active |
| 2050 | 12 | Output O05 active |
| 2050 | 13 | Output O06 active |
| 2050 | 14 | Electronic anode 3 V output active |
| 2050 | 15 | Electronic anode MV output active |
| 2051 | 0 | Thermostat satisfied |
| 2051 | 1 | Network module online |
| 2051 | 2 | Defrost active |
| 2051 | 3 | Hot water mode active |
| 2085 | 0 | Fault ambient temperature sensor |
| 2085 | 1 | Fault lower tank temperature sensor |
| 2085 | 2 | Fault upper tank temperature sensor |
| 2085 | 3 | Fault coil temperature sensor |
| 2085 | 4 | Fault suction temperature sensor |
| 2085 | 5 | Fault solar temperature sensor |
| 2085 | 8 | Protection high pressure |
| 2085 | 9 | Protection low pressure |
| 2085 | 13 | Protection tank frost |

### `hw211`

| Register | Workbook name | Display name |
| --- | --- | --- |
| 1011 | 开关机 | Power |
| 1012 | 模式 | Operating mode |
| 1014 | 电加热独立开启 | Electric heater independent operation |
| 1015 | 强制除霜 | Defrost forced operation |
| 1016 | 通风模式 | Ventilation mode |
| 1020 | O05的用途 | Output O05 function |
| 1021 | O06的用途 | Output O06 function |
| 1025 | 是否有环境温度补偿 | Ambient compensation enabled |
| 1026 | 温度补偿的最大偏移量 | Ambient compensation maximum offset |
| 1027 | 温度补偿的补偿系数 | Ambient compensation coefficient |
| 1028 | 开始补偿的环温 | Ambient compensation start temperature |
| 1034 | 制热进入除霜温度点 | Defrost start temperature |
| 1035 | 制热退出除霜温度点 | Defrost stop temperature |
| 1036 | 制热除霜周期 | Defrost interval |
| 1037 | 制热除霜最长时间 | Defrost maximum duration |
| 1038 | 经济除霜最短时间 | Defrost minimum duration |
| 1039 | 除霜方式 | Defrost mode |
| 1040 | 智能除霜温度转换点 | Defrost smart mode temperature threshold |
| 1046 | 高温消毒目标温度 | Disinfection target temperature |
| 1047 | 高温消毒维持时间 | Disinfection hold time |
| 1048 | 高温消毒的启动时间 | Disinfection start hour |
| 1049 | 高温消毒周期 | Disinfection interval |
| 1055 | 电子膨胀阀调节 | Expansion valve control mode |
| 1056 | 电子膨胀阀目标过热度 | Expansion valve target superheat |
| 1057 | 电子膨胀阀初开度 | Expansion valve initial position |
| 1058 | 电子膨胀阀最小开度 | Expansion valve minimum position |
| 1059 | 除霜开度 | Expansion valve defrost position |
| 1060 | 电子膨胀阀动作的低环温温度 | Expansion valve low ambient threshold |
| 1061 | 电子膨胀阀的低环温开度 | Expansion valve low ambient position |
| 1067 | 掉电记忆功能 | Restore state after power loss |
| 1068 | 是否启用冷气模式 | Cooling mode enabled |
| 1069 | 热源模式 | Heat source type |
| 1070 | 热源侧水泵提前压缩机运行时间 | Source pump compressor lead time |
| 1071 | 是否启用独立制冷 | Cooling independent operation enabled |
| 1072 | 独立冷气制冷维持时间 | Cooling independent operation duration |
| 1073 | 华氏度摄氏度转换 | Display temperature unit |
| 1074 | 线控器模式参数 | Control panel model code |
| 1075 | 主界面温度调整 | Display temperature compensation |
| 1080 | 使用何种感温头控制太阳能 | Solar pump control sensor |
| 1081 | 太阳能水泵最长运行时间 | Solar pump maximum run time |
| 1082 | 太阳能水泵启动回差 | Solar pump start temperature difference |
| 1083 | 夜间降温模式是否开启 | Night cooling enabled |
| 1084 | 降温功能的开启时间 | Night cooling start hour |
| 1085 | 降温功能的停止时间 | Night cooling stop hour |
| 1086 | 夜间降温的开启温度 | Night cooling start temperature |
| 1087 | 夜间降温的停止温差 | Night cooling stop temperature difference |
| 1088 | 太阳能排水阀温度设定点 | Solar drain valve temperature threshold |
| 1089 | 太阳能水泵停止温度设定点 | Solar pump stop temperature |
| 1090 | 太阳能是否独立运行 | Solar pump independent operation |
| 1104 | 热水设定温度 | Water target temperature |
| 1105 | 冷气设定温度 | Cooling target temperature |
| 1106 | 制热时下部回差温度设定 | Tank lower reheat differential |
| 1107 | 是否启用电加热设定温度 | Electric heater separate target enabled |
| 1108 | 电加热设定温度 | Electric heater target temperature |
| 1109 | 电加热启动延时 | Electric heater start delay |
| 1110 | 电加热是否取代压缩机 | Electric heater takeover enabled |
| 1111 | 电加热取代压缩机的环境温度 | Electric heater takeover ambient temperature |
| 1112 | 电加热零延时启动的环境温度 | Electric heater immediate start ambient temperature |
| 1113 | 电加热延时启动的环境温度 | Electric heater delayed start ambient temperature |
| 1114 | 循环泵开启时间 | Circulation pump on time |
| 1115 | 压缩机强制停止温度 | Compressor low ambient cutoff |
| 1116 | 冷气模式转换温度 | Cooling mode changeover temperature |
| 1117 | 第二外部温度设定点 | Auxiliary heat source target temperature |
| 1118 | 压缩机高温停止温度 | Compressor high temperature cutoff |
| 1119 | 高低风切换环境温度 | Fan speed changeover ambient temperature |
| 1120 | 压机开启是否启用上部温度 | Tank upper sensor control enabled |
| 1121 | 制热时上部回差温度设定 | Tank upper reheat differential |
| 1122 | 压缩机高温停止温度1 | Compressor high temperature cutoff 1 |
| 1123 | 压缩机高温停止温度2 | Compressor high temperature cutoff 2 |
| 1129 | 假期模式使能 | Vacation date enabled |
| 1130 | 假期模式年份位 | Vacation year |
| 1131 | 假期模式月份位 | Vacation month |
| 1132 | 假期模式日份位 | Vacation day |
| 1133 | 定时开关机使能 | Timer enable flags |
| 1134 | 定时时段1开机小时位 | Timer 1 start hour |
| 1135 | 定时时段1开机分钟位 | Timer 1 start minute |
| 1136 | 定时时段1关机小时位 | Timer 1 stop hour |
| 1137 | 定时时段1关机分钟位 | Timer 1 stop minute |
| 1138 | 定时时段2开机小时位 | Timer 2 start hour |
| 1139 | 定时时段2开机分钟位 | Timer 2 start minute |
| 1140 | 定时时段2关机小时位 | Timer 2 stop hour |
| 1141 | 定时时段2关机分钟位 | Timer 2 stop minute |
| 1149 | 手动功能 | Manual output flags |
| 1150 | 时钟修改使能 | Clock update flag |
| 1151 | 当前时 | Clock hour setting |
| 1152 | 当前分 | Clock minute setting |
| 1153 | 当前月 | Clock month setting |
| 1154 | 当前日 | Clock day setting |
| 1155 | 当前星期 | Clock weekday setting |
| 1156 | 当前年 | Clock year setting |
| 1157 | 机组地址 | Modbus address |
| 1158 | 线上智能控制 | Control interface |
| 2011 | 环境温度 AmbieNt temperature | Ambient temperature |
| 2012 | 水箱下部温度 bottom temperature | Tank temperature lower |
| 2013 | 水箱上部温度top temperature | Tank temperature upper |
| 2014 | 盘管温度 coil temperature | Coil temperature |
| 2015 | 回气温度 suctioN temperature | Refrigerant suction temperature |
| 2016 | 太阳能温度 solar temperature | Solar sensor temperature |
| 2019 | 电子膨胀阀当前开度 EEV positioN | Expansion valve position |
| 2020 | 压机运行累积时间 | Compressor run time |
| 2021 | 电加热运行累积时间 | Electric heater run time |
| 2030 | 从机部件状态 | Component status flags |
| 2031 | 从机功能状态 | Operating status flags |
| 2060 | 故障页：Failure（W10） | Fault flags |
| 2077 | 主程序版本号 | Main controller firmware version |
| 2078 | 软件代码 | Main controller firmware code |
| 2079 | 主程序版本号 | Secondary controller firmware version |
| 2080 | 软件代码 | Secondary controller firmware code |

#### Binary fields

| Register | Bit | Display name |
| --- | --- | --- |
| 1129 | 0 | Vacation date enabled |
| 1133 | 0 | Timer 1 start enabled |
| 1133 | 1 | Timer 1 stop enabled |
| 1133 | 2 | Timer 2 start enabled |
| 1133 | 3 | Timer 2 stop enabled |
| 1149 | 0 | Manual output O01 active |
| 1149 | 1 | Manual output O02 active |
| 1149 | 2 | Manual output O03 active |
| 1149 | 3 | Manual output O04 active |
| 1149 | 4 | Manual output O05 active |
| 1149 | 5 | Manual output O06 active |
| 1149 | 6 | Manual output O07 active |
| 1149 | 7 | Manual output O08 active |
| 2030 | 0 | Remote switch input open |
| 2030 | 1 | Electric heater overload input closed |
| 2030 | 2 | Low pressure switch input open |
| 2030 | 3 | High pressure switch input open |
| 2030 | 4 | Shortened timing input open |
| 2030 | 5 | Water flow input stopped |
| 2030 | 6 | Component reserved bit 6 |
| 2030 | 7 | Component reserved bit 7 |
| 2030 | 8 | Compressor running |
| 2030 | 9 | Electric heater running |
| 2030 | 10 | Reversing valve active |
| 2030 | 11 | Fan high speed active |
| 2030 | 12 | Output O05 active |
| 2030 | 13 | Output O06 active |
| 2031 | 0 | Thermostat satisfied |
| 2031 | 1 | Network module control selected |
| 2031 | 2 | Defrost active |
| 2060 | 0 | Fault ambient temperature sensor |
| 2060 | 1 | Fault lower tank temperature sensor |
| 2060 | 2 | Fault upper tank temperature sensor |
| 2060 | 3 | Fault coil temperature sensor |
| 2060 | 4 | Fault suction temperature sensor |
| 2060 | 5 | Fault solar temperature sensor |
| 2060 | 8 | Protection high pressure |
| 2060 | 9 | Protection low pressure |
| 2060 | 10 | Protection electric heater overheat |
| 2060 | 11 | Protection water flow |
| 2060 | 12 | Protection frost |
| 2060 | 13 | Protection tank frost |
| 2060 | 14 | Protection second stage frost |
