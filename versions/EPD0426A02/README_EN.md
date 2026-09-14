<p align="left"><img alt="OSPTEK" src="./images/logo.png" width="200" /></p>

<h1 align="center">OSPTEK 4.26″ EPD 480×800 (SSD1677 · SPI)</h1>

<p align="center"><b>E-paper · SPI · SSD1677</b></p>

<p align="center"><a href="./README.md">简体中文</a> | English · <a href="../../README_EN.md">Family index</a></p>

<p align="center">
  <img alt="Size: 4.26 inch" src="https://img.shields.io/badge/Size-4.26%22-3498DB?style=flat-square" />
  <img alt="Resolution: 480x800" src="https://img.shields.io/badge/Resolution-480%C3%97800-8E44AD?style=flat-square" />
  <img alt="Interface: SPI" src="https://img.shields.io/badge/Interface-SPI-27AE60?style=flat-square" />
  <img alt="Driver: SSD1677" src="https://img.shields.io/badge/Driver-SSD1677-E7352C?style=flat-square" />
</p>

<p align="center"><img alt="OSPTEK 4.26″ 480×800 e-paper SPI module (SSD1677) product image" src="./images/product.png" width="640" /></p>

## Contents

- [Overview](#overview)
- [Specifications](#specifications)
- [Sample projects](#sample-projects)
- [Repository layout](#repository-layout)
- [Resources](#resources)
- [Buy](#buy)
- [Support](#support)

---

## Overview

OSPTEK **4.26″ 480×800 e-paper (EPD)** is a **SPI** monochrome electronic paper module driven by **SSD1677**. Suited to low-power labels, instruments, and static information displays. The sample covers black/white and 4-gray images plus partial refresh.

Spec ID (repository name): `epd-4.26-480x800-spi-ssd1677`

Current module version: **EPD0426A02**. Electrical and mechanical details follow [`docs/EPD_0426_A02_f14fa2c9ce.pdf`](./docs/EPD_0426_A02_f14fa2c9ce.pdf).

## Specifications

| Item | Spec |
| ---- | ---- |
| Size | 4.26 inch |
| Type | E-paper / EPD (monochrome) |
| Resolution | 480×800 |
| Interface | SPI |
| Driver IC | SSD1677 |

> Full outline, FPC definition, power, and timing follow the product datasheet / driver IC datasheet.

## Sample projects

| Description | Path |
| ---- | ---- |
| ESP32-S3 · SSD1677 SPI bring-up (BW / 4-gray / partial refresh) | [`examples/esp32s3-epd-4.26-480x800-spi-ssd1677-bringup/`](./examples/esp32s3-epd-4.26-480x800-spi-ssd1677-bringup/) |

## Repository layout

```text
epd-4.26-480x800-spi-ssd1677/                                # repo root (nav: ../../README_EN.md)
└── versions/
    └── EPD0426A02/                                # full materials for this part number
        ├── README.md
        ├── README_EN.md
        ├── images/
        ├── docs/
        └── examples/
```

## Resources

### Product files

| Resource | Link |
| ---- | ---- |
| Product datasheet (EPD0426A02) | [`docs/EPD_0426_A02_f14fa2c9ce.pdf`](./docs/EPD_0426_A02_f14fa2c9ce.pdf) |
| Driver IC datasheet (SSD1677) | [`docs/SSD_1677_447731c055.pdf`](./docs/SSD_1677_447731c055.pdf) |
| 4.26″ e-paper adapter board | [`docs/4.26寸墨水屏转接板.pdf`](./docs/4.26%E5%AF%B8%E5%A2%A8%E6%B0%B4%E5%B1%8F%E8%BD%AC%E6%8E%A5%E6%9D%BF.pdf) |
| 4-gray LUT | [`docs/4灰波表.txt`](./docs/4%E7%81%B0%E6%B3%A2%E8%A1%A8.txt) |

### Samples

- [ESP32-S3 SSD1677 SPI bring-up](./examples/esp32s3-epd-4.26-480x800-spi-ssd1677-bringup/)

## Buy

<p align="center">
  <a href="https://www.aliexpress.com/store/1105701619"><img alt="AliExpress store" src="https://img.shields.io/badge/AliExpress-Official_Store-FF6A00?style=for-the-badge" /></a>
  &nbsp;&nbsp;
  <a href="https://shop110742373.taobao.com/"><img alt="Taobao store" src="https://img.shields.io/badge/Taobao-Official_Store-FF6A00?style=for-the-badge" /></a>
</p>

**Overseas (AliExpress)**

- Store: [OSPTEK Official Store](https://www.aliexpress.com/store/1105701619)

**China (Taobao)**

- Store: [鱼鹰光电工厂店](https://shop110742373.taobao.com/)

## Support

- Technical support / product inquiry: <luyu@osptek.com>
- QQ group: **985881096**
- Website: <https://osptek.com/>
- Feel free to open an Issue in this repository with any questions

---

<p align="center"><sub>© 2026 OSPTEK · Materials in this repository are licensed under CC BY 4.0</sub></p>
