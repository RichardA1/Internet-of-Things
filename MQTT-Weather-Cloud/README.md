# MQTT-Weather-Cloud

Use the following feeds and JSON formatting:

Sending user controll data:

**Feed**: cloud/set

**Payload**:
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
    "effect": "solid"
  }
```  
Sending weather information data:

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
      - weather
      - sunny
      - overcast
      - rain
      - snow
      - lightning
      - rainbow
      - rainbow with glitter 
    brightness: true
    rgb: true
    optimistic: false
    qos: 0
```
