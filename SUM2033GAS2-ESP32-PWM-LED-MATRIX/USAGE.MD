before start, you have to check ip : 
```bash
ip a
```

to insert into esp32 : 
```bash
pio run -t upload
```

to observe the serial output : 
```bash
pio device monitor -b 115200
```