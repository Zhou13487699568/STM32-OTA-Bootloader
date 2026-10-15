# -*- coding: utf-8 -*-
"""
虚拟 DTU：串口 <--> TCP 双向透传（代替 4G 模块/DTU 的角色）

适用场景：4G 模块还没到手，先用 PC 验证整条透传链路，
也就是未来 Air780 DTU 要走的那条路。

接线（以 bootloader 命令行 USART1 为例）：
    USB-TTL 的 RX  <-- STM32 PA9 (USART1_TX)
    USB-TTL 的 TX  --> STM32 PA10(USART1_RX)
    USB-TTL 的 GND --- STM32 GND（必须共地）

链路：
    网络调试助手(TCP Server) <---花生壳公网映射---> 本脚本(TCP Client) <---USB-TTL---> STM32

使用步骤：
    1. 网络调试助手开 TCP Server，监听例如 0.0.0.0:8899
    2. 花生壳建 TCP 映射：内网主机=本机IP，内网端口=8899，得到公网域名:端口
    3. 把下方配置区的串口号、波特率、花生壳公网地址改好
    4. 运行：python virtual_dtu.py
       （本机直连测试可跳过花生壳：TCP_HOST 填 127.0.0.1）

依赖：pip install pyserial
"""

import socket
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("缺少 pyserial，先执行: pip install pyserial")

# ================= 配置区（按实际修改） =================
SERIAL_PORT = "COM3"          # USB-TTL 串口号，设备管理器里查
BAUDRATE    = 921600          # 与 STM32 串口一致（bootloader USART1 = 921600）
TCP_HOST    = "xxx.oray.net"  # 花生壳映射的公网域名；本机直连测试填 127.0.0.1
TCP_PORT    = 12345           # 花生壳映射的公网端口
RECONNECT_S = 3               # TCP 断开后重连间隔（秒）
VERBOSE     = False           # True=打印每笔数据预览；False=只打印状态和流量
# ========================================================


def hex_preview(data, limit=24):
    """前 N 字节的十六进制预览，确认有数据在流动"""
    if not data:
        return ""
    txt = " ".join("%02X" % b for b in data[:limit])
    return txt + (" ..." if len(data) > limit else "")


def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=0.05)
    except Exception as e:
        sys.exit("[!] 打开串口失败: %s（检查串口号/是否被其他软件占用）" % e)
    print("[串口] %s @ %d 已打开" % (SERIAL_PORT, BAUDRATE))

    up_bytes = 0          # STM32 --> 网络 累计
    down_bytes = 0        # 网络 --> STM32 累计
    last_stat = time.time()

    while True:
        sock = None
        try:
            print("[TCP ] 连接 %s:%d ..." % (TCP_HOST, TCP_PORT))
            sock = socket.create_connection((TCP_HOST, TCP_PORT), timeout=8)
            sock.settimeout(0.05)
            print("[TCP ] 已连接，进入透传（Ctrl+C 退出）")

            while True:
                # ---- 串口 --> 网络 ----
                n = ser.in_waiting
                if n:
                    data = ser.read(n)
                    if data:
                        sock.sendall(data)
                        up_bytes += len(data)
                        if VERBOSE:
                            print("[STM32->网络] %d B  %s" % (len(data), hex_preview(data)))

                # ---- 网络 --> 串口 ----
                try:
                    data = sock.recv(4096)
                except socket.timeout:
                    data = None
                if data == b"":
                    raise ConnectionError("服务器断开连接")
                if data:
                    ser.write(data)
                    down_bytes += len(data)
                    if VERBOSE:
                        print("[网络->STM32] %d B  %s" % (len(data), hex_preview(data)))

                # ---- 周期流量统计 ----
                now = time.time()
                if not VERBOSE and (now - last_stat) >= 10:
                    print("[统计] 上行 %d B / 下行 %d B" % (up_bytes, down_bytes))
                    last_stat = now

        except KeyboardInterrupt:
            print("\n[退出] 累计: 上行 %d B / 下行 %d B" % (up_bytes, down_bytes))
            break
        except Exception as e:
            print("[!] %s，%d 秒后重连" % (e, RECONNECT_S))
            time.sleep(RECONNECT_S)
        finally:
            if sock:
                try:
                    sock.close()
                except Exception:
                    pass

    ser.close()


if __name__ == "__main__":
    main()
