# MQTT-Weather-Cloud
## Passing WiFi and MQTT credentials to the Cloud
Network configuration is don via the config.txt file that will need to be saved onto your microSD card. This will be loaded on startup and used to connect to your WiFi Network and MQTT server.

## Use the following feeds and JSON formatting ##

### Sending user controll data ###

#### Feed ####
**cloud/set**

#### Payload ####
```
  {
    "brightness": 120,
    "color": {
      "r": 255,
      "g": 100,
      "b": 100
    },
    "flash": 2,
    "transition": 5,
    "state": "ON",
    "effect": "solid",
    "volume": 100,
    "fade": 3,
    "sound": "loop"
  }
```  
Volume Control:

***volume***: A number from 0-100 with zero being no sound and 100 being max volume.

***fade***: A number from 1-255 representing the number to subtract from the brightness every 10 seconds. This is for slow fade outs up to 45 minutes long.

***sound***: How is the sound handled. Options are 1, 5, loop, or none.


### Sending weather information data ###

**Feed**: cloud/data

**Payload**:
```
  {
    "precip": 10,
    "acc": 20,
    "cover": 5,
    "forcast": "sunny"
  }
```

**YAML file Exsample**:
```
light:
  - platform: mqtt
    schema: json
    name: "New Cloud"
    state_topic: "cloud"
    command_topic: "cloud/set"
    effect: true
    effect_list:
      - bpm
      - candy cane
      - confetti
      - dots
      - juggle
      - police
      - sinelon
      - twinkle
      - noise
      - sunrise
      - night
      - weather
      - sunny
      - overcast
      - rain
      - snow
      - wind
      - lightning
      - rainbow
      - rainbow with glitter 
      - solid
    brightness: true
    rgb: true
    optimistic: false
    qos: 0

input_number:
  cloud_animation_speed:
    name: Cloud Speed
    initial: 150
    min: 1
    max: 150
    step: 10
  cloud_volume:
    name: Cloud Volume
    initial: 100
    min: 0
    max: 100
    step: 10

weather:
  - platform: darksky
    api_key: #YourAPIkey
    mode: daily

sensor:
  - platform: darksky
    api_key: #YourAPIkey
    monitored_conditions:
      - precip_intensity
      - precip_accumulation
      - cloud_cover
```

**Automation for updating the weather data feed**
```
- id: '1570982839163'
  alias: Weather Update MQTT
  trigger:
  - entity_id: weather.dark_sky
    platform: state
  - entity_id: sensor.dark_sky_cloud_coverage
    platform: state
  - entity_id: sensor.dark_sky_precip_intensity
    platform: state
  condition: []
  action:
  - service: mqtt.publish
    data_template:
      payload_template: "{\"precip\":\"{{ states('sensor.dark_sky_precip_intensity') }}\", \"cover\":\"{{ states('sensor.dark_sky_cloud_coverage') }}\", \"forcast\":\"{{ states('weather.dark_sky') }}\"}"
      topic: weatherdata
```

**Automation for Volume and Animation speed slider
```
- id: '1571439534847'
  alias: Cloud Speed
  trigger:
  - entity_id: input_number.cloud_speed
    platform: state
  condition: []
  action:
  - data_template:
      topic: cloud
      payload_template: '{"transition": {{ states(''input_number.cloud_speed'')
        }} }'
    service: mqtt.publish
- id: '1571439606917'
  alias: Cloud Volume
  trigger:
  - entity_id: input_number.cloud_volume
    platform: state
  condition: []
  action:
  - data_template:
      topic: cloud/set
      payload_template: '{"volume": {{ states(''input_number.cloud_volume'') }} }'
    service: mqtt.publish
```
