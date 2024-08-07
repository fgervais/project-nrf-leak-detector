# Project Management

## Init

```bash
mkdir project-nrf-leak-detector
cd project-nrf-leak-detector
docker run --rm -u $(id -u):$(id -g) -v $(pwd):/new -w /new zephyrprojectrtos/ci \
        bash -c "west init -m https://github.com/fgervais/project-nrf-leak-detector.git . && west update"
```

## Build

```bash
cd application
docker compose run --rm nrf west build -b leak_detector -s app
```

## menuconfig

```bash
cd application
docker compose run --rm nrf west build -b leak_detector -s app -t menuconfig
```

## Clean

```bash
cd application
rm -rf build/
```

## Update

```bash
cd application
docker compose run --rm nrf west update
```

## Flash

### nrfjprog
```bash
cd application
docker compose -f docker-compose.yml -f docker-compose.device.yml \
        run --rm nrf west flash
```

### pyocd
```bash
cd application
pyocd flash -e sector -t nrf52840 -f 4000000 build/zephyr/zephyr.hex
```

# Hardware

https://github.com/fgervais/project-nrf-leak-detector_hardware
