## Compilación
```bash
gcc medidor_vibracion.c -o medidor_vibracion -lpigpio -lpthread -lrt
```

## Como ejecutarlo
### Sin servo
```bash
sudo ./medidor_vibracion | python3 plotter_3ejes.py
```

### Con servo
```bash
sudo ./medidor_vibracion --servo | python3 plotter_3ejes.py
```