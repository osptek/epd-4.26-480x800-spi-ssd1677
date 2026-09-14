<p align="left"><img alt="OSPTEK" src="./images/logo.png" width="200" /></p>

<h1 align="center">OSPTEK 4.26″ EPD 480×800（SSD1677 · SPI）</h1>

<p align="center"><b>电子纸 · SPI · SSD1677</b></p>

<p align="center"><a href="./README_EN.md">English</a> | 简体中文 · <a href="../../README.md">规格族索引</a></p>

<p align="center">
  <img alt="Size: 4.26 inch" src="https://img.shields.io/badge/Size-4.26%22-3498DB?style=flat-square" />
  <img alt="Resolution: 480x800" src="https://img.shields.io/badge/Resolution-480%C3%97800-8E44AD?style=flat-square" />
  <img alt="Interface: SPI" src="https://img.shields.io/badge/Interface-SPI-27AE60?style=flat-square" />
  <img alt="Driver: SSD1677" src="https://img.shields.io/badge/Driver-SSD1677-E7352C?style=flat-square" />
</p>

<p align="center"><img alt="OSPTEK 4.26 寸 480×800 电子纸 SPI 模组（SSD1677）宣传图" src="./images/product.png" width="640" /></p>

## 目录

- [产品简介](#产品简介)
- [规格参数](#规格参数)
- [示例工程](#示例工程)
- [仓库结构](#仓库结构)
- [相关资料](#相关资料)
- [购买链接](#购买链接)
- [技术支持](#技术支持)

---

## 产品简介

OSPTEK **4.26 寸 480×800 电子纸（EPD）** 是一款 **SPI** 接口黑白墨水屏模组，显示驱动为 **SSD1677**。适合低功耗标签、仪表与静态信息显示；示例演示黑白 / 4 灰阶图像与局部刷新。

规格标识（仓库名）：`epd-4.26-480x800-spi-ssd1677`

当前模组版本：**EPD0426A02**。电气与外形细节以 [`docs/EPD_0426_A02_f14fa2c9ce.pdf`](./docs/EPD_0426_A02_f14fa2c9ce.pdf) 为准。

## 规格参数

| 项目 | 规格 |
| ---- | ---- |
| 尺寸 | 4.26 英寸 |
| 类型 | 电子纸 / EPD（黑白） |
| 分辨率 | 480×800 |
| 接口 | SPI |
| 驱动 IC | SSD1677 |

> 完整外形尺寸、FPC 定义、供电与时序以产品规格书 / 驱动手册为准。

## 示例工程

| 说明 | 路径 |
| ---- | ---- |
| ESP32-S3 · SSD1677 SPI bring-up（黑白 / 4 灰阶 / 局部刷新） | [`examples/esp32s3-epd-4.26-480x800-spi-ssd1677-bringup/`](./examples/esp32s3-epd-4.26-480x800-spi-ssd1677-bringup/) |

## 仓库结构

```text
epd-4.26-480x800-spi-ssd1677/                                # 仓库根（导航见 ../../README.md）
└── versions/
    └── EPD0426A02/                                # 本料号完整资料
        ├── README.md
        ├── README_EN.md
        ├── images/
        ├── docs/
        └── examples/
```

## 相关资料

### 本产品资料

| 资料 | 链接 |
| ---- | ---- |
| 产品规格书（EPD0426A02） | [`docs/EPD_0426_A02_f14fa2c9ce.pdf`](./docs/EPD_0426_A02_f14fa2c9ce.pdf) |
| 驱动 IC 数据手册（SSD1677） | [`docs/SSD_1677_447731c055.pdf`](./docs/SSD_1677_447731c055.pdf) |
| 4.26″ 墨水屏转接板 | [`docs/4.26寸墨水屏转接板.pdf`](./docs/4.26%E5%AF%B8%E5%A2%A8%E6%B0%B4%E5%B1%8F%E8%BD%AC%E6%8E%A5%E6%9D%BF.pdf) |
| 4 灰阶波表 | [`docs/4灰波表.txt`](./docs/4%E7%81%B0%E6%B3%A2%E8%A1%A8.txt) |

### 示例工程

- [ESP32-S3 SSD1677 SPI bring-up](./examples/esp32s3-epd-4.26-480x800-spi-ssd1677-bringup/)

## 购买链接

<p align="center">
  <a href="https://shop110742373.taobao.com/"><img alt="淘宝官方店铺" src="https://img.shields.io/badge/淘宝-官方店铺-FF6A00?style=for-the-badge" /></a>
  &nbsp;&nbsp;
  <a href="https://www.aliexpress.com/store/1105701619"><img alt="速卖通官方店铺" src="https://img.shields.io/badge/速卖通-官方店铺-FF6A00?style=for-the-badge" /></a>
</p>

**国内（淘宝）**

- 店铺：[鱼鹰光电工厂店](https://shop110742373.taobao.com/)

**海外（AliExpress）**

- 店铺：[OSPTEK Official Store](https://www.aliexpress.com/store/1105701619)

## 技术支持

- 技术支持 / 产品咨询：<luyu@osptek.com>
- QQ 技术交流群：**985881096**
- 公司官网：<https://osptek.com/>
- 有任何问题，都可以在本仓库 Issues 中提问

---

<p align="center"><sub>© 2026 OSPTEK 鱼鹰光电 · 本仓库资料采用 CC BY 4.0 许可</sub></p>
