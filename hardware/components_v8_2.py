"""
components_paymentbox_v8_2.py — EcoSynTech PaymentBox V8.2 PRODUCTION-READY

75mm × 110mm 2-layer PCB.
Major changes from V8.1:
- Pin 18650 + TP4056 charger + MT3608 boost converter
- P-FET reverse polarity protection
- SMBJ5.0CA TVS + PPTC fuse (surge protection)
- All 11 audit fixes applied
"""

KFP = "/usr/share/kicad/footprints"

COMPONENTS = {
    # ============================================================
    # Z1 — ESP32 module zone (y = 12 to 42)
    # ============================================================
    "U1": (f"{KFP}/RF_Module.pretty", "ESP32-WROOM-32", 35, 28, 0,
           "ESP32-WROOM-32", "C701343"),

    # ESP32 decoupling caps
    "C3": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 28, 22, 0,
           "100nF", "C49678"),
    "C4": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 37, 36, 0,
           "100nF", "C49678"),
    "C5": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 32, 22, 0,
           "10uF", "C15850"),

    # Pull-up R6 (EN) and R10 (GPIO0)
    "R6": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 22, 28, 90,
           "10k", "C17414"),
    "R10": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 48, 28, 90,
            "10k", "C17414"),

    # NEW V8.2: Strapping pull-down GPIO12 and GPIO15 (FIXES A2, A3)
    "R21": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 22, 32, 90,
            "10k", "C17414"),
    "R22": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 48, 32, 90,
            "10k", "C17414"),

    # Buttons
    "SW1": (f"{KFP}/Button_Switch_SMD.pretty", "SW_Push_1P1T_NO_6x6mm_H9.5mm",
            12, 40, 0, "RESET", "C318884"),
    "SW2": (f"{KFP}/Button_Switch_SMD.pretty", "SW_Push_1P1T_NO_6x6mm_H9.5mm",
            58, 40, 0, "BOOT", "C318884"),

    # ============================================================
    # Z2 — USB-C input + surge protection (y = 42 to 60)
    # ============================================================
    "J1": (f"{KFP}/Connector_USB.pretty", "USB_C_Receptacle_Amphenol_12401548E4-2A",
           10, 50, 90, "USB-C", "C168688"),

    # USB-C CC pull-downs
    "R11": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 3.5, 55, 90,
            "5.1k", "C23186"),
    "R12": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 3.5, 58, 90,
            "5.1k", "C23186"),

    # NEW V8.2: Protection chain USB-C → +5V_SAFE
    "F1": (f"{KFP}/Fuse.pretty", "Fuse_1812_4532Metric", 20, 48, 0,
           "PPTC_500mA", "C369317"),  # PPTC 0.5A SMD
    "D6": (f"{KFP}/Diode_SMD.pretty", "D_SMB", 20, 53, 0,
           "SMBJ5.0CA", "C8975"),  # Bidirectional TVS
    "Q3": (f"{KFP}/Package_TO_SOT_SMD.pretty", "SOT-23", 27, 50, 0,
           "AO3401", "C15127"),  # P-MOSFET reverse polarity
    "R23": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 30, 48, 0,
            "100k", "C17407"),  # Q3 gate to source pull
    "R24": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 30, 52, 0,
            "10k", "C17414"),  # Q3 gate to GND

    # Ferrite bead
    "FB1": (f"{KFP}/Inductor_SMD.pretty", "L_0805_2012Metric", 36, 50, 0,
            "FB_1k", "C1017"),

    # Bulk cap on +5V_SAFE
    "C1": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 40, 48, 0,
           "10uF", "C15850"),
    "C2": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 40, 52, 0,
           "100nF", "C49678"),

    # ============================================================
    # Z2A — Battery charging + Boost converter (y = 60 to 80) — NEW
    # ============================================================

    # TP4056 Li-ion charger
    "U6": (f"{KFP}/Package_SO.pretty", "SOIC-8_3.9x4.9mm_P1.27mm",
           14, 66, 0, "TP4056", "C16581"),
    "R13": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 20, 64, 0,
            "1.2k", "C17520"),  # I_charge = 1A
    "R15": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 20, 67, 0,
            "1k", "C17513"),
    "R16": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 20, 70, 0,
            "1k", "C17513"),
    "C9": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 10, 70, 0,
           "10uF", "C15850"),
    "C10": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 18, 70, 0,
            "10uF", "C15850"),

    # Charge status LEDs
    "LED4": (f"{KFP}/LED_SMD.pretty", "LED_0805_2012Metric", 25, 64, 0,
             "CHRG_RED", "C84256"),
    "LED5": (f"{KFP}/LED_SMD.pretty", "LED_0805_2012Metric", 25, 67, 0,
             "DONE_GRN", "C72043"),

    # JST PH 2-pin for battery (use 1x2 header as proxy footprint)
    "J5": (f"{KFP}/Connector_PinHeader_2.54mm.pretty",
           "PinHeader_1x02_P2.54mm_Vertical", 32, 67, 0, "BAT", "C124375"),

    # MT3608 boost converter
    "U7": (f"{KFP}/Package_TO_SOT_SMD.pretty", "SOT-23-6",
           40, 67, 0, "MT3608", "C47773"),
    "L1": (f"{KFP}/Inductor_SMD.pretty", "L_0805_2012Metric", 45, 64, 0,
           "4.7uH", "C407457"),  # 4.7uH 1.5A SMD inductor
    "D7": (f"{KFP}/Diode_SMD.pretty", "D_SMA", 45, 70, 0,
           "SS14", "C2480"),
    "R17": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 50, 65, 0,
            "22k", "C17557"),
    "R18": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 50, 68, 0,
            "3k", "C17561"),
    "C11": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 38, 70, 0,
            "22uF", "C45783"),
    "C12": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 50, 72, 0,
            "22uF", "C45783"),
    "C13_boost": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 53, 68, 0,
                  "100nF", "C49678"),

    # ADC battery sense divider
    "R19": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 56, 64, 90,
            "100k", "C17407"),
    "R20": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 56, 68, 90,
            "47k", "C17475"),

    # AMS1117-3.3V LDO (3V3 from 5V_SYS)
    "U2": (f"{KFP}/Package_TO_SOT_SMD.pretty", "SOT-223-3_TabPin2",
           62, 67, 0, "AMS1117-3.3", "C6186"),
    "C6": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 58, 72, 0,
           "22uF", "C45783"),
    "C7": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 66, 72, 0,
           "100nF", "C49678"),

    # ============================================================
    # Z3 — Watchdog peripherals (y = 80 to 95)
    # ============================================================
    "LED1": (f"{KFP}/LED_SMD.pretty", "LED_0805_2012Metric", 10, 84, 0,
             "RED", "C84256"),
    "LED2": (f"{KFP}/LED_SMD.pretty", "LED_0805_2012Metric", 10, 87, 0,
             "GREEN", "C72043"),
    "LED3": (f"{KFP}/LED_SMD.pretty", "LED_0805_2012Metric", 10, 90, 0,
             "BLUE", "C72041"),

    "R1": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 15, 84, 0,
           "220R", "C17557"),
    "R2": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 15, 87, 0,
           "220R", "C17557"),
    "R3": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 15, 90, 0,
           "220R", "C17557"),

    "BZ1": (f"{KFP}/Buzzer_Beeper.pretty", "Buzzer_CUI_CPT-9019S-SMT",
            28, 87, 0, "BUZZER_5V", "C95298"),

    "Q2": (f"{KFP}/Package_TO_SOT_SMD.pretty", "SOT-23",
           25, 93, 0, "MMBT3904", "C20526"),
    "R4": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 22, 93, 0,
           "1k", "C17513"),

    # I2C pull-ups
    "R8": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 38, 84, 0,
           "4.7k", "C17475"),
    "R9": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 38, 87, 0,
           "4.7k", "C17475"),

    # Headers
    "J2": (f"{KFP}/Connector_PinHeader_2.54mm.pretty",
           "PinHeader_1x04_P2.54mm_Vertical", 55, 84, 90, "UART", "C124378"),
    "J3": (f"{KFP}/Connector_PinHeader_2.54mm.pretty",
           "PinHeader_1x06_P2.54mm_Vertical", 60, 88, 90, "EXP", "C124380"),

    # ============================================================
    # Z4 — PaymentBox peripherals (DNP for E)
    # ============================================================
    "U4": (f"{KFP}/Package_SO.pretty", "SOIC-8_3.9x4.9mm_P1.27mm",
           12, 100, 0, "ATECC608B", "C313975"),
    "C8": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 18, 104, 0,
           "100nF", "C49678"),

    "U5": (f"{KFP}/Connector_Card.pretty",
           "microSD_HC_Hirose_DM3AT-SF-PEJM5", 32, 104, 0, "microSD", "C91145"),
    # NEW V8.2: SD bulk cap (FIX A5)
    "C13_SD": (f"{KFP}/Capacitor_SMD.pretty", "C_0805_2012Metric", 24, 100, 0,
               "10uF", "C15850"),

    "OPT1": (f"{KFP}/Package_DIP.pretty", "DIP-4_W7.62mm",
             50, 100, 0, "PC817", "C7591"),
    "R7": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 45, 98, 0,
           "1k", "C17513"),
    "R5": (f"{KFP}/Resistor_SMD.pretty", "R_0805_2012Metric", 50, 104, 0,
           "1k", "C17513"),
    "Q1": (f"{KFP}/Package_TO_SOT_SMD.pretty", "SOT-23",
           55, 100, 0, "MMBT3904", "C20526"),

    # NEW V8.2: Schottky SS14 thay 1N4148 (FIX A4)
    "D4": (f"{KFP}/Diode_SMD.pretty", "D_SMA",
           60, 100, 0, "SS14", "C2480"),

    "K1": (f"{KFP}/Relay_THT.pretty", "Relay_SPDT_Schrack-RT1-FormC_RM5mm",
           60, 105, 0, "SRD-05VDC", "C137113"),
    "J4": (f"{KFP}/TerminalBlock.pretty", "TerminalBlock_Altech_AK300-3_P5.00mm",
           68, 100, 90, "RELAY_OUT", "C8240"),

    # ============================================================
    # Mounting & fiducials (PCB 75x110)
    # ============================================================
    "MH1": (f"{KFP}/MountingHole.pretty", "MountingHole_3.2mm_M3_DIN965_Pad",
            4, 4, 0, "M3", ""),
    "MH2": (f"{KFP}/MountingHole.pretty", "MountingHole_3.2mm_M3_DIN965_Pad",
            71, 4, 0, "M3", ""),
    "MH3": (f"{KFP}/MountingHole.pretty", "MountingHole_3.2mm_M3_DIN965_Pad",
            4, 106, 0, "M3", ""),
    "MH4": (f"{KFP}/MountingHole.pretty", "MountingHole_3.2mm_M3_DIN965_Pad",
            71, 106, 0, "M3", ""),

    "FID1": (f"{KFP}/Fiducial.pretty", "Fiducial_1mm_Mask3mm", 4, 55, 0, "FID", ""),
    "FID2": (f"{KFP}/Fiducial.pretty", "Fiducial_1mm_Mask3mm", 71, 55, 0, "FID", ""),
    "FID3": (f"{KFP}/Fiducial.pretty", "Fiducial_1mm_Mask3mm", 37, 106, 0, "FID", ""),
}


if __name__ == "__main__":
    print(f"Total components V8.2: {len(COMPONENTS)}")
    z1 = sum(1 for v in COMPONENTS.values() if 12 <= v[3] < 42)
    z2 = sum(1 for v in COMPONENTS.values() if 42 <= v[3] < 60)
    z2a = sum(1 for v in COMPONENTS.values() if 60 <= v[3] < 80)
    z3 = sum(1 for v in COMPONENTS.values() if 80 <= v[3] < 95)
    z4 = sum(1 for v in COMPONENTS.values() if 95 <= v[3] < 110)
    mh = sum(1 for k in COMPONENTS if k.startswith("MH") or k.startswith("FID"))
    print(f"  Z1 (ESP32):       {z1}")
    print(f"  Z2 (USB+protect): {z2}")
    print(f"  Z2A (BAT+boost):  {z2a}")
    print(f"  Z3 (Watchdog):    {z3}")
    print(f"  Z4 (PayBox DNP):  {z4}")
    print(f"  Mount/Fid:        {mh}")
