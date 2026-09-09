# 4-button Zigbee remote (XIAO ESP32-C6)

Battery-powered Zigbee 3.0 **end device** for Home Assistant **ZHA**.

Wake from deep sleep on any button (GPIO low) or ADXL345 motion (INT1 on GPIO5).
Idle timeout is 15 seconds.

## Gestures

Every physical button (endpoints 1–4):

1. Single press → Zigbee `toggle` (cluster 0x0006)
2. Double press → Zigbee `on` (cluster 0x0006)
3. Long press → level `step` every 250 ms (cluster 0x0008). Direction flips each hold (dim / brighten).

Motion / tap (endpoint 5, manufacturer model `Motion`):

- ADXL activity or single tap → `toggle` on endpoint 5

Map those events in Home Assistant automations. See below.

### Wiring: [wiring-instructions.md](wiring-instructions.md)

## Arduino IDE

1. Board: **XIAO_ESP32C6**
2. Partition Scheme: **Zigbee 4MB with spiffs**
3. Zigbee Mode: **Zigbee ED (end device)**
4. Erase All Flash: **Enabled only for first pair / factory reset.** Leave Disabled after it has joined or every upload wipes the network key.

If the board will not enter download mode, GPIO5 (MTDI) may be held by ADXL INT1. Hold the BOOT pad to GND while plugging USB.

Each button has 3 actions:

      Single Press (reads as a toggle)
      Double Press (reads as "ON")
      Long Press - (2-way dimmer/brighter on a light or light group - hold to dim, release, hold to brighten)



Use Automations to set each button behavior. *See Below. 


Example button config:

      Single Press: toggles on/off Kitchen Lights
      Double Press: toggles a switch, automation, or scene
      Long press: dims kitchen lights; release; long press: increase brightness on kitchen lights


-----------------------------------------------

## *Capture ZHA events (MAC / IEEE for automations)

The remote does not expose a separate entity for every gesture. Home Assistant sees zha_event. You need the device IEEE (MAC) and endpoint from that event.

### 1. Listen

   Home Assistant → Developer tools → Events
   
   In Listen to events, type:

         zha_event

   Click Start listening
   
   Press each button on the remote (single, double, hold)

### 2. Read the payload

   short press looks like this:

         event_type: zha_event
          data:
            device_ieee: xx:xx:xx:xx:xx:xx:xx:xx
            device_id: XXXXXXXXXXXXXXXXXXXXXXXXXX
            unique_id: xx:xx:xx:xx:xx:xx:xx:xx:1:0x0006
            endpoint_id: 1
            cluster_id: 6
            command: toggle
    
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
   
   motion example:

         triggers:
        - trigger: event
          event_type: zha_event
          event_data:
            device_ieee: xx:xx:xx:xx:xx:xx:xx:xx
            endpoint_id: 5
            command: toggle
    
   Copy:

         Field              What is is
         device_ieee        Radio MAC. Use this in automations. It is unique to that C6
         device_id          ZHA's internal id. Also works; changes if you delete and re-pair.
         endpoint_id        Button number in firmware (1-4).
         cluster_id         6 (0x0006) = on/off / toggle / on. 8 (0x0008) = level step (hold).
         command            toggle, on, off, step, attribute_updated
 
   attribute_updated is the cluster echoing state. Prefer toggle / on / step as triggers so you do not fire twice.

### 3. Map buttons to endpoints

   Match endpoint_id to the GPIO you wired:

        Button / source   GPIO    endpoint_id
         1                 0       1
         2                 1       2
         3                 2       3
         4                 4       4
         ADXL INT1         5       5

   Confirm by pressing one button at a time while listening.

### 4. Use it in YAML for your automations

   Single press

         triggers:
           - trigger: event
             event_type: zha_event
             event_data:
               device_ieee: 10:bd:a3:ff:fe:a0:54:74   # your IEEE
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

### 5.  If nothing appears
   
   •   Restart Home Assistant if a previous interview got stuck.
   •   Serial on the C6 should print the button press. If Serial is silent, the event will not fire either.

