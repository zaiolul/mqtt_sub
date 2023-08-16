# mqtt_sub

MQTT subscriber program package. Uses libmosquitto, libsqlite3 and libcurl. Can subscribe to multiple topics. Can add events to topics which trigger when specified conditions are met.
Sends email to specified address when an event is triggered. Logs all received topic messages.

Events and topics are described in `mqtt_sub.config` file. UCI is used to fetch data from the file. Example file:

```
config 'topic'
  option 'name' 'hello/test'
  option 'qos' '1'

config event`
  option topic 'hello/test'
  option key 'temp'
  option type '1'
  option comparison '2'
  option value '11'
  option sender '<email>'
  list receivers '<email>'
```

`mqtt_messages.lua` is a simple command line script to get all messages of a given topic from the database file.
