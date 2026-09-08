# 4-button-zigbee-remote-xiao-esp32c6
Battery-powered Zigbee 3.0 end device for Home Assistant (ZHA).
Deep Sleep with wake on any GPIO to low or with motion.

Each button has 3 actions:
Single Press (reads as a toggle)
Double Press (reads as "ON")
Long Press - (2-way dimmer/brighter on a light or light group - hold to dim, release, hold to brighten)

Use Automations to set each button behavior. *See Below. 

Example button config:
Single Press: toggles on/off Kitchen Lights
Double Press: toggles a switch, automation, or scene
Long press: dims kitchen lights; release; long press: increase brightness on kitchen lights

Arduino IDE Tools Settings:
Board: "XIAO_ESP32C6"
Erase All Flash Before Sketch Upload: "Enabled"
Partition Scheme: "Zigbee 4MB with spiffs"
Zigbee Mode: "Zigbee ED (end device)"

-----------------------------------------------

*Capture ZHA events (MAC / IEEE for automations)
The remote does not expose a separate entity for every gesture. Home Assistant sees zha_event. You need the device IEEE (MAC) and endpoint from that event.

1. Listen
   A. Home Assistant → Developer tools → Events
   B. In Listen to events, type: zha_event
   C. Click Start listening
   D. Press each button on the remote (single, double, triple, hold)

2. Read the payload
   A.
   short press looks like this:
    event_type: zha_event
    data:
      device_ieee: xx:xx:xx:xx:xx:xx:xx:xx
      device_id: XXXXXXXXXXXXXXXXXXXXXXXXXX
      unique_id: xx:xx:xx:xx:xx:xx:xx:xx:1:0x0006
      endpoint_id: 1
      cluster_id: 6
      command: toggle
    
   B.
   dim step (hold) looks like this:
    event_type: zha_event
    data:
      device_ieee: xx:xx:xx:xx:xx:xx:xx:xx
      unique_id: xx:xx:xx:xx:xx:xx:xx:xx:1:0x0008
      endpoint_id: 1
      cluster_id: 8
      command: step
      params:
        step_mode: 0    # 0 = up, 1 = down
        step_size: 10
        transition_time: 2
    
      Copy:
      Field              What is is
      device_ieee        Radio MAC. Use this in automations. It is unique to that C6
      device_id          ZHA's internal id. Also works; changes if you delete and re-pair.
      endpoint_id        Button number in firmware (1-4).
      cluster_id         6 (0x0006) = on/off / toggle / on. 8 (0x0008) = level step (hold).
      command            toggle, on, off, step, attribute_updated
    
      attribute_updated is the cluster echoing state. Prefer toggle / on / step as triggers so you do not fire twice.

3. Map buttons to endpoints
     Match endpoint_id to the GPIO you wired:

     Button  GPIO  endpoint_id
     1       0      1
     2       1      2
     3       2      3
     4       4      4

     Confirm by pressing one button at a time while listening.

4. Use it in YAML for your automations

    triggers:
  - trigger: event
    event_type: zha_event
    event_data:
      device_ieee: xx:xx:xx:xx:xx:xx:xx:xx   # your IEEE
      endpoint_id: 1
      command: toggle

  Hold/dim:

    triggers:
  - trigger: event
    event_type: zha_event
    event_data:
      device_ieee: 10:bd:a3:ff:fe:a0:54:74
      endpoint_id: 1
      command: step
      params:
        step_mode: 0   # 0 up, 1 down

    After a factory reset or a new board, listen again. device_ieee changes per chip. device_id changes if you remove the device from ZHA and pair it again. (Recommend using device_ieee for all automations)

5.  If nothing appears
  • ZHA → Add device is not required once it is already paired.
  • Restart Home Assistant if a previous interview got stuck.
  • Serial on the C6 should print the press. If Serial is silent, the event will not fire either.

