# MQTT-Weather-Cloud

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
    "sound": "1 min"
  }
```  
Volume Control:

***volume***: A number from 0-100 with zero being no sound and 100 being max volume.

***fade***: A number from 1-255 representing the number to subtract from the brightness every 10 seconds. This is for slow fade outs up to 45 minutes long.

***sound***: How is the sound handled. Options are 1 min, 5 mins, loop, or none.


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
