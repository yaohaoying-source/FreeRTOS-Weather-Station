# 硬件设计资料

本目录保存气象站的本地嘉立创 EDA 工程、原理图、PCB 布局和物料清单。

| 文件 | 说明 |
| --- | --- |
| [weather-station.epro](weather-station.epro) | 本地原硬件工程，可在嘉立创 EDA 专业版导入 |
| [schematic.pdf](schematic.pdf) | 原工程原理图 PDF |
| [pcb-layout.pdf](pcb-layout.pdf) | 原工程 PCB 图 PDF |
| [BOM.md](BOM.md) | 整理后的板级器件、整机模块、电源与配件清单 |
| [pcb-bom.xlsx](pcb-bom.xlsx) | EDA PCB 页面导出的原始板级 BOM |

原始 BOM 的 22 个板级位号、数量、数值、封装、制造商及供应商字段已与本地 `.epro` 的 PCB 数据逐项对照。原始表格只覆盖板上器件；开发板、传感器、电源、天线及其他配件列于 `BOM.md` 的整机模块与配件部分。

**采购前待核对：** R1、R3、R4 的工程标称值为 150Ω，但关联的制造商型号对应工程库中的 10Ω 器件，与 R2 相同。当前保留原始数据，不代替作者确认实际选型。C2、C3 等数值也是工程标注，并未测量实物。

本次按作者要求保留 `.epro`，未提供 Gerber；需要制板时可在嘉立创 EDA 中打开该版本并重新导出，先核对原理图、PCB、物料和设计规则。
