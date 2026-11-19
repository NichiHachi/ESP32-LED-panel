launch backend :
```bash
cd back
cargo run
```

launch esp32 :
```bash
cd SUM2033GAS2-ESP32-PWM-LED-MATRIX
pio run -t upload
```

monitor serial output :
```bash
pio device monitor -b 115200
```

send test message to esp32 :
```bash
curl -X POST -F "image=@t.png" http://192.168.1.97:18080/esp
```